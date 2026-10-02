idf_component_register(SRCS "src/ambient_auth.c" "src/ambient_identity.c" "src/ambient_identity_esp.c"
    INCLUDE_DIRS "include" REQUIRES mbedtls nvs_flash esp_hw_support bootloader_support)
