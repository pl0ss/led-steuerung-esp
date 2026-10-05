/*
 * Thin wrapper around the ESP-IDF "led_strip" component (RMT-backed WS2812B
 * driver). Everything strip-specific (GPIO, LED count, driver handle) lives
 * here, ble_write_handler.c only calls the setters below and doesn't need
 * to know anything about RMT.
 *
 * Requires the "led_strip" component, add it once with:
 *   idf.py add-dependency "espressif/led_strip"
 */

#include <math.h>
#include "led_strip_ctrl.h"
#include "led_strip.h"
#include "led_strip_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "led_strip_ctrl";

/* Human brightness perception is roughly logarithmic (Weber-Fechner law),
 * not linear: dropping the raw PWM duty cycle from 100% to 50% looks
 * barely dimmer at all, almost all of the visible dimming happens only in
 * roughly the bottom third of a linear scale. Without correcting for
 * this, the brightness slider feels like it does nothing for most of its
 * travel and then gets dark (and, combined with 8-bit rounding,
 * color-shifted) only right at the low end. 2.2 is the typical gamma used
 * for LED/display brightness correction. */
#define LED_STRIP_BRIGHTNESS_GAMMA 2.2

/* How often the rainbow effect advances and redraws, in milliseconds. */
#define LED_STRIP_RAINBOW_STEP_MS 20

typedef enum
{
    LED_MODE_SOLID,
    LED_MODE_RAINBOW,
} led_mode_t;

static led_strip_handle_t strip;

/* Guards every field below plus all access to the strip itself: both the
 * rainbow task and any BLE command can touch these at any time, and only
 * one of them may be mid-write to the strip at once. Recursive because
 * setters call apply_state() while already holding it. */
static SemaphoreHandle_t state_mutex;

static led_mode_t current_mode = LED_MODE_SOLID;

/* Base color, independent of brightness and active LED count. */
static uint8_t current_red;
static uint8_t current_green;
static uint8_t current_blue;

/* 0..100, applied as a scale factor on top of the base color. */
static uint8_t current_brightness_percent = 100;

/* How many LEDs (from index 0) are lit, the rest stays off. Defaults to
 * the whole strip. */
static uint16_t current_led_count = LED_STRIP_LED_COUNT;

/* Maps a channel value (0..255) to its brightness-scaled value for the
 * current brightness. Rebuilt only when the brightness changes, so the
 * per-pixel path (up to 900 channel lookups per rainbow frame) is a plain
 * array read instead of a software-emulated double pow() each time. */
static uint8_t brightness_lut[256];

/* Caller must hold state_mutex (or be in init, before any task exists). */
static void rebuild_brightness_lut(void)
{
    double linear_fraction = (double)current_brightness_percent / 100.0;
    double perceptual_fraction = pow(linear_fraction, LED_STRIP_BRIGHTNESS_GAMMA);

    for (int value = 0; value < 256; value++)
    {
        if (current_brightness_percent == 0 || value == 0)
        {
            brightness_lut[value] = 0;
            continue;
        }

        uint8_t scaled = (uint8_t)((double)value * perceptual_fraction + 0.5);

        /* Only 0% itself should produce true black. Otherwise the gamma
         * curve can round a still-lit channel all the way down to 0 at low
         * percentages, which would look like a hue shift instead of a dim,
         * barely-there color. */
        brightness_lut[value] = scaled == 0 ? 1 : scaled;
    }
}

static uint8_t scale_channel(uint8_t value)
{
    return brightness_lut[value];
}

/* Standard integer HSV -> RGB conversion, h/s/v all 0..255. Used only by
 * the rainbow effect below. */
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0)
    {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (uint8_t)(((uint16_t)v * (255 - s)) >> 8);
    uint8_t q = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * remainder) >> 8))) >> 8);
    uint8_t t = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * (255 - remainder)) >> 8))) >> 8);

    switch (region)
    {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}

/* Recomputes every pixel from the current color/brightness/led_count and
 * pushes the result out. Called by every solid-color setter, so the strip
 * always reflects the latest combination of all three. Caller must hold
 * state_mutex. */
