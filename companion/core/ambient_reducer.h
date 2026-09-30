#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_model.h"

typedef struct {
    bool occupied;
    ambient_key_t session_key;
    ambient_key_t last_event_key;
    ambient_key_t active_turn_key;
    ambient_status_t status;
    ambient_outcome_t last_outcome;
    uint64_t last_seen_ms;
    uint64_t done_at_ms;
} ambient_session_slot_t;

typedef struct {
    ambient_session_slot_t *slots;
    size_t slot_capacity;
    uint64_t freshness_ms;
    uint64_t done_hold_ms;
    uint64_t last_now_ms;
    bool has_now;
} ambient_reducer_t;

typedef struct {
    ambient_key_t session_key;
    ambient_key_t active_turn_key;
    ambient_status_t status;
    ambient_outcome_t last_outcome;
    uint64_t last_seen_ms;
} ambient_session_view_t;

typedef struct {
    ambient_status_t status;
    size_t fresh_session_count;
    size_t working_session_count;
    size_t attention_session_count;
    size_t done_session_count;
    bool clock_regressed;
} ambient_dashboard_snapshot_t;

typedef enum {
    AMBIENT_REDUCER_INVALID = 0,
    AMBIENT_REDUCER_APPLIED,
    AMBIENT_REDUCER_REPLAY,
    AMBIENT_REDUCER_NO_CAPACITY,
    AMBIENT_REDUCER_STALE_TURN,
} ambient_reducer_result_t;

bool ambient_reducer_init(ambient_reducer_t *reducer,
                          ambient_session_slot_t *slots,
                          size_t slot_capacity,
                          uint64_t freshness_ms,
                          uint64_t done_hold_ms);

ambient_reducer_result_t ambient_reducer_apply(ambient_reducer_t *reducer,
                                               const ambient_event_t *event,
                                               uint64_t now_ms);

bool ambient_reducer_get_session(ambient_reducer_t *reducer,
                                 ambient_key_t session_key,
                                 uint64_t now_ms,
                                 ambient_session_view_t *view,
                                 bool *clock_regressed);

bool ambient_reducer_snapshot(ambient_reducer_t *reducer,
                              uint64_t now_ms,
                              ambient_dashboard_snapshot_t *snapshot);
