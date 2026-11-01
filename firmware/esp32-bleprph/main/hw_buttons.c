/*
 * Hardware buttons, see hw_buttons.h. A small task polls the GPIOs every
 * 10 ms (polling instead of interrupts: buttons bounce, and 10 ms is far
 * below what a human notices) and applies the selected list entry through the
 * same led_strip_ctrl setters the BLE messages use. That makes the two
 * control paths equal: a button press and a phone command can follow each
 * other in any order.
 */

#include <stdbool.h>
#include <stdint.h>
#include "hw_buttons.h"
#include "led_animations.h"
#include "led_strip_ctrl.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hw_buttons";

#define BUTTON_POLL_MS 10

/* A change of the pin level only counts after it stayed that way for this
 * many polls in a row (3 * 10 ms), which swallows contact bounce. */
#define BUTTON_DEBOUNCE_POLLS 3

typedef struct
{
    const char *name;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_preset_t;

/* The colors, in button order. WS2812B are much brighter in green than in
 * red, so "orange" and "yellow" need a lot less green than the textbook
 * values to look like orange and yellow. Tweak to taste. */
static const color_preset_t color_presets[] = {
    {"orange", 255, 70, 0},
    {"purple", 150, 0, 255},
    {"yellow", 255, 200, 0},
    {"blue", 0, 0, 255},
    {"green", 0, 255, 0},
    {"red", 255, 0, 0},
};

#define COLOR_PRESET_COUNT (sizeof(color_presets) / sizeof(color_presets[0]))

/* The list is [color_presets..., off, led_animations...]:
 *   0 .. COLOR_PRESET_COUNT-1               a color
 *   COLOR_PRESET_COUNT                      off
 *   COLOR_PRESET_COUNT+1 .. total_entries-1 an animation */
#define OFF_INDEX COLOR_PRESET_COUNT

static size_t total_entries(void)
{
    return COLOR_PRESET_COUNT + 1 + led_animation_count;
}

/* Only touched by the button task, so no locking needed. Starts at "off". */
static size_t current_index = OFF_INDEX;

static void apply_entry(size_t index)
{
    if (index < COLOR_PRESET_COUNT)
    {
        const color_preset_t *color = &color_presets[index];
        ESP_LOGI(TAG, "entry %u: color %s", (unsigned)index, color->name);

        /* Color first, then power on: coming from "off" the strip would
         * otherwise flash up in the old color for a moment. */
        led_strip_ctrl_set_color(color->red, color->green, color->blue);
        led_strip_ctrl_set_power(true);
    }
    else if (index == OFF_INDEX)
    {
        ESP_LOGI(TAG, "entry %u: off", (unsigned)index);
        led_strip_ctrl_set_power(false);
    }
    else
    {
        size_t animation = index - OFF_INDEX - 1;
        ESP_LOGI(TAG, "entry %u: animation %s", (unsigned)index,
                 led_animations[animation].name);

        led_strip_ctrl_start_animation(animation);
        led_strip_ctrl_set_power(true);
    }
}

static void step(int delta)
{
    size_t total = total_entries();

    /* wraps around at both ends (adding total - 1 is the same as -1) */
    current_index = (current_index + (delta > 0 ? 1 : total - 1)) % total;
    apply_entry(current_index);
}

typedef struct
{
    gpio_num_t gpio;
    bool pressed;       /* debounced state */
    uint8_t same_polls; /* how many polls in a row the raw level differed from "pressed" */
    uint16_t held_polls; /* how many polls the button has been held down */
    bool long_fired;    /* the long press of the current press was already handled */
} button_t;

typedef enum
{
    BUTTON_NONE,
    BUTTON_PRESSED,  /* just went down */
    BUTTON_RELEASED, /* just went up */
} button_edge_t;

/* Call once per poll. Reports the debounced press / release edges. */
static button_edge_t button_update(button_t *button)
{
    bool raw_pressed = gpio_get_level(button->gpio) == HW_BUTTON_ACTIVE_LEVEL;

    if (raw_pressed == button->pressed)
    {
        button->same_polls = 0;
        if (button->pressed && button->held_polls < UINT16_MAX)
        {
            button->held_polls++;
        }
        return BUTTON_NONE;
    }

    if (++button->same_polls < BUTTON_DEBOUNCE_POLLS)
    {
        return BUTTON_NONE;
    }

    button->same_polls = 0;
    button->pressed = raw_pressed;
    if (raw_pressed)
    {
        button->held_polls = 0;
        button->long_fired = false;
        return BUTTON_PRESSED;
    }
    /* long_fired stays as it is on release, the caller still needs it */
    return BUTTON_RELEASED;
}

#define LONG_PRESS_POLLS (HW_BUTTON_LONG_PRESS_MS / BUTTON_POLL_MS)

/* One button: short press = next (on release, since only then it is clear
 * that it was not a long press), long press = previous (as soon as the time
 * is up, while the button is still down). */
static void one_button_poll(button_t *button)
{
    button_edge_t edge = button_update(button);

    if (edge == BUTTON_RELEASED && !button->long_fired)
    {
        step(+1);
    }
    else if (button->pressed && !button->long_fired &&
             button->held_polls >= LONG_PRESS_POLLS)
    {
        button->long_fired = true;
        step(-1);
    }
}

static void buttons_task_fn(void *arg)
{
    (void)arg;

    button_t right = {.gpio = HW_BUTTON_RIGHT_GPIO};
#if HW_BUTTON_LEFT_GPIO >= 0
    button_t left = {.gpio = HW_BUTTON_LEFT_GPIO};
#endif

    for (;;)
    {
#if HW_BUTTON_LEFT_GPIO >= 0
        if (button_update(&left) == BUTTON_PRESSED)
        {
            step(-1);
        }
        if (button_update(&right) == BUTTON_PRESSED)
        {
            step(+1);
        }
#else
        one_button_poll(&right);
#endif

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

esp_err_t hw_buttons_init(void)
{
    gpio_config_t config = {
#if HW_BUTTON_LEFT_GPIO >= 0
        .pin_bit_mask = (1ULL << HW_BUTTON_LEFT_GPIO) | (1ULL << HW_BUTTON_RIGHT_GPIO),
#else
        .pin_bit_mask = (1ULL << HW_BUTTON_RIGHT_GPIO),
#endif
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = HW_BUTTON_ACTIVE_LEVEL == 0 ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = HW_BUTTON_ACTIVE_LEVEL == 0 ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ok = xTaskCreate(buttons_task_fn, "hw_buttons", 3072, NULL,
                                tskIDLE_PRIORITY + 1, NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "failed to create button task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "buttons ready (left GPIO %d, right GPIO %d, left -1 = one-button mode), "
                  "%u entries, start at off",
             HW_BUTTON_LEFT_GPIO, HW_BUTTON_RIGHT_GPIO, (unsigned)total_entries());
    return ESP_OK;
}
