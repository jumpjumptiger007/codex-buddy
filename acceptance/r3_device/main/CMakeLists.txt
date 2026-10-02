idf_component_register(SRCS "r3_acceptance.c" INCLUDE_DIRS "."
    REQUIRES ambient_auth ambient_session ambient_ble bsp esp_timer heap)
