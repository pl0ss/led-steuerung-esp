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

/* Cheap deterministic "random" byte for a (position, time step) pair. The
 * animation functions have no state, so flicker has to be computed from the
 * LED index and the frame number instead of from a random number generator. */
static uint8_t hash8(uint16_t a, uint16_t b)
{
    uint32_t h = (uint32_t)a * 2654435761u + (uint32_t)b * 40503u + 0x9E3779B9u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (uint8_t)h;
}

/* Smooth noise: a random value every 4 frames, linearly blended in between.
 * The 64 steps wrap around exactly like the 8-bit frame counter, so there is
 * no jump when the frame counter overflows. */
static uint8_t smooth_noise(uint16_t seed, uint8_t frame)
{
    uint8_t step = frame >> 2;
    int from = hash8(seed, step);
    int to = hash8(seed, (step + 1) & 63);
    int blend = (frame & 3) * 64; /* 0, 64, 128, 192 */
    return (uint8_t)(from + ((to - from) * blend) / 256);
}

/* Triangle wave 0..255..0 over one full frame counter turn. */
static uint8_t triangle(uint8_t phase)
{
    return phase < 128 ? (uint8_t)(phase * 2) : (uint8_t)((255 - phase) * 2);
}

/* Candle: one warm color that flickers in brightness. Part of the flicker is
 * shared by the whole strip (a flame moves as a whole), part is different per
 * LED. When it gets dimmer it also turns a bit redder, like a real flame. */
static void candle_pixel(uint16_t index, uint16_t led_count, uint8_t frame,
                         uint8_t *red, uint8_t *green, uint8_t *blue)
{
    (void)led_count;

    uint8_t shared = smooth_noise(0xFFFF, frame);
    uint8_t local = smooth_noise(index, frame);
    uint8_t mixed = (uint8_t)((shared * 6 + local * 4) / 10);

    /* 120..255 of full brightness */
    uint32_t level = 120 + (mixed * 135u) / 255;

    *red = (uint8_t)level;
    *green = (uint8_t)((90u * level * level) / (255u * 255u));
    *blue = (uint8_t)((4u * level) / 255u);
}

/* Warm fade: the whole strip slowly drifts between orange and yellow. The
 * phase is shifted a little along the strip, so the colors seem to roll
 * through instead of everything changing at once. */
static void warm_fade_pixel(uint16_t index, uint16_t led_count, uint8_t frame,
                            uint8_t *red, uint8_t *green, uint8_t *blue)
{
    uint16_t span = led_count == 0 ? 1 : led_count;
    uint8_t phase = (uint8_t)(frame + (index * 96u) / span);

    /* Hue 6 is a red-ish orange, hue 26 a warm yellow. Going further towards
     * green would look yellow-green on a WS2812. */
    uint8_t hue = (uint8_t)(6 + (triangle(phase) * 20u) / 255);
    hsv_to_rgb(hue, 255, 255, red, green, blue);
}

const led_animation_t led_animations[] = {
    {"rainbow", "Regenbogen", 20, rainbow_pixel},
    {"candle", "Kerze", 50, candle_pixel},
    {"warm_fade", "Warmes Gelb-Orange", 50, warm_fade_pixel},
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
