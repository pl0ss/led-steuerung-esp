/*
 * The small blue LED of the dev board (labelled "D2", wired to GPIO 2) shows
 * whether a phone is connected over BLE: on while connected, off otherwise.
 * (The red "PWR" LED is hardwired to the power supply, software can't touch it.)
 */

#ifndef H_STATUS_LED_
#define H_STATUS_LED_

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* GPIO of the LED. GPIO 2 is the "D2" LED on the ESP32 NodeMCU board. */
#define STATUS_LED_GPIO 2

/* Level that turns the LED on: 1 if the LED sits between GPIO and GND (as on
 * the NodeMCU board), 0 if it sits between 3.3V and the GPIO. */
#define STATUS_LED_ON_LEVEL 1

    /* Configures the GPIO as output and switches the LED off. Call once. */
    esp_err_t status_led_init(void);

    /* true = LED on, false = off */
    void status_led_set(bool on);

#ifdef __cplusplus
}
#endif

#endif
