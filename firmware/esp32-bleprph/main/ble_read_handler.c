/*
 * Handles the "query" characteristic: acts like a tiny REST router over
 * BLE. The app first WRITEs which "endpoint" it wants:
 *
 *   { "endpoint": "status" }
 *
 * and then performs a GATT READ on the same characteristic to get the
 * answer:
 *
 *   { "endpoint": "status", "data": { ... } }
 *
 * Reading before ever writing an endpoint answers "status" by default, so
 * the characteristic is useful right away.
 *
 * To add a new endpoint: write a handler that builds and returns a cJSON
 * object (ownership passes to the caller) and add one entry to
 * query_endpoints[] below. Nothing else in this file, and nothing in
 * gatt_svr.c, needs to change.
 */

#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "host/ble_hs.h"
#include "ble_read_handler.h"
#include "ble_protocol.h"
#include "led_strip_ctrl.h"

static const char *TAG = "ble_read_handler";

uint16_t ble_read_handler_chr_val_handle;

#define ENDPOINT_NAME_MAX_LEN 32

/* Currently selected endpoint. "status" is the default so a read before
 * any write still returns something useful. */
static char current_endpoint[ENDPOINT_NAME_MAX_LEN] = "status";

typedef cJSON *(*ble_query_handler_t)(void);

typedef struct
{
    const char *name;
    ble_query_handler_t handler;
} ble_query_endpoint_t;

/* --- individual endpoint handlers -------------------------------------------- */

/* { "endpoint": "status", "data": { "uptime_ms": ..., "free_heap": ... } } */
static cJSON *handle_status(void)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "uptime_ms", (double)(esp_timer_get_time() / 1000));
    cJSON_AddNumberToObject(data, "free_heap", (double)esp_get_free_heap_size());
    return data;
}

/* { "endpoint": "color", "data": { "r": ..., "g": ..., "b": ... } } */
static cJSON *handle_color(void)
{
    uint8_t red, green, blue;
    led_strip_ctrl_get_color(&red, &green, &blue);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "r", red);
    cJSON_AddNumberToObject(data, "g", green);
    cJSON_AddNumberToObject(data, "b", blue);
    return data;
}

/* { "endpoint": "brightness", "data": { "percent": ... } } */
static cJSON *handle_brightness(void)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "percent", led_strip_ctrl_get_brightness());
    return data;
}

/* { "endpoint": "led_count", "data": { "count": ..., "max": ... } } */
static cJSON *handle_led_count(void)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "count", led_strip_ctrl_get_led_count());
    cJSON_AddNumberToObject(data, "max", LED_STRIP_LED_COUNT);
    return data;
}

/*
 * TODO(led): the "preset" endpoint goes here once presets exist. It
 * already exists below so the app-side router has something to call
 * against, it just reports that there is nothing to report yet.
 */
static cJSON *handle_not_implemented(void)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "implemented", false);
    return data;
}

static const ble_query_endpoint_t query_endpoints[] = {
    {"status", handle_status},
    {"color", handle_color},
    {"brightness", handle_brightness},
    {"led_count", handle_led_count},
    {"preset", handle_not_implemented},
};

/* --- write: select an endpoint ------------------------------------------------ */

static int handle_select_endpoint(struct os_mbuf *om)
{
    uint16_t om_len = OS_MBUF_PKTLEN(om);
    if (om_len == 0 || om_len > BLE_JSON_MSG_MAX_LEN)
    {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[BLE_JSON_MSG_MAX_LEN];
    uint16_t out_len = 0;
    int rc = ble_hs_mbuf_to_flat(om, buf, sizeof(buf), &out_len);
    if (rc != 0)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)buf, out_len);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "received invalid JSON (%u bytes)", out_len);
        return BLE_ATT_ERR_UNLIKELY;
    }

    const cJSON *endpoint_json = cJSON_GetObjectItemCaseSensitive(root, "endpoint");
    if (!cJSON_IsString(endpoint_json) || endpoint_json->valuestring == NULL)
    {
        ESP_LOGW(TAG, "message is missing an \"endpoint\" string field");
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }

    bool known = false;
    for (size_t i = 0; i < sizeof(query_endpoints) / sizeof(query_endpoints[0]); i++)
    {
        if (strcmp(endpoint_json->valuestring, query_endpoints[i].name) == 0)
        {
            known = true;
            break;
        }
    }

    if (!known)
    {
        ESP_LOGW(TAG, "unknown endpoint \"%s\"", endpoint_json->valuestring);
        cJSON_Delete(root);
        return BLE_ATT_ERR_UNLIKELY;
    }

    strlcpy(current_endpoint, endpoint_json->valuestring, sizeof(current_endpoint));
    cJSON_Delete(root);
    return 0;
}

/* --- read: answer the currently selected endpoint ----------------------------- */

static int handle_read_endpoint(struct ble_gatt_access_ctxt *ctxt)
{
    ble_query_handler_t handler = NULL;
    for (size_t i = 0; i < sizeof(query_endpoints) / sizeof(query_endpoints[0]); i++)
    {
        if (strcmp(current_endpoint, query_endpoints[i].name) == 0)
        {
            handler = query_endpoints[i].handler;
            break;
        }
    }

    if (handler == NULL)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "endpoint", current_endpoint);
    cJSON_AddItemToObject(root, "data", handler());

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL)
    {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    int rc = os_mbuf_append(ctxt->om, json_str, strlen(json_str));
    cJSON_free(json_str);

    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

/* --- GATT access callback ------------------------------------------------------ */

int ble_read_handler_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        return handle_read_endpoint(ctxt);
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        return handle_select_endpoint(ctxt->om);
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}
