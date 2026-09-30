#include <assert.h>
#include <string.h>

#include "ambient_fake_transport.h"

int main(void)
{
    uint8_t storage[2 * 4];
    size_t lengths[2];
    uint8_t received[4];
    const uint8_t first[] = {1, 2};
    const uint8_t second[] = {3};
    const uint8_t too_large[] = {1, 2, 3, 4, 5};
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    size_t received_bytes = 999;

    assert(!ambient_fake_transport_init(&fake, storage, sizeof(storage),
                                        lengths, 2, 4, 5, &transport));
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage),
                                      lengths, 2, 4, 4, &transport));
    assert(ambient_transport_send(&transport, first, sizeof(first))
           == AMBIENT_TRANSPORT_DISCONNECTED);
    ambient_fake_transport_set_connected(&fake, true);
    assert(ambient_transport_send(&transport, too_large, sizeof(too_large))
           == AMBIENT_TRANSPORT_TOO_LARGE);
    assert(ambient_transport_send(&transport, first, sizeof(first))
           == AMBIENT_TRANSPORT_OK);
    assert(ambient_transport_send(&transport, second, sizeof(second))
           == AMBIENT_TRANSPORT_OK);
    assert(ambient_transport_send(&transport, second, sizeof(second))
           == AMBIENT_TRANSPORT_WOULD_BLOCK);
    assert(ambient_transport_receive(&transport, received, sizeof(received),
                                     &received_bytes)
           == AMBIENT_TRANSPORT_OK);
    assert(received_bytes == sizeof(first));
    assert(memcmp(received, first, sizeof(first)) == 0);
    assert(ambient_transport_receive(&transport, received, sizeof(received),
                                     &received_bytes)
           == AMBIENT_TRANSPORT_OK);
    assert(received_bytes == sizeof(second) && received[0] == second[0]);
    assert(ambient_transport_receive(&transport, received, sizeof(received),
                                     &received_bytes)
           == AMBIENT_TRANSPORT_WOULD_BLOCK);
    assert(received_bytes == 0);
    assert(ambient_transport_send(&transport, NULL, 1)
           == AMBIENT_TRANSPORT_INVALID);
    ambient_fake_transport_set_connected(&fake, false);
    assert(ambient_transport_receive(&transport, received, sizeof(received),
                                     &received_bytes)
           == AMBIENT_TRANSPORT_DISCONNECTED);
    assert(received_bytes == 0);
    return 0;
}
