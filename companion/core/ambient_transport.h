#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    AMBIENT_TRANSPORT_OK = 0,
    AMBIENT_TRANSPORT_WOULD_BLOCK,
    AMBIENT_TRANSPORT_DISCONNECTED,
    AMBIENT_TRANSPORT_TOO_LARGE,
    AMBIENT_TRANSPORT_INVALID,
} ambient_transport_result_t;

/* Implementations borrow buffers only for the duration of each callback. */
typedef struct {
    bool (*is_connected)(void *context);
    ambient_transport_result_t (*send)(void *context,
                                       const uint8_t *bytes,
                                       size_t byte_count);
    ambient_transport_result_t (*receive)(void *context,
                                          uint8_t *buffer,
                                          size_t buffer_capacity,
                                          size_t *received_bytes);
} ambient_transport_ops_t;

typedef struct {
    const ambient_transport_ops_t *ops;
    void *context;
    size_t max_message_bytes;
} ambient_transport_t;

ambient_transport_result_t ambient_transport_send(
    const ambient_transport_t *transport,
    const uint8_t *bytes,
    size_t byte_count);

ambient_transport_result_t ambient_transport_receive(
    const ambient_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_capacity,
    size_t *received_bytes);
