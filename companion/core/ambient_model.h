#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t ambient_key_t;

typedef enum {
    AMBIENT_STATUS_OFFLINE = 0,
    AMBIENT_STATUS_IDLE,
    AMBIENT_STATUS_WORKING,
    AMBIENT_STATUS_ATTENTION,
    AMBIENT_STATUS_DONE,
} ambient_status_t;

typedef enum {
    AMBIENT_OUTCOME_NONE = 0,
    AMBIENT_OUTCOME_SUCCESS,
    AMBIENT_OUTCOME_FAILED,
    AMBIENT_OUTCOME_ABORTED,
} ambient_outcome_t;

typedef enum {
    AMBIENT_EVENT_INVALID = 0,
    AMBIENT_EVENT_SESSION_IDLE,
    AMBIENT_EVENT_TURN_STARTED,
    AMBIENT_EVENT_TURN_SUCCEEDED,
    AMBIENT_EVENT_TURN_FAILED,
    AMBIENT_EVENT_TURN_ABORTED,
    AMBIENT_EVENT_ATTENTION_REQUIRED,
    AMBIENT_EVENT_ATTENTION_CLEARED,
} ambient_event_kind_t;

/*
 * Map external identifiers before entering core. session_key and turn_key are
 * opaque local keys. event_key is a local event identity and sequence, strictly
 * increasing within one session so the reducer can reject replay/out-of-order
 * events without comparing unrelated sessions.
 */
typedef struct {
    ambient_event_kind_t kind;
    ambient_key_t session_key;
    ambient_key_t turn_key;
    ambient_key_t event_key;
} ambient_event_t;

/*
 * Reset markers keep source-defined units, but must be ordered: a greater
 * marker represents a later reset boundary.
 */
typedef struct {
    uint32_t duration_minutes;
    bool used_percent_present;
    double used_percent;
    bool reset_marker_present;
    uint64_t reset_marker;
} ambient_quota_window_t;

typedef struct {
    bool has_short_window;
    bool has_long_window;
    ambient_quota_window_t short_window;
    ambient_quota_window_t long_window;
} ambient_quota_snapshot_t;
