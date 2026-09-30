#include <assert.h>
#include <stdint.h>

#include "ambient_dedup.h"

static ambient_notification_key_t key(ambient_key_t session,
                                      ambient_key_t event)
{
    ambient_notification_key_t value = {
        .session_key = session,
        .event_key = event,
    };
    return value;
}

int main(void)
{
    ambient_dedup_t dedup;
    ambient_notification_key_t entries[2];

    assert(!ambient_dedup_init(&dedup, entries, 0));
    assert(!ambient_dedup_init(&dedup, entries, SIZE_MAX));
    assert(ambient_dedup_init(&dedup, entries, 2));
    assert(ambient_dedup_check_and_add(&dedup, key(0, 1))
           == AMBIENT_DEDUP_INVALID);
    assert(ambient_dedup_check_and_add(&dedup, key(1, 10))
           == AMBIENT_DEDUP_EMIT);
    assert(ambient_dedup_check_and_add(&dedup, key(1, 10))
           == AMBIENT_DEDUP_SUPPRESS);
    assert(ambient_dedup_check_and_add(&dedup, key(2, 10))
           == AMBIENT_DEDUP_EMIT);
    assert(ambient_dedup_check_and_add(&dedup, key(3, 10))
           == AMBIENT_DEDUP_EMIT);
    assert(ambient_dedup_check_and_add(&dedup, key(1, 10))
           == AMBIENT_DEDUP_EMIT);
    assert(ambient_dedup_check_and_add(&dedup, key(2, 10))
           == AMBIENT_DEDUP_EMIT);
    return 0;
}
