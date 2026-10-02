#include "../components/ambient_ble/src/ambient_ble_ring.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    ambient_ble_ring_t ring = {0};
    uint8_t input[AMBIENT_BLE_RX_RING_CAPACITY];
    uint8_t output[AMBIENT_BLE_RX_RING_CAPACITY];

    for (size_t i = 0; i < sizeof(input); i++) {
        input[i] = (uint8_t)i;
    }

    assert(ambient_ble_ring_write(&ring, input, 400));
    assert(ambient_ble_ring_read(&ring, output, 317) == 317);
    assert(memcmp(output, input, 317) == 0);
    assert(ambient_ble_ring_write(&ring, input + 400, 116));
    assert(ring.length == 199);
    assert(ambient_ble_ring_read(&ring, output, 83) == 83);
    assert(memcmp(output, input + 317, 83) == 0);
    assert(ambient_ble_ring_read(&ring, output, sizeof(output)) == 116);
    assert(memcmp(output, input + 400, 116) == 0);

    assert(ambient_ble_ring_write(&ring, input, sizeof(input)));
    assert(!ambient_ble_ring_write(&ring, input, 1));
    assert(ring.length == AMBIENT_BLE_RX_RING_CAPACITY);
    assert(ambient_ble_ring_read(&ring, output, sizeof(output)) == sizeof(output));
    assert(memcmp(output, input, sizeof(input)) == 0);
    assert(!ambient_ble_ring_write(&ring, input, AMBIENT_BLE_RX_RING_CAPACITY + 1U));
    assert(!ambient_ble_ring_write(&ring, NULL, 1));
    assert(!ambient_ble_ring_write(&ring, input, 0));

    ambient_ble_ring_reset(&ring);
    assert(ring.length == 0 && ring.head == 0);
    assert(ambient_ble_ring_read(&ring, output, sizeof(output)) == 0);
    return 0;
}
