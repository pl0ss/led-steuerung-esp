set(srcs "main.c"
         "gatt_svr.c")

idf_component_register(SRCS "ble_read_handler.c" "ble_write_handler.c" "${srcs}"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES cjson)