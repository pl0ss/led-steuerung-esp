/*
 * Shared definitions for the two extra characteristics added on top of the
 * bleprph example (see ble_write_handler.c and ble_read_handler.c).
 */

#ifndef H_BLE_PROTOCOL_
#define H_BLE_PROTOCOL_

#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Both characteristics are added to the existing demo service
 * (gatt_svr_svc_uuid in gatt_svr.c), so no new service UUID is needed.
 *
 *   - BLE_CMD_CHR_UUID   : write-only "command" channel. The app writes a
 *                          JSON message describing what the ESP32 should
 *                          do (see ble_write_handler.h/.c).
 *
 *   - BLE_QUERY_CHR_UUID : write + read "query" channel. The app writes a
 *                          JSON message selecting which piece of state it
 *                          wants ("endpoint"), then performs a GATT read
 *                          on the same characteristic to get the answer
 *                          (see ble_read_handler.h/.c).
 *
 * Both are freshly generated 128-bit UUIDs, there is nothing "standard"
 * about them.
 */
#define BLE_CMD_CHR_UUID                                             \
    BLE_UUID128_INIT(0xfd, 0x85, 0xc2, 0x45, 0xfd, 0x03, 0xf4, 0x95, \
                     0x95, 0x4f, 0x9d, 0x1c, 0x07, 0xfb, 0xe9, 0x67)

#define BLE_QUERY_CHR_UUID                                           \
    BLE_UUID128_INIT(0xfe, 0x27, 0x92, 0x89, 0x99, 0x3e, 0xf9, 0xb8, \
                     0x1a, 0x46, 0x94, 0xea, 0x54, 0x41, 0x59, 0x37)

/*
 * Max size (bytes) of a single JSON message, in either direction.
 * Kept intentionally small: the default ATT MTU only leaves 20 bytes of
 * payload, so anything bigger requires the app to negotiate a larger MTU
 * first (BleClient.requestMtu on the Capacitor side). Since throughput is
 * not a concern here, 512 bytes is just a safety cap against runaway
 * writes, not a target size.
 */
#define BLE_JSON_MSG_MAX_LEN 512

#ifdef __cplusplus
}
#endif

#endif
