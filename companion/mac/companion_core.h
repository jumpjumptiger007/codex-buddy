#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_dedup.h"
#include "ambient_protocol.h"
#include "ambient_reducer.h"
#include "ambient_transport.h"
#include "ambient_wire.h"
#define COMPANION_NOTIFICATION_DEDUP_MAX 16U
#include "quota_reset_state_store.h"
#include "rollout_quota_source.h"
#include "rollout_watcher.h"

typedef enum {
    COMPANION_IDENTIFIER_SESSION = 1,
    COMPANION_IDENTIFIER_TURN,
} companion_identifier_kind_t;

/*
 * External rollout identifiers are mapped synchronously and are never kept
 * by CompanionCore. The mapper must return stable, unique, nonzero opaque
 * keys for the lifetime of the reducer state.
 */
typedef bool (*companion_identifier_map_fn)(
    void *context,
    companion_identifier_kind_t kind,
    const char *identifier,
    size_t identifier_length,
    ambient_key_t *key);

typedef enum {
    COMPANION_NOTIFICATION_NONE = 0,
    COMPANION_NOTIFICATION_ATTENTION,
    COMPANION_NOTIFICATION_COMPLETED,
    COMPANION_NOTIFICATION_ERROR,
    COMPANION_NOTIFICATION_QUOTA_5H_RESET,
    COMPANION_NOTIFICATION_QUOTA_WEEK_RESET,
} companion_notification_kind_t;

typedef struct {
    companion_notification_kind_t kind;
    union {
        struct {
            ambient_event_t event;
            uint64_t observed_at_ms;
        } lifecycle;
        struct {
            uint32_t window_minutes;
            uint64_t reset_marker;
            int64_t observed_at_unix_seconds;
        } quota_reset;
    } payload;
} companion_notification_t;

/* The notification view is borrowed only for the duration of the callback. */
typedef void (*companion_notification_fn)(
    void *context,
    const companion_notification_t *notification);

typedef struct {
    ambient_session_slot_t *session_slots;
    size_t session_capacity;
    ambient_notification_key_t *notification_entries;
    size_t notification_capacity;
    uint64_t session_freshness_ms;
    uint64_t done_hold_ms;
    double minimum_quota_reset_drop_percent;
    /* NULL disables persistence; otherwise the path must outlive the core. */
    const char *quota_reset_state_path;
    companion_identifier_map_fn map_identifier;
    void *identifier_context;
    companion_notification_fn on_notification;
    void *notification_context;
} companion_core_options_t;

typedef struct {
    rollout_quota_result_t source_result;
    uint8_t confirmed_reset_windows_mask;
    bool state_store_write_attempted;
    quota_reset_state_store_result_t state_store_result;
} companion_quota_update_t;

typedef struct {
    uint64_t generated_at_ms;
    ambient_dashboard_snapshot_t lifecycle;
    rollout_quota_result_t quota_result;
    ambient_quota_snapshot_t quota;
    bool state_store_write_attempted;
    quota_reset_state_store_result_t state_store_result;
} companion_snapshot_t;

typedef struct {
    ambient_reducer_t reducer;
    ambient_dedup_t notification_dedup;
    rollout_quota_source_t rollout_quota_source;
    const char *quota_reset_state_path;
    companion_identifier_map_fn map_identifier;
    void *identifier_context;
    companion_notification_fn on_notification;
    void *notification_context;
    quota_reset_state_store_result_t quota_state_load_result;
    bool initialized;
} companion_core_t;

/*
 * Per-poll callback context for rollout_watcher_poll_with_quota(). Time is
 * supplied by the caller in the reducer's monotonic domain and Unix seconds.
 */
typedef struct {
    companion_core_t *core;
    uint64_t now_ms;
    int64_t now_unix_seconds;
    size_t events_applied;
    size_t events_rejected;
    size_t notifications_emitted;
    size_t quota_updates;
    ambient_reducer_result_t last_event_result;
    companion_quota_update_t last_quota_update;
} companion_core_rollout_context_t;

bool companion_core_init(companion_core_t *core,
                         const companion_core_options_t *options);

ambient_reducer_result_t companion_core_apply_event(
    companion_core_t *core,
    const ambient_event_t *event,
    uint64_t now_ms,
    bool *notification_emitted);

ambient_reducer_result_t companion_core_ingest_rollout_event(
    companion_core_t *core,
    const rollout_lifecycle_input_t *input,
    uint64_t now_ms,
    bool *notification_emitted);

rollout_quota_result_t companion_core_update_rollout_quota(
    companion_core_t *core,
    const rollout_rate_limits_input_t *rate_limits,
    int64_t now_unix_seconds,
    companion_quota_update_t *update);

bool companion_core_snapshot(companion_core_t *core,
                             uint64_t now_ms,
                             int64_t now_unix_seconds,
                             companion_snapshot_t *snapshot);

/* Product projection contains only approved numeric semantic fields. */
/* Notification carries no raw source content; queue overflow drops this notice
 * and never changes persistent core truth. */
bool companion_core_queue_notification(ambient_wire_session_t *session,
                                       const companion_notification_t *notification);
bool companion_core_wire_snapshot(const companion_snapshot_t *snapshot,
                                  ambient_wire_snapshot_t *wire);
ambient_transport_result_t companion_core_publish_snapshot(
    companion_core_t *core, ambient_wire_session_t *session,
    const ambient_transport_t *transport, uint64_t now_ms,
    int64_t now_unix_seconds);

bool companion_core_rollout_context_init(
    companion_core_rollout_context_t *context,
    companion_core_t *core,
    uint64_t now_ms,
    int64_t now_unix_seconds);

/* These callbacks can be passed directly to rollout_watcher_poll_with_quota. */
void companion_core_rollout_event_callback(
    void *context,
    const rollout_lifecycle_input_t *event);
void companion_core_rollout_quota_callback(
    void *context,
    const rollout_rate_limits_input_t *rate_limits);
