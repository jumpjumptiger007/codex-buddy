#pragma once

#include <stddef.h>

#include "ambient_model.h"

typedef struct {
    ambient_key_t session_key;
    ambient_key_t event_key;
} ambient_notification_key_t;

typedef struct {
    ambient_notification_key_t *entries;
    size_t capacity;
    size_t count;
    size_t oldest_index;
} ambient_dedup_t;

typedef enum {
    AMBIENT_DEDUP_INVALID = 0,
    AMBIENT_DEDUP_EMIT,
    AMBIENT_DEDUP_SUPPRESS,
} ambient_dedup_result_t;

bool ambient_dedup_init(ambient_dedup_t *dedup,
                        ambient_notification_key_t *entries,
                        size_t capacity);

ambient_dedup_result_t ambient_dedup_check_and_add(
    ambient_dedup_t *dedup,
    ambient_notification_key_t key);
