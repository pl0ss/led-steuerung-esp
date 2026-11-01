/*
 * Hardware buttons to use the strip without the app. Two ways to use them:
 *
 * Two buttons (HW_BUTTON_LEFT_GPIO >= 0):
 *   left button  = previous entry (index - 1)
 *   right button = next entry     (index + 1)
 *
 * One button (HW_BUTTON_LEFT_GPIO = -1, the default, e.g. only the BOOT
 * button of a dev board):
 *   short press  = next entry     (index + 1)
 *   long press   = previous entry (index - 1), fires as soon as the press
 *                  has lasted HW_BUTTON_LONG_PRESS_MS, no need to let go
 *
 * They walk through one list (wrapping around at both ends):
 *
 *   [ orange, purple, yellow, blue, green, red, off, <every animation from
 *     led_animations[]> ]
 *
 * The index starts at "off". The colors are the array in hw_buttons.c, the
 * animations are appended automatically from led_animations.c, so a new
 * animation shows up on the buttons without touching this module.
 *
 * Wiring of external buttons: each between its GPIO and GND, nothing else
 * (the internal pull-up is used). The BOOT button of an ESP32 dev board is
 * already wired like that to GPIO 0. It is a strapping pin, so don't hold it
 * while the board powers up or resets (that starts the flash mode), pressing
 * it while the firmware runs is fine. If yours pull to 3.3V instead, set
 * HW_BUTTON_ACTIVE_LEVEL to 1 and the internal pull-down is used.
 */

#ifndef H_HW_BUTTONS_
#define H_HW_BUTTONS_

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Adjust to match your wiring. Use GPIOs with internal pull resistors (not
 * the input-only GPIO 34..39 of the classic ESP32) and none that are already
 * used (LED data pin: LED_STRIP_GPIO_PIN in led_strip_ctrl.h). */

/* Left button, -1 = none: then the right button does everything with short
 * and long presses. */
#define HW_BUTTON_LEFT_GPIO -1

/* Right button. GPIO 0 is the BOOT button on the ESP32 dev board. */
#define HW_BUTTON_RIGHT_GPIO 0

/* One-button mode only: how long to hold the button for "previous". */
#define HW_BUTTON_LONG_PRESS_MS 600

/* Level the GPIO has while a button is pressed: 0 for button to GND (with
 * the internal pull-up), 1 for button to 3.3V (with the internal pull-down). */
#define HW_BUTTON_ACTIVE_LEVEL 0

    /* Configures the GPIOs and starts the task that watches them. Call once,
     * after led_strip_ctrl_init(). */
    esp_err_t hw_buttons_init(void);

#ifdef __cplusplus
}
#endif

#endif
