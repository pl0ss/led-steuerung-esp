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

#include <stdbool.h>
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

/* Hard upper limit of LEDs the firmware can drive. It sizes the driver's
 * pixel buffer at init, so it cannot change at runtime. The real length of
 * the strip is told to us by the app (led_strip_ctrl_set_strip_length()) and
 * is always clamped to this value, so asking for more LEDs than we have
 * buffer for is harmless. */
#define LED_STRIP_MAX_LEDS 300

/* Strip length used until the app has told us the real one (the app sends it
 * right after connecting). */
#define LED_STRIP_DEFAULT_LED_COUNT 100

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
     * turned off. Values above the strip length are clamped. Combined with
     * the current color and brightness, then pushed out immediately. Also
     * applies while a running effect (e.g. the rainbow) is active, without
     * interrupting it. */
    esp_err_t led_strip_ctrl_set_led_count(uint16_t count);

    /* Sets how many LEDs the physical strip really has (1..LED_STRIP_MAX_LEDS,
     * values outside that range are clamped). This is the upper bound for
     * led_strip_ctrl_set_led_count(); if the lit LED count is currently
     * higher, it is reduced to the new length. Everything behind the strip
     * length is driven dark. */
    esp_err_t led_strip_ctrl_set_strip_length(uint16_t length);

    /* Master switch. false blanks the strip, true shows it again exactly as
     * it was (same color / same running effect). Color, brightness and LED
     * count can still be changed while off, they just stay invisible until
     * the strip is switched on. */
    esp_err_t led_strip_ctrl_set_power(bool on);

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
    uint16_t led_strip_ctrl_get_strip_length(void);
    bool led_strip_ctrl_get_power(void);

    /* "solid" or "rainbow", for reporting the current effect over BLE. */
    const char *led_strip_ctrl_get_effect_name(void);

#ifdef __cplusplus
}
#endif

#endif