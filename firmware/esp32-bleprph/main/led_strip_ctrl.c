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
#include "led_animations.h"
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

/* How often the animation task checks whether the strip was switched back on
 * while it is powered off (nothing is drawn in that time). How fast frames
 * advance while on is set per animation (step_ms in led_animations.c). */
#define LED_STRIP_ANIMATION_IDLE_MS 100

typedef enum
{
    LED_MODE_SOLID,
    LED_MODE_ANIMATION,
} led_mode_t;

static led_strip_handle_t strip;

/* Guards every field below plus all access to the strip itself: both the
 * rainbow task and any BLE command can touch these at any time, and only
 * one of them may be mid-write to the strip at once. Recursive because
 * setters call apply_state() while already holding it. */
static SemaphoreHandle_t state_mutex;

static led_mode_t current_mode = LED_MODE_SOLID;

/* Index into led_animations[] of the animation that plays (or played last).
 * Only meaningful while current_mode is LED_MODE_ANIMATION. */
static size_t current_animation = 0;

/* True while an animation task exists. Set and cleared under state_mutex, so
 * a quick "set_color -> start_animation" can never end up with two animation
 * tasks (the old one only notices the mode change on its next frame). */
static bool animation_task_alive = false;

/* Master switch, see led_strip_ctrl_set_power(). */
static bool current_power = true;

/* Base color, independent of brightness and active LED count. */
static uint8_t current_red;
static uint8_t current_green;
static uint8_t current_blue;

/* 0..100, applied as a scale factor on top of the base color. */
static uint8_t current_brightness_percent = 100;

/* How many LEDs the physical strip has. Comes from the app after connecting,
 * until then a default is assumed. Never above LED_STRIP_MAX_LEDS. */
static uint16_t current_strip_length = LED_STRIP_DEFAULT_LED_COUNT;

/* How many LEDs (from index 0) are lit, the rest stays off. Defaults to
 * the whole strip. Never above current_strip_length. */
static uint16_t current_led_count = LED_STRIP_DEFAULT_LED_COUNT;

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

/* (hsv_to_rgb() moved to led_animations.c together with the rainbow, it is
 * only needed there.) */

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
    ESP_LOGI(TAG, "apply: r=%u g=%u b=%u (scaled) leds=%u brightness=%u%% power=%d",
             red, green, blue, current_led_count, current_brightness_percent,
             current_power);

    /* Always writes the whole buffer (not just the strip length): that way
     * LEDs that were lit before the strip length / LED count was reduced get
     * switched off again, and so does a physical strip that is longer than
     * the length the app told us about. */
    for (int i = 0; i < LED_STRIP_MAX_LEDS; i++)
    {
        bool lit = current_power && (uint16_t)i < current_led_count;

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
 * until something switches current_mode away from LED_MODE_ANIMATION (e.g.
 * led_strip_ctrl_set_color()), then deletes itself. Which animation it plays
 * is read from current_animation every frame, so switching from one
 * animation to another needs no new task. */
static void animation_task_fn(void *arg)
{
    (void)arg;
    uint8_t frame = 0;

    for (;;)
    {
        xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

        if (current_mode != LED_MODE_ANIMATION)
        {
            animation_task_alive = false;
            xSemaphoreGiveRecursive(state_mutex);
            break;
        }

        /* Powered off: led_strip_ctrl_set_power(false) already blanked the
         * strip, so there is nothing to draw. The task stays alive (instead
         * of deleting itself) so the animation just continues once the strip
         * is switched back on. */
        if (!current_power)
        {
            xSemaphoreGiveRecursive(state_mutex);
            vTaskDelay(pdMS_TO_TICKS(LED_STRIP_ANIMATION_IDLE_MS));
            continue;
        }

        const led_animation_t *animation = &led_animations[current_animation];
        uint16_t led_count = current_led_count;

        for (int i = 0; i < LED_STRIP_MAX_LEDS; i++)
        {
            bool lit = (uint16_t)i < led_count;
            uint8_t red = 0, green = 0, blue = 0;

            if (lit)
            {
                animation->pixel((uint16_t)i, led_count, frame, &red, &green, &blue);
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
        uint16_t step_ms = animation->step_ms;
        xSemaphoreGiveRecursive(state_mutex);

        frame++;
        vTaskDelay(pdMS_TO_TICKS(step_ms));
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
        .max_leds = LED_STRIP_MAX_LEDS,
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

    /* A solid color always wins over a running effect. The animation task
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

    /* Don't touch current_mode: if an animation is running, it picks up the
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

    current_led_count = count > current_strip_length ? current_strip_length : count;

    esp_err_t err = ESP_OK;
    if (current_mode == LED_MODE_SOLID)
    {
        err = apply_state();
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_set_strip_length(uint16_t length)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    if (length < 1)
    {
        length = 1;
    }
    current_strip_length = length > LED_STRIP_MAX_LEDS ? LED_STRIP_MAX_LEDS : length;

    if (current_led_count > current_strip_length)
    {
        current_led_count = current_strip_length;
    }

    /* A running animation picks up the new length / LED count on its next
     * frame, same as for the other setters. */
    esp_err_t err = ESP_OK;
    if (current_mode == LED_MODE_SOLID)
    {
        err = apply_state();
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_set_power(bool on)
{
    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    current_power = on;

    /* Solid color: nobody else redraws, so do it here. Animation: switching on
     * is picked up by the (idle) animation task by itself, but switching off
     * has to blank the strip right now, the task won't draw anymore. */
    esp_err_t err = ESP_OK;
    if (current_mode == LED_MODE_SOLID || !on)
    {
        err = apply_state();
    }

    xSemaphoreGiveRecursive(state_mutex);
    return err;
}

esp_err_t led_strip_ctrl_start_animation(size_t index)
{
    if (index >= led_animation_count)
    {
        ESP_LOGW(TAG, "no animation with index %u", (unsigned)index);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);

    /* Switching from one animation to another only changes the index, the
     * running task picks it up on its next frame. */
    current_animation = index;
    current_mode = LED_MODE_ANIMATION;
    bool must_create_task = !animation_task_alive;
    animation_task_alive = true;

    xSemaphoreGiveRecursive(state_mutex);

    if (!must_create_task)
    {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(animation_task_fn, "led_animation", 4096, NULL,
                                tskIDLE_PRIORITY + 1, NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "failed to create animation task");

        xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY);
        animation_task_alive = false;
        current_mode = LED_MODE_SOLID;
        xSemaphoreGiveRecursive(state_mutex);

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

uint16_t led_strip_ctrl_get_strip_length(void)
{
    return current_strip_length;
}

bool led_strip_ctrl_get_power(void)
{
    return current_power;
}

const char *led_strip_ctrl_get_effect_name(void)
{
    return current_mode == LED_MODE_ANIMATION ? led_animations[current_animation].name : "solid";
}