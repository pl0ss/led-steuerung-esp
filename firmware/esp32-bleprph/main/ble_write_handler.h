/*
 * Handles incoming BLE writes on the "command" characteristic.
 * See ble_write_handler.c for the wire format and how to add new message
 * types.
 */

#ifndef H_BLE_WRITE_HANDLER_
#define H_BLE_WRITE_HANDLER_

#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Value handle of the command characteristic, filled in by NimBLE once the
     * service is registered. Exposed in case other code needs to compare
     * attr_handle against it. */
    extern uint16_t ble_write_handler_chr_val_handle;

    /*
     * GATT access callback for the command characteristic. Plug this straight
     * into the characteristic definition in gatt_svr.c. Only reacts to
     * BLE_GATT_ACCESS_OP_WRITE_CHR, every other op is rejected.
     */
    int ble_write_handler_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);

#ifdef __cplusplus
}
#endif

#endif
