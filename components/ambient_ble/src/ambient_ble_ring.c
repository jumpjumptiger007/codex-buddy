#include "ambient_ble_ring.h"

#include <string.h>

void ambient_ble_ring_reset(ambient_ble_ring_t *ring)
{
    if (ring != NULL) {
        memset(ring->bytes, 0, sizeof(ring->bytes));
        ring->head = 0;
        ring->length = 0;
    }
}

bool ambient_ble_ring_write(ambient_ble_ring_t *ring,
                            const uint8_t *bytes,
                            size_t length)
{
    size_t tail;
    size_t first;

    if (ring == NULL || bytes == NULL || length == 0 ||
        length > AMBIENT_BLE_RX_RING_CAPACITY ||
        ring->head >= AMBIENT_BLE_RX_RING_CAPACITY ||
        ring->length > AMBIENT_BLE_RX_RING_CAPACITY ||
        length > AMBIENT_BLE_RX_RING_CAPACITY - ring->length) {
        return false;
    }

    tail = (ring->head + ring->length) % AMBIENT_BLE_RX_RING_CAPACITY;
    first = AMBIENT_BLE_RX_RING_CAPACITY - tail;
    if (first > length) {
        first = length;
    }
    memcpy(&ring->bytes[tail], bytes, first);
    memcpy(ring->bytes, &bytes[first], length - first);
    ring->length += length;
    return true;
}

size_t ambient_ble_ring_read(ambient_ble_ring_t *ring,
                             uint8_t *out,
                             size_t capacity)
{
    size_t count;
    size_t first;

    if (ring == NULL || out == NULL || capacity == 0 ||
        ring->head >= AMBIENT_BLE_RX_RING_CAPACITY ||
        ring->length > AMBIENT_BLE_RX_RING_CAPACITY) {
        return 0;
    }

    count = ring->length < capacity ? ring->length : capacity;
    first = AMBIENT_BLE_RX_RING_CAPACITY - ring->head;
    if (first > count) {
        first = count;
    }
    memcpy(out, &ring->bytes[ring->head], first);
    memcpy(&out[first], ring->bytes, count - first);
    ring->head = (ring->head + count) % AMBIENT_BLE_RX_RING_CAPACITY;
    ring->length -= count;
    return count;
}
