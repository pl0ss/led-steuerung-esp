/*
 * Thin wrapper around the ESP-IDF "led_strip" component (RMT-backed WS2812B
 * driver). Everything strip-specific (GPIO, LED count, driver handle) lives
 * here, ble_write_handler.c only calls led_strip_ctrl_set_all() and doesn't
 * need to know anything about RMT.
 *
 * Requires the "led_strip" component, add it once with:
 *   idf.py add-dependency "espressif/led_strip"
 */

#include "led_strip_ctrl.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "led_strip_ctrl";

static led_strip_handle_t strip;
static uint8_t last_red;
static uint8_t last_green;
static uint8_t last_blue;

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

esp_err_t led_strip_ctrl_set_all(uint8_t red, uint8_t green, uint8_t blue)
{
    if (strip == NULL)
    {
        ESP_LOGE(TAG, "led_strip_ctrl_set_all called before led_strip_ctrl_init");
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < LED_STRIP_LED_COUNT; i++)
    {
        esp_err_t err = led_strip_set_pixel(strip, i, red, green, blue);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "led_strip_set_pixel(%d) failed: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    esp_err_t err = led_strip_refresh(strip);
    if (err == ESP_OK)
    {
        last_red = red;
        last_green = green;
        last_blue = blue;
    }

    return err;
}

void led_strip_ctrl_get_last_color(uint8_t *red, uint8_t *green, uint8_t *blue)
{
    *red = last_red;
    *green = last_green;
    *blue = last_blue;
}