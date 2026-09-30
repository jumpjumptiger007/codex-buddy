#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_transport.h"

typedef struct {
    bool connected;
    uint8_t *storage;
    size_t storage_capacity_bytes;
    size_t *message_lengths;
    size_t queue_capacity;
    size_t message_capacity_bytes;
    size_t head;
    size_t count;
} ambient_fake_transport_t;

bool ambient_fake_transport_init(ambient_fake_transport_t *fake,
                                 uint8_t *storage,
                                 size_t storage_capacity_bytes,
                                 size_t *message_lengths,
                                 size_t queue_capacity,
                                 size_t message_capacity_bytes,
                                 size_t max_message_bytes,
                                 ambient_transport_t *transport);

void ambient_fake_transport_set_connected(ambient_fake_transport_t *fake,
                                          bool connected);
