idf_component_register(
    SRCS
        "src/ambient_session.c"
        "src/ambient_authenticated_session.c"
        "src/ambient_session_ble.c"
    INCLUDE_DIRS "include"
    REQUIRES ambient_ble ambient_wire ambient_auth
)
