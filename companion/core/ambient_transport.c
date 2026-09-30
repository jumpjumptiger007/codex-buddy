#include "ambient_transport.h"

ambient_transport_result_t ambient_transport_send(
    const ambient_transport_t *transport,
    const uint8_t *bytes,
    size_t byte_count)
{
    if (!transport || !transport->ops || !transport->ops->is_connected
        || !transport->ops->send || !bytes || byte_count == 0
        || transport->max_message_bytes == 0) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    if (byte_count > transport->max_message_bytes) {
        return AMBIENT_TRANSPORT_TOO_LARGE;
    }
    if (!transport->ops->is_connected(transport->context)) {
        return AMBIENT_TRANSPORT_DISCONNECTED;
    }
    return transport->ops->send(transport->context, bytes, byte_count);
}

ambient_transport_result_t ambient_transport_receive(
    const ambient_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_capacity,
    size_t *received_bytes)
{
    ambient_transport_result_t result;

    if (!transport || !transport->ops || !transport->ops->is_connected
        || !transport->ops->receive || !buffer || buffer_capacity == 0
        || !received_bytes || transport->max_message_bytes == 0) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    *received_bytes = 0;
    if (!transport->ops->is_connected(transport->context)) {
        return AMBIENT_TRANSPORT_DISCONNECTED;
    }
    if (buffer_capacity > transport->max_message_bytes) {
        buffer_capacity = transport->max_message_bytes;
    }
    result = transport->ops->receive(transport->context, buffer,
                                     buffer_capacity, received_bytes);
    if (*received_bytes > buffer_capacity
        || (result != AMBIENT_TRANSPORT_OK && *received_bytes != 0)) {
        *received_bytes = 0;
        return AMBIENT_TRANSPORT_INVALID;
    }
    return result;
}
