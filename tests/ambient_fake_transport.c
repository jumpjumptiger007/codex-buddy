#include "ambient_fake_transport.h"

#include <stdint.h>
#include <string.h>

static bool ambient_fake_is_connected(void *context)
{
    const ambient_fake_transport_t *fake = context;
    return fake && fake->connected;
}

static ambient_transport_result_t ambient_fake_send(void *context,
                                                     const uint8_t *bytes,
                                                     size_t byte_count)
{
    ambient_fake_transport_t *fake = context;
    size_t tail;

    if (!fake || !bytes || byte_count == 0) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    if (byte_count > fake->message_capacity_bytes) {
        return AMBIENT_TRANSPORT_TOO_LARGE;
    }
    if (fake->count == fake->queue_capacity) {
        return AMBIENT_TRANSPORT_WOULD_BLOCK;
    }

    tail = (fake->head + fake->count) % fake->queue_capacity;
    memcpy(fake->storage + tail * fake->message_capacity_bytes, bytes,
           byte_count);
    fake->message_lengths[tail] = byte_count;
    fake->count++;
    return AMBIENT_TRANSPORT_OK;
}

static ambient_transport_result_t ambient_fake_receive(
    void *context,
    uint8_t *buffer,
    size_t buffer_capacity,
    size_t *received_bytes)
{
    ambient_fake_transport_t *fake = context;
    size_t message_length;

    if (!fake || !buffer || buffer_capacity == 0 || !received_bytes) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    *received_bytes = 0;
    if (fake->count == 0) {
        return AMBIENT_TRANSPORT_WOULD_BLOCK;
    }
    message_length = fake->message_lengths[fake->head];
    if (message_length > buffer_capacity) {
        return AMBIENT_TRANSPORT_TOO_LARGE;
    }

    memcpy(buffer, fake->storage + fake->head * fake->message_capacity_bytes,
           message_length);
    *received_bytes = message_length;
    fake->head = (fake->head + 1) % fake->queue_capacity;
    fake->count--;
    return AMBIENT_TRANSPORT_OK;
}

static const ambient_transport_ops_t ambient_fake_ops = {
    .is_connected = ambient_fake_is_connected,
    .send = ambient_fake_send,
    .receive = ambient_fake_receive,
};

bool ambient_fake_transport_init(ambient_fake_transport_t *fake,
                                 uint8_t *storage,
                                 size_t storage_capacity_bytes,
                                 size_t *message_lengths,
                                 size_t queue_capacity,
                                 size_t message_capacity_bytes,
                                 size_t max_message_bytes,
                                 ambient_transport_t *transport)
{
    size_t required_storage;

    if (!fake || !storage || !message_lengths || !transport
        || queue_capacity == 0 || message_capacity_bytes == 0
        || max_message_bytes == 0
        || max_message_bytes > message_capacity_bytes
        || queue_capacity > SIZE_MAX / sizeof(*message_lengths)
        || queue_capacity > SIZE_MAX / message_capacity_bytes) {
        return false;
    }
    required_storage = queue_capacity * message_capacity_bytes;
    if (storage_capacity_bytes < required_storage) {
        return false;
    }

    memset(storage, 0, required_storage);
    memset(message_lengths, 0, sizeof(*message_lengths) * queue_capacity);
    fake->connected = false;
    fake->storage = storage;
    fake->storage_capacity_bytes = storage_capacity_bytes;
    fake->message_lengths = message_lengths;
    fake->queue_capacity = queue_capacity;
    fake->message_capacity_bytes = message_capacity_bytes;
    fake->head = 0;
    fake->count = 0;
    transport->ops = &ambient_fake_ops;
    transport->context = fake;
    transport->max_message_bytes = max_message_bytes;
    return true;
}

void ambient_fake_transport_set_connected(ambient_fake_transport_t *fake,
                                          bool connected)
{
    if (fake) {
        fake->connected = connected;
    }
}
