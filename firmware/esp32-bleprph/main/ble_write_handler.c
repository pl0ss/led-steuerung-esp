/*
 * Handles incoming BLE writes on the "command" characteristic.
 *
 * Wire format (UTF-8 JSON, see ble_protocol.h for the size limit):
 *
 *   { "type": "<message type>", "payload": { ... } }
 *
 * "type" selects which handler in msg_handlers[] below gets called with the
 * "payload" object ("payload" may be omitted for messages that don't need
 * one). To add a new message type: write a handler function and add one
 * entry to msg_handlers[]. Nothing else in this file, and nothing in
 * gatt_svr.c, needs to change.
 *
 * This channel gives you an ATT-level write acknowledgement (write, not
 * writeWithoutResponse, so BleClient.write()'s promise only resolves once
 * the ESP32 confirmed receipt), but no application-level answer: unknown or
 * malformed messages are only logged, ble_write_handler_access_cb() still
 * returns success. Use the query characteristic (ble_read_handler.c)
 * whenever the app needs an actual answer back.
 */

#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "ble_write_handler.h"
#include "ble_protocol.h"
#include "led_strip_ctrl.h"

static const char *TAG = "ble_write_handler";

uint16_t ble_write_handler_chr_val_handle;

typedef void (*ble_msg_type_handler_t)(const cJSON *payload);

typedef struct
{
    const char *type;
    ble_msg_type_handler_t handler;
} ble_msg_type_entry_t;

/* --- individual message-type handlers -------------------------------------- */

/*
 * Demo message, only here to prove the write path works end to end:
 *   { "type": "ping" }
 */
static void handle_ping(const cJSON *payload)
{
    (void)payload;
    ESP_LOGI(TAG, "ping received");
}

/* Reads a single required numeric field out of payload, clamped into
 * [0, max]. Returns false (and logs) if the field is missing/not a number. */
static bool read_clamped_uint(const cJSON *payload, const char *field,
                              long max_value, long *out_value)
{
    const cJSON *field_json = cJSON_GetObjectItemCaseSensitive(payload, field);
    if (!cJSON_IsNumber(field_json))
    {
        ESP_LOGW(TAG, "message is missing a numeric \"%s\" field", field);
        return false;
    }

    long value = (long)field_json->valuedouble;
    if (value < 0)
    {
        value = 0;
    }
    else if (value > max_value)
    {
        value = max_value;
    }

    *out_value = value;
    return true;
}

/*
 * Sets every LED on the strip to the same color:
 *   { "type": "set_color", "payload": { "r": 255, "g": 0, "b": 0 } }
 */
static void handle_set_color(const cJSON *payload)
{
    if (payload == NULL)
    {
        ESP_LOGW(TAG, "set_color message is missing a payload");
        return;
    }

    const cJSON *r_json = cJSON_GetObjectItemCaseSensitive(payload, "r");
    const cJSON *g_json = cJSON_GetObjectItemCaseSensitive(payload, "g");
    const cJSON *b_json = cJSON_GetObjectItemCaseSensitive(payload, "b");

    if (!cJSON_IsNumber(r_json) || !cJSON_IsNumber(g_json) || !cJSON_IsNumber(b_json))
    {
        ESP_LOGW(TAG, "set_color payload needs numeric \"r\", \"g\", \"b\" fields");
        return;
    }

    uint8_t red = (uint8_t)r_json->valuedouble;
    uint8_t green = (uint8_t)g_json->valuedouble;
    uint8_t blue = (uint8_t)b_json->valuedouble;

    ESP_LOGI(TAG, "set_color: r=%u g=%u b=%u", red, green, blue);
    led_strip_ctrl_set_color(red, green, blue);
}

/*
 * Sets the overall brightness in percent (0..100), applied on top of the
 * current color:
 *   { "type": "set_brightness", "payload": { "brightness": 50 } }
 */
static void handle_set_brightness(const cJSON *payload)
{
    if (payload == NULL)
    {
        ESP_LOGW(TAG, "set_brightness message is missing a payload");
        return;
    }

    long brightness;
    if (!read_clamped_uint(payload, "brightness", 100, &brightness))
    {
        return;
    }

    ESP_LOGI(TAG, "set_brightness: %ld%%", brightness);
    led_strip_ctrl_set_brightness((uint8_t)brightness);
}

/*
 * Sets how many LEDs (starting from index 0) should be lit, the rest is
 * turned off:
 *   { "type": "set_led_count", "payload": { "count": 150 } }
 */
static void handle_set_led_count(const cJSON *payload)
{
    if (payload == NULL)
    {
        ESP_LOGW(TAG, "set_led_count message is missing a payload");
        return;
    }

    long count;
    if (!read_clamped_uint(payload, "count", LED_STRIP_LED_COUNT, &count))
    {
        return;
    }

    ESP_LOGI(TAG, "set_led_count: %ld", count);
    led_strip_ctrl_set_led_count((uint16_t)count);
}

/*
 * Starts the rainbow animation. Runs on the ESP32 itself until a
 * "set_color" message (or a future effect) switches back to a solid
 * color, independent of whether a phone stays connected:
 *   { "type": "start_rainbow" }
 */
static void handle_start_rainbow(const cJSON *payload)
{
    (void)payload;
    ESP_LOGI(TAG, "start_rainbow");
    led_strip_ctrl_start_rainbow();
}

/*
 * TODO(led): message types for presets (e.g. "set_preset") go here once
 * presets exist. Intentionally not implemented yet.
 */

static const ble_msg_type_entry_t msg_handlers[] = {
    {"ping", handle_ping},
    {"set_color", handle_set_color},
    {"set_brightness", handle_set_brightness},
    {"set_led_count", handle_set_led_count},
    {"start_rainbow", handle_start_rainbow},
};

/* --- dispatch --------------------------------------------------------------- */

static void dispatch_message(const uint8_t *data, uint16_t len)
{
    cJSON *root = cJSON_ParseWithLength((const char *)data, len);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "received invalid JSON (%u bytes)", len);
        return;
    }

    const cJSON *type_json = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_json) || type_json->valuestring == NULL)
    {
        ESP_LOGW(TAG, "message is missing a \"type\" string field");
        cJSON_Delete(root);
        return;
    }

    const cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");

    for (size_t i = 0; i < sizeof(msg_handlers) / sizeof(msg_handlers[0]); i++)
    {
        if (strcmp(type_json->valuestring, msg_handlers[i].type) == 0)
        {
            msg_handlers[i].handler(payload);
            cJSON_Delete(root);
            return;
        }
    }

    ESP_LOGW(TAG, "unknown message type \"%s\"", type_json->valuestring);
    cJSON_Delete(root);
}

int ble_write_handler_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len == 0 || om_len > BLE_JSON_MSG_MAX_LEN)
    {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[BLE_JSON_MSG_MAX_LEN];
    uint16_t out_len = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
    if (rc != 0)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    dispatch_message(buf, out_len);
    return 0;
}