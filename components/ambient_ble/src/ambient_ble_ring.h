#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMBIENT_BLE_RX_RING_CAPACITY 516U

typedef struct {
    uint8_t bytes[AMBIENT_BLE_RX_RING_CAPACITY];
    size_t head;
    size_t length;
} ambient_ble_ring_t;

void ambient_ble_ring_reset(ambient_ble_ring_t *ring);
bool ambient_ble_ring_write(ambient_ble_ring_t *ring,
                            const uint8_t *bytes,
                            size_t length);
size_t ambient_ble_ring_read(ambient_ble_ring_t *ring,
                             uint8_t *out,
                             size_t capacity);
