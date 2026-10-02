set(AMBIENT_R1_CORE "${CMAKE_CURRENT_LIST_DIR}/../../companion/core")
idf_component_register(
    SRCS
        "${AMBIENT_R1_CORE}/ambient_wire.c"
        "${AMBIENT_R1_CORE}/ambient_protocol.c"
        "${AMBIENT_R1_CORE}/ambient_transport.c"
    INCLUDE_DIRS "${AMBIENT_R1_CORE}"
)
