/* Connection LED, see status_led.h. */

#include "status_led.h"
#include "driver/gpio.h"

esp_err_t status_led_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
    {
        return err;
    }

    status_led_set(false);
    return ESP_OK;
}

void status_led_set(bool on)
{
    gpio_set_level(STATUS_LED_GPIO, on ? STATUS_LED_ON_LEVEL : !STATUS_LED_ON_LEVEL);
}
