/*
 * All animations the strip can play, as a table. This table is the single
 * source of truth: led_strip_ctrl.c plays entries from it, the hardware
 * buttons (hw_buttons.c) append every entry to their cycle, and the app asks
 * for the list over BLE (query endpoint "animations") and only shows what is
 * in here. So adding an animation means adding one function and one table
 * entry in led_animations.c, nothing else (no app update needed).
 */

#ifndef H_LED_ANIMATIONS_
#define H_LED_ANIMATIONS_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Computes the color of one LED for one frame, at full brightness (the
     * caller applies the brightness setting afterwards).
     *
     *   index     which LED, 0 .. led_count - 1
     *   led_count how many LEDs are lit right now
     *   frame     counts up by one per frame and wraps at 255, use it to
     *             move things along the strip
     */
    typedef void (*led_animation_pixel_fn)(uint16_t index, uint16_t led_count,
                                           uint8_t frame,
                                           uint8_t *red, uint8_t *green, uint8_t *blue);

    typedef struct
    {
        const char *name;  /* stable id, used in BLE messages and in the app */
        const char *label; /* text the app shows on its button */
        uint16_t step_ms;  /* time between two frames */
        led_animation_pixel_fn pixel;
    } led_animation_t;

    extern const led_animation_t led_animations[];
    extern const size_t led_animation_count;

    /* Index of the animation with this name, or -1 if there is none. */
    int led_animations_find(const char *name);

#ifdef __cplusplus
}
#endif

#endif