static esp_err_t apply_state(void)
{
    if (strip == NULL)
    {
        ESP_LOGE(TAG, "apply_state called before led_strip_ctrl_init");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t red = scale_channel(current_red);
    uint8_t green = scale_channel(current_green);
    uint8_t blue = scale_channel(current_blue);

    /* Exactly what is handed to the driver, so you can compare it with the
     * monitor output when the strip shows something unexpected: if these
     * values are right but the strip is wrong, the problem is on the way
     * to the strip (signal level, ground, power), not in this code. */
    ESP_LOGI(TAG, "apply: r=%u g=%u b=%u (scaled) leds=%u brightness=%u%%",
             red, green, blue, current_led_count, current_brightness_percent);

    for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
    {
        bool lit = (uint16_t)i < current_led_count;

        /* Plain (r, g, b) — led_model=LED_MODEL_WS2812 combined with
         * color_component_format=LED_STRIP_COLOR_COMPONENT_FMT_GRB already
         * makes the driver reorder onto the physical GRB wire correctly.
         * Verified against pure red/green/blue/yellow. */
        esp_err_t err = led_strip_set_pixel(strip, i,
                                            lit ? red : 0,
                                            lit ? green : 0,
                                            lit ? blue : 0);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "led_strip_set_pixel(%d) failed: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    return led_strip_refresh(strip);
}

/* Runs entirely on its own: it only reads shared state (brightness,
 * led_count, mode) through state_mutex each frame, so it neither knows nor
 * cares whether a phone is currently connected over BLE. It keeps going
 * until something switches current_mode away from LED_MODE_RAINBOW (e.g.
 * led_strip_ctrl_set_color()), then deletes itself. */
static void rainbow_task_fn(void *arg)
{
    (void)arg;
    uint8_t offset = 0;

    for (;;)
    {
        xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

        if (current_mode != LED_MODE_RAINBOW)
        {
            xSemaphoreGiveRecursive(state_mutex);
            break;
        }

        uint16_t led_count = current_led_count;
        uint16_t hue_span = led_count == 0 ? 1 : led_count;

        for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
        {
            bool lit = (uint16_t)i < led_count;
            uint8_t red = 0, green = 0, blue = 0;

            if (lit)
            {
                uint8_t hue = (uint8_t)(((i * 256 / hue_span) + offset) & 0xFF);
                hsv_to_rgb(hue, 255, 255, &red, &green, &blue);
                red = scale_channel(red);
                green = scale_channel(green);
                blue = scale_channel(blue);
            }

            esp_err_t err = led_strip_set_pixel(strip, i, red, green, blue);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "led_strip_set_pixel(%d) failed: %s", i, esp_err_to_name(err));
                break;
            }
        }

        led_strip_refresh(strip);
        xSemaphoreGiveRecursive(state_mutex);

        offset++;
        vTaskDelay(pdMS_TO_TICKS(LED_STRIP_RAINBOW_STEP_MS));
    }

    vTaskDelete(NULL);
}

esp_err_t led_strip_ctrl_init(void)
{
    state_mutex = xSemaphoreCreateRecursiveMutex();
    if (state_mutex == NULL)
    {
        ESP_LOGE(TAG, "failed to create state_mutex");
        return ESP_ERR_NO_MEM;
    }

    rebuild_brightness_lut();

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    /* SPI backend with DMA instead of RMT. The RMT peripheral on the
     * classic ESP32 has no DMA: a 300 LED frame (900 bytes) is fed through
     * a tiny buffer that an interrupt keeps refilling. That interrupt runs
     * on the same core as the BLE stack, and whenever BLE delays a refill
     * the WS2812 bit timing breaks, which shows up as wrong colors (stuck
     * until the next refresh) or flicker in an animation. SPI + DMA streams
     * the whole frame from memory without the CPU, so BLE can't disturb it.
     * The data pin is driven as SPI MOSI through the GPIO matrix, so any
     * output-capable GPIO works. */
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags = {
            .with_dma = true,
        },
    };

    esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "led_strip_new_spi_device failed: %s", esp_err_to_name(err));
        return err;
    }

    return led_strip_clear(strip);
}

esp_err_t led_strip_ctrl_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    /* A solid color always wins over a running effect. The rainbow task
     * notices current_mode changed the next time it takes state_mutex
     * (at most one frame later) and deletes itself; no explicit signal or
     * wait is needed since state_mutex already keeps their strip writes
     * from ever overlapping. */
    current_mode = LED_MODE_SOLID;
    current_red = red;
    current_green = green;
    current_blue = blue;
    esp_err_t err = apply_state();

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_set_brightness(uint8_t percent)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    current_brightness_percent = percent > 100 ? 100 : percent;
    rebuild_brightness_lut();

    /* Don't touch current_mode: if the rainbow is running, it picks up the
     * new brightness on its own on the next frame. Only push an update
     * ourselves for a static color, which has no task doing that. */
    esp_err_t err = ESP_OK;
    if (current_mode == LED_MODE_SOLID)
    {
        err = apply_state();
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_set_led_count(uint16_t count)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    current_led_count = count > LED_STRIP_LED_COUNT ? LED_STRIP_LED_COUNT : count;

    esp_err_t err = ESP_OK;
    if (current_mode == LED_MODE_SOLID)
    {
        err = apply_state();
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_start_rainbow(void)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    bool already_running = (current_mode == LED_MODE_RAINBOW);
    current_mode = LED_MODE_RAINBOW;

    xSemaphoreGiveRecursive(state_mutex);

    if (already_running)
    {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(rainbow_task_fn, "led_rainbow", 4096, NULL,
                                tskIDLE_PRIORITY + 1, NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "failed to create rainbow task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void led_strip_ctrl_get_color(uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = current_red;
    *green = current_green;
    *blue = current_blue;
}

uint8_t led_strip_ctrl_get_brightness(void)
{
    return current_brightness_percent;
}

uint16_t led_strip_ctrl_get_led_count(void)
{
    return current_led_count;
}

const char *led_strip_ctrl_get_effect_name(void)
{
    return current_mode == LED_MODE_RAINBOW ? "rainbow" : "solid";
}