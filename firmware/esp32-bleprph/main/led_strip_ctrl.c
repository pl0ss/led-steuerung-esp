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
#include "esp_log.h"

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

static led_strip_handle_t strip;

/* Base color, independent of brightness and active LED count. */
static uint8_t current_red;
static uint8_t current_green;
static uint8_t current_blue;

/* 0..100, applied as a scale factor on top of the base color. */
static uint8_t current_brightness_percent = 100;

/* How many LEDs (from index 0) are lit, the rest stays off. Defaults to
 * the whole strip. */
static uint16_t current_led_count = LED_STRIP_LED_COUNT;

static uint8_t scale_channel(uint8_t value)
{
    if (current_brightness_percent == 0 || value == 0)
    {
        return 0;
    }

    double linear_fraction = (double)current_brightness_percent / 100.0;
    double perceptual_fraction = pow(linear_fraction, LED_STRIP_BRIGHTNESS_GAMMA);

    uint8_t scaled = (uint8_t)((double)value * perceptual_fraction + 0.5);

    /* Only 0% itself should produce true black. Otherwise the gamma curve
     * can round a still-lit channel all the way down to 0 at low
     * percentages, which would again look like a hue shift instead of a
     * dim, barely-there color. */
    if (scaled == 0)
    {
        scaled = 1;
    }

    return scaled;
}

/* Recomputes every pixel from the current color/brightness/led_count and
 * pushes the result out. Called by every setter, so the strip always
 * reflects the latest combination of all three. */
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

    for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
    {
        bool lit = (uint16_t)i < current_led_count;

        /* Plain (r, g, b) — led_model=LED_MODEL_WS2812 combined with
         * color_component_format=LED_STRIP_COLOR_COMPONENT_FMT_GRB already
         * makes the driver reorder onto the physical GRB wire correctly.
         * An earlier version of this file rotated these arguments to
         * compensate for a "wrong color" report that turned out to be a
         * misread test (the input/observed colors had been swapped when
         * describing the bug), so that rotation was compensating for a bug
         * that didn't exist, and produced this rotation itself. Verified
         * against pure red/green/blue/yellow: this direct order is
         * correct. */
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

esp_err_t led_strip_ctrl_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,
        .max_leds = LED_STRIP_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz, i.e. a 0.1us RMT tick */
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return err;
    }

    return led_strip_clear(strip);
}

esp_err_t led_strip_ctrl_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    current_red = red;
    current_green = green;
    current_blue = blue;
    return apply_state();
}

esp_err_t led_strip_ctrl_set_brightness(uint8_t percent)
{
    current_brightness_percent = percent > 100 ? 100 : percent;
    return apply_state();
}

esp_err_t led_strip_ctrl_set_led_count(uint16_t count)
{
    current_led_count = count > LED_STRIP_LED_COUNT ? LED_STRIP_LED_COUNT : count;
    return apply_state();
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