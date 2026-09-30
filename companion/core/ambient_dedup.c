#include "ambient_dedup.h"

#include <stdint.h>
#include <string.h>

bool ambient_dedup_init(ambient_dedup_t *dedup,
                        ambient_notification_key_t *entries,
                        size_t capacity)
{
    if (!dedup || !entries || capacity == 0
        || capacity > SIZE_MAX / sizeof(*entries)) {
        return false;
    }
    memset(entries, 0, sizeof(*entries) * capacity);
    dedup->entries = entries;
    dedup->capacity = capacity;
    dedup->count = 0;
    dedup->oldest_index = 0;
    return true;
}

ambient_dedup_result_t ambient_dedup_check_and_add(
    ambient_dedup_t *dedup,
    ambient_notification_key_t key)
{
    if (!dedup || !dedup->entries || dedup->capacity == 0
        || key.session_key == 0 || key.event_key == 0) {
        return AMBIENT_DEDUP_INVALID;
    }

    for (size_t i = 0; i < dedup->count; ++i) {
        size_t index = (dedup->oldest_index + i) % dedup->capacity;
        if (dedup->entries[index].session_key == key.session_key
            && dedup->entries[index].event_key == key.event_key) {
            return AMBIENT_DEDUP_SUPPRESS;
        }
    }

    if (dedup->count < dedup->capacity) {
        size_t index = (dedup->oldest_index + dedup->count) % dedup->capacity;
        dedup->entries[index] = key;
        dedup->count++;
    } else {
        dedup->entries[dedup->oldest_index] = key;
        dedup->oldest_index = (dedup->oldest_index + 1) % dedup->capacity;
    }
    return AMBIENT_DEDUP_EMIT;
}
