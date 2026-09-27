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
 * malformed messages are only logged, ble_msg_handler_access_cb() still
 * returns success. Use the query characteristic (ble_read_handler.c)
 * whenever the app needs an actual answer back.
 */

#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "ble_write_handler.h"
#include "ble_protocol.h"

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

// TODO
/*
 * TODO(led): message types for LED control (e.g. "set_color",
 * "set_preset") go here once the LED driver exists. Intentionally not
 * implemented yet.
 */

static const ble_msg_type_entry_t msg_handlers[] = {
    {"ping", handle_ping},
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
