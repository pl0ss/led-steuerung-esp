/*
 * Thin wrapper around the ESP-IDF "led_strip" component. Keeps every
 * strip-specific detail (GPIO pin, LED count, driver handle) in this one
 * file, so callers only ever deal with the setters below.
 *
 * The strip has no native brightness control, so "brightness" is applied
 * by scaling the base color before writing pixels. Color, brightness and
 * active LED count are tracked independently and re-combined whenever any
 * of them changes.
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
     * strip. Call once, before any of the setters below. */
    esp_err_t led_strip_ctrl_init(void);

    /* Sets the base color (0..255 each). Combined with the current brightness
     * and active LED count, then pushed out immediately. */
    esp_err_t led_strip_ctrl_set_color(uint8_t red, uint8_t green, uint8_t blue);

    /* Sets brightness as a percentage (0..100), values outside that range are
     * clamped. Combined with the current color and active LED count, then
     * pushed out immediately. */
    esp_err_t led_strip_ctrl_set_brightness(uint8_t percent);

    /* Sets how many LEDs (starting from index 0) should be lit, the rest is
     * turned off. Values above LED_STRIP_LED_COUNT are clamped. Combined with
     * the current color and brightness, then pushed out immediately. Also
     * applies while a running effect (e.g. the rainbow) is active, without
     * interrupting it. */
    esp_err_t led_strip_ctrl_set_led_count(uint16_t count);

    /* Starts an animated rainbow across the strip, running in its own
     * FreeRTOS task on the ESP32. It keeps running by itself, independent of
     * any BLE connection, until led_strip_ctrl_set_color() (or a future
     * effect) switches back to a solid color. Brightness and active LED count
     * still apply while it runs. Calling this again while it's already
     * running is a no-op. */
    esp_err_t led_strip_ctrl_start_rainbow(void);

    /* Reports the current state, so other code (e.g. the BLE query endpoints)
     * can answer without keeping its own copy. */
    void led_strip_ctrl_get_color(uint8_t *red, uint8_t *green, uint8_t *blue);
    uint8_t led_strip_ctrl_get_brightness(void);
    uint16_t led_strip_ctrl_get_led_count(void);

    /* "solid" or "rainbow", for reporting the current effect over BLE. */
    const char *led_strip_ctrl_get_effect_name(void);

#ifdef __cplusplus
}
#endif

#endif