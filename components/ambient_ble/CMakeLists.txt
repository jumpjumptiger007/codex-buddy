idf_component_register(
    SRCS
        "src/ambient_ble.c"
        "src/ambient_ble_gap.c"
        "src/ambient_ble_gatt.c"
        "src/ambient_ble_tx.c"
        "src/ambient_ble_peer_nvs.c"
        "src/ambient_ble_ring.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES bt nvs_flash
)
