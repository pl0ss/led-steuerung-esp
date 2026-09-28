set(srcs "main.c"
         "gatt_svr.c")

idf_component_register(
    SRCS
        "main.c"
        "gatt_svr.c"
        "ble_read_handler.c"
        "ble_write_handler.c"
        "led_strip_ctrl.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_timer bt console
    REQUIRES espressif__led_strip
)