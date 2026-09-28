/*
 * Thin wrapper around the ESP-IDF "led_strip" component. Keeps every
 * strip-specific detail (GPIO pin, LED count, driver handle) in this one
 * file, so callers only ever deal with led_strip_ctrl_set_all().
 */

#ifndef H_LED_STRIP_CTRL_
#define H_LED_STRIP_CTRL_

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* GPIO the strip's data line is connected to (through a level shifter,
 * e.g. 74AHCT125, since WS2812B expects 5V logic and the ESP32 outputs
 * 3.3V). Adjust to match your wiring. */
#define LED_STRIP_GPIO_PIN 16

/* Total number of addressable LEDs on the strip. */
#define LED_STRIP_LED_COUNT 300

    /* Sets up the RMT peripheral and the led_strip driver, and clears the
     * strip. Call once, before the first led_strip_ctrl_set_all(). */
    esp_err_t led_strip_ctrl_init(void);

    /* Sets every LED on the strip to the same RGB color (0..255 each) and
     * pushes the update out immediately. */
    esp_err_t led_strip_ctrl_set_all(uint8_t red, uint8_t green, uint8_t blue);

    /* Returns the color that was last requested via led_strip_ctrl_set_all(),
     * so other code (e.g. the BLE "color" query endpoint) can report the
     * current state without keeping its own copy. */
    void led_strip_ctrl_get_last_color(uint8_t *red, uint8_t *green, uint8_t *blue);

#ifdef __cplusplus
}
#endif

#endif
