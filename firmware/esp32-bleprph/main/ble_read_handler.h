/*
 * Handles the "query" characteristic (read + write), see ble_read_handler.c
 * for the wire format and how to add new endpoints.
 */

#ifndef H_BLE_READ_HANDLER_
#define H_BLE_READ_HANDLER_

#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Value handle of the query characteristic, filled in by NimBLE once the
 * service is registered. Exposed in case other code needs to compare
 * attr_handle against it. */
extern uint16_t ble_read_handler_chr_val_handle;

/*
 * GATT access callback for the query characteristic. Plug this straight
 * into the characteristic definition in gatt_svr.c. Handles both
 * BLE_GATT_ACCESS_OP_WRITE_CHR (select an endpoint) and
 * BLE_GATT_ACCESS_OP_READ_CHR (answer with that endpoint's JSON).
 */
int ble_read_handler_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

#ifdef __cplusplus
}
#endif

#endif
