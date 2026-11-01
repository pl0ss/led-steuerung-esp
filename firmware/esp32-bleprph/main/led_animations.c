/*
 * The animation table, see led_animations.h. To add an animation: write a
 * pixel function like rainbow_pixel() below and add a line to led_animations[].
 */

#include <string.h>
#include "led_animations.h"

/* Standard integer HSV -> RGB conversion, h/s/v all 0..255. Used by the
 * rainbow animation below. */
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0)
    {
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (uint8_t)(((uint16_t)v * (255 - s)) >> 8);
    uint8_t q = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * remainder) >> 8))) >> 8);
    uint8_t t = (uint8_t)(((uint16_t)v * (255 - (((uint16_t)s * (255 - remainder)) >> 8))) >> 8);

    switch (region)
    {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}

/* One full hue circle spread over the lit LEDs, moving along the strip. */
static void rainbow_pixel(uint16_t index, uint16_t led_count, uint8_t frame,
                          uint8_t *red, uint8_t *green, uint8_t *blue)
{
    uint16_t hue_span = led_count == 0 ? 1 : led_count;
    uint8_t hue = (uint8_t)(((index * 256 / hue_span) + frame) & 0xFF);
    hsv_to_rgb(hue, 255, 255, red, green, blue);
}

const led_animation_t led_animations[] = {
    {"rainbow", "Regenbogen", 20, rainbow_pixel},
};

const size_t led_animation_count = sizeof(led_animations) / sizeof(led_animations[0]);

int led_animations_find(const char *name)
{
    if (name == NULL)
    {
        return -1;
    }

    for (size_t i = 0; i < led_animation_count; i++)
    {
        if (strcmp(name, led_animations[i].name) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}
