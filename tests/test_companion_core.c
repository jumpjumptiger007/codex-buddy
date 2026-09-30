#define _POSIX_C_SOURCE 200809L

#include "companion_core.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ambient_fake_transport.h"

typedef struct {
    companion_notification_t entries[8];
    size_t count;
} notification_capture_t;

static bool map_identifier(void *context,
                           companion_identifier_kind_t kind,
                           const char *identifier,
                           size_t identifier_length,
                           ambient_key_t *key)
{
    (void)context;
    if (!identifier || !key) {
        return false;
    }
    if (kind == COMPANION_IDENTIFIER_SESSION
        && identifier_length == sizeof("session-a") - 1
        && memcmp(identifier, "session-a", identifier_length) == 0) {
        *key = 11;
        return true;
    }
    if (kind == COMPANION_IDENTIFIER_SESSION
        && identifier_length == sizeof("session-b") - 1
        && memcmp(identifier, "session-b", identifier_length) == 0) {
        *key = 22;
        return true;
    }
    if (kind == COMPANION_IDENTIFIER_SESSION
        && identifier_length == sizeof("session-c") - 1
        && memcmp(identifier, "session-c", identifier_length) == 0) {
        *key = 33;
        return true;
    }
    if (kind == COMPANION_IDENTIFIER_TURN
        && identifier_length == sizeof("turn-a") - 1
        && memcmp(identifier, "turn-a", identifier_length) == 0) {
        *key = 101;
        return true;
    }
    return false;
}

static void capture_notification(void *context,
                                 const companion_notification_t *notification)
{
    notification_capture_t *capture = context;
    assert(capture != NULL);
    assert(notification != NULL);
    assert(capture->count < sizeof(capture->entries) / sizeof(capture->entries[0]));
    capture->entries[capture->count++] = *notification;
}

static companion_core_options_t make_options(
    ambient_session_slot_t *slots,
    size_t slot_capacity,
    ambient_notification_key_t *dedup_entries,
    size_t dedup_capacity,
    notification_capture_t *notifications,
    const char *state_path)
{
    companion_core_options_t options = {
        .session_slots = slots,
        .session_capacity = slot_capacity,
        .notification_entries = dedup_entries,
        .notification_capacity = dedup_capacity,
        .session_freshness_ms = 1000,
        .done_hold_ms = 50,
        .minimum_quota_reset_drop_percent = 15.0,
        .quota_reset_state_path = state_path,
        .map_identifier = map_identifier,
        .on_notification = capture_notification,
        .notification_context = notifications,
    };
    return options;
}

static ambient_event_t make_event(ambient_event_kind_t kind,
                                  ambient_key_t session,
                                  ambient_key_t turn,
                                  ambient_key_t sequence)
{
    ambient_event_t event = {
        .kind = kind,
        .session_key = session,
        .turn_key = turn,
        .event_key = sequence,
    };
    return event;
}

static void test_aggregation_freshness_and_notification_dedup(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[2];
    ambient_notification_key_t dedup_entries[8];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 2, dedup_entries, 8, &notifications, NULL);
    companion_snapshot_t snapshot;
    ambient_event_t event;
    bool notified;

    assert(companion_core_init(&core, &options));
    event = make_event(AMBIENT_EVENT_ATTENTION_REQUIRED, 11, 0, 1);
    assert(companion_core_apply_event(&core, &event, 100, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(notified && notifications.count == 1);
    event = make_event(AMBIENT_EVENT_TURN_STARTED, 22, 201, 1);
    assert(companion_core_apply_event(&core, &event, 101, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified && notifications.count == 1);

    assert(companion_core_snapshot(&core, 101, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_ATTENTION);
    assert(snapshot.lifecycle.fresh_session_count == 2);
    assert(snapshot.lifecycle.attention_session_count == 1);
    assert(snapshot.lifecycle.working_session_count == 1);

    event = make_event(AMBIENT_EVENT_ATTENTION_CLEARED, 11, 0, 2);
    assert(companion_core_apply_event(&core, &event, 102, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(companion_core_snapshot(&core, 102, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_WORKING);

    event = make_event(AMBIENT_EVENT_TURN_SUCCEEDED, 22, 201, 2);
    assert(companion_core_apply_event(&core, &event, 103, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(notified && notifications.count == 2);
    assert(notifications.entries[1].kind == COMPANION_NOTIFICATION_COMPLETED);
    assert(notifications.entries[1].payload.lifecycle.event.kind
           == AMBIENT_EVENT_TURN_SUCCEEDED);
    assert(companion_core_apply_event(&core, &event, 104, &notified)
           == AMBIENT_REDUCER_REPLAY);
    assert(!notified && notifications.count == 2);
    assert(companion_core_snapshot(&core, 104, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_DONE);
    assert(companion_core_snapshot(&core, 153, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
    assert(snapshot.lifecycle.done_session_count == 0);

    /* The dedup ring still suppresses a repeated notice after stale-slot reuse. */
    event = make_event(AMBIENT_EVENT_SESSION_IDLE, 33, 0, 1);
    assert(companion_core_apply_event(&core, &event, 1104, &notified)
           == AMBIENT_REDUCER_APPLIED);
    event = make_event(AMBIENT_EVENT_ATTENTION_REQUIRED, 11, 0, 1);
    assert(companion_core_apply_event(&core, &event, 1105, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified && notifications.count == 2);
    assert(companion_core_snapshot(&core, 2106, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
    assert(snapshot.lifecycle.fresh_session_count == 0);
}

static void write_all(int fd, const char *bytes, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t amount = write(fd, bytes + offset, length - offset);
        assert(amount > 0);
        offset += (size_t)amount;
    }
}

static void make_missing_temp_path(char *path, size_t capacity,
                                   const char *pattern)
{
    char temporary[128];
    int fd;
    size_t length = strlen(pattern);

    assert(length + 1 <= sizeof(temporary));
    memcpy(temporary, pattern, length + 1);
    fd = mkstemp(temporary);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(temporary) == 0);
    assert(strlen(temporary) + 1 <= capacity);
    memcpy(path, temporary, strlen(temporary) + 1);
}

static rollout_rate_limits_input_t changed_quota(void)
{
    rollout_rate_limits_input_t limits = {0};
    memcpy(limits.limit_id, "codex", sizeof("codex"));
    limits.has_primary = true;
    limits.primary = (rollout_quota_window_input_t){
        .used_percent_present = true,
        .used_percent = 50.0,
        .window_minutes_present = true,
        .window_minutes = 300,
        .resets_at_present = true,
        .resets_at = 2100,
    };
    limits.has_secondary = true;
    limits.secondary = (rollout_quota_window_input_t){
        .used_percent_present = true,
        .used_percent = 70.0,
        .window_minutes_present = true,
        .window_minutes = 10080,
        .resets_at_present = true,
        .resets_at = 3000,
    };
    return limits;
}

static bool encode_test_snapshot(void *context,
                                 const companion_snapshot_t *snapshot,
                                 uint8_t *buffer,
                                 size_t buffer_capacity,
                                 size_t *encoded_bytes)
{
    (void)context;
    if (!snapshot || !buffer || !encoded_bytes || buffer_capacity < 3) {
        return false;
    }
    /* Test-only bytes; this is not a product wire schema. */
    buffer[0] = (uint8_t)snapshot->lifecycle.status;
    buffer[1] = snapshot->quota.has_short_window ? 1 : 0;
    buffer[2] = snapshot->quota.has_long_window ? 1 : 0;
    *encoded_bytes = 3;
    return true;
}

static void test_rollout_quota_persistence_snapshot_and_fake_transport(void)
{
    static const char rollout_lines[] =
        "{\"type\":\"session_meta\",\"payload\":{\"session_id\":\"session-a\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":2,\"timestamp\":\"t\",\"payload\":{\"type\":\"turn_aborted\",\"turn_id\":\"turn-a\",\"reason\":\"user\",\"started_at\":1,\"completed_at\":2,\"duration_ms\":1}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":3,\"timestamp\":\"t\",\"payload\":{\"type\":\"codex.rate_limits\",\"rate_limits\":{\"primary\":{\"used_percent\":80,\"window_minutes\":300,\"reset_at\":2000},\"secondary\":{\"used_percent\":70,\"window_minutes\":10080,\"reset_at\":3000}}}}\n";
    char rollout_path[128];
    char state_path[128];
    char line_buffer[2048];
    int fd;
    rollout_watcher_t watcher;
    rollout_watcher_stats_t watcher_stats;
    companion_core_t core;
    ambient_session_slot_t slots[3];
    ambient_notification_key_t dedup_entries[8];
    notification_capture_t notifications = {0};
    companion_core_options_t options;
    companion_core_rollout_context_t poll_context;
    companion_quota_update_t quota_update;
    companion_snapshot_t snapshot;
    ambient_quota_reset_window_state_t weekly_state;
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    uint8_t transport_storage[2 * 8];
    size_t message_lengths[2];
    uint8_t receive_buffer[8];
    uint8_t encode_buffer[8];
    size_t received_bytes = 0;

    make_missing_temp_path(rollout_path, sizeof(rollout_path),
                           "/tmp/companion-core-rollout.XXXXXX");
    make_missing_temp_path(state_path, sizeof(state_path),
                           "/tmp/companion-core-state.XXXXXX");
    fd = open(rollout_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    write_all(fd, rollout_lines, strlen(rollout_lines));
    assert(close(fd) == 0);

    options = make_options(slots, 3, dedup_entries, 8, &notifications,
                           state_path);
    assert(companion_core_init(&core, &options));
    assert(core.quota_state_load_result == QUOTA_RESET_STATE_STORE_NOT_FOUND);
    assert(companion_core_apply_event(
               &core,
               &(ambient_event_t){
                   .kind = AMBIENT_EVENT_TURN_STARTED,
                   .session_key = 11,
                   .turn_key = 101,
                   .event_key = 1,
               },
               50, NULL) == AMBIENT_REDUCER_APPLIED);
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                4096));
    assert(companion_core_rollout_context_init(
        &poll_context, &core, 60, 100));
    assert(rollout_watcher_poll_with_quota(
               &watcher, rollout_path,
               companion_core_rollout_event_callback,
               companion_core_rollout_quota_callback,
               &poll_context, &watcher_stats) == ROLLOUT_WATCHER_OK);
    assert(watcher_stats.emitted_events == 1);
    assert(watcher_stats.emitted_quota_updates == 1);
    assert(poll_context.events_applied == 1);
    assert(poll_context.notifications_emitted == 0);
    assert(notifications.count == 0);
    assert(poll_context.last_quota_update.source_result
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(poll_context.last_quota_update.state_store_write_attempted);
    assert(poll_context.last_quota_update.state_store_result
           == QUOTA_RESET_STATE_STORE_OK);

    assert(companion_core_snapshot(&core, 60, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
    assert(snapshot.lifecycle.fresh_session_count == 1);
    assert(snapshot.quota_result == ROLLOUT_QUOTA_AVAILABLE);
    assert(snapshot.quota.has_short_window && snapshot.quota.has_long_window);
    assert(snapshot.quota.short_window.duration_minutes == 300);
    assert(snapshot.quota.long_window.duration_minutes == 10080);

    rollout_rate_limits_input_t limits = changed_quota();
    assert(companion_core_update_rollout_quota(&core, &limits, 110,
                                               &quota_update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(quota_update.confirmed_reset_windows_mask == 0);
    assert(quota_update.state_store_write_attempted);
    assert(quota_update.state_store_result == QUOTA_RESET_STATE_STORE_OK);
    assert(core.rollout_quota_source.reset_detector.short_window.has_candidate);
    assert(notifications.count == 0);

    /* A fresh core restores only the bounded detector state, not a snapshot. */
    ambient_session_slot_t restarted_slots[3];
    ambient_notification_key_t restarted_dedup[8];
    notification_capture_t restarted_notifications = {0};
    companion_core_t restarted_core;
    options = make_options(restarted_slots, 3, restarted_dedup, 8,
                           &restarted_notifications, state_path);
    assert(companion_core_init(&restarted_core, &options));
    assert(restarted_core.quota_state_load_result == QUOTA_RESET_STATE_STORE_OK);
    assert(!restarted_core.rollout_quota_source.has_snapshot);
    assert(companion_core_update_rollout_quota(
               &restarted_core, &limits, 120, &quota_update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(quota_update.confirmed_reset_windows_mask
           == AMBIENT_QUOTA_RESET_CONFIRMED_SHORT);
    assert(restarted_notifications.count == 1);
    assert(restarted_notifications.entries[0].kind
           == COMPANION_NOTIFICATION_QUOTA_5H_RESET);
    assert(restarted_notifications.entries[0].payload.quota_reset.window_minutes
           == 300);
    assert(restarted_notifications.entries[0].payload.quota_reset.reset_marker
           == 2100);
    assert(!restarted_core.rollout_quota_source.reset_detector
                .short_window.has_candidate);
    weekly_state = restarted_core.rollout_quota_source.reset_detector.long_window;

    /* Confirmed detector state survives restart and does not notify again. */
    ambient_session_slot_t confirmed_restart_slots[3];
    ambient_notification_key_t confirmed_restart_dedup[8];
    notification_capture_t confirmed_restart_notifications = {0};
    companion_core_t confirmed_restart_core;
    options = make_options(confirmed_restart_slots, 3,
                           confirmed_restart_dedup, 8,
                           &confirmed_restart_notifications, state_path);
    assert(companion_core_init(&confirmed_restart_core, &options));
    assert(confirmed_restart_core.quota_state_load_result
           == QUOTA_RESET_STATE_STORE_OK);
    assert(companion_core_update_rollout_quota(
               &confirmed_restart_core, &limits, 130, &quota_update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(quota_update.confirmed_reset_windows_mask == 0);
    assert(confirmed_restart_notifications.count == 0);
    assert(!confirmed_restart_core.rollout_quota_source.reset_detector
                .short_window.has_candidate);

    /* The Companion snapshot is fresh while this Codex session is stale. */
    assert(companion_core_apply_event(
               &restarted_core,
               &(ambient_event_t){
                   .kind = AMBIENT_EVENT_SESSION_IDLE,
                   .session_key = 11,
                   .event_key = 99,
               },
               60, NULL) == AMBIENT_REDUCER_APPLIED);
    assert(companion_core_snapshot(&restarted_core, 1200, 2100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
    assert(snapshot.lifecycle.fresh_session_count == 0);
    assert(snapshot.quota_result == ROLLOUT_QUOTA_AVAILABLE);
    assert(!snapshot.quota.has_short_window && snapshot.quota.has_long_window);
    assert(!restarted_core.rollout_quota_source.reset_detector
                .short_window.has_baseline);
    assert(restarted_core.rollout_quota_source.reset_detector
               .long_window.baseline_used_percent == weekly_state.baseline_used_percent);
    assert(restarted_core.rollout_quota_source.reset_detector
               .long_window.baseline_reset_marker == weekly_state.baseline_reset_marker);

    assert(ambient_fake_transport_init(&fake, transport_storage,
                                       sizeof(transport_storage),
                                       message_lengths, 2, 8, 8,
                                       &transport));
    assert(companion_core_publish_snapshot(
               &restarted_core, &transport, 1201, 2100,
               encode_test_snapshot, NULL, encode_buffer,
               sizeof(encode_buffer)) == AMBIENT_TRANSPORT_DISCONNECTED);
    ambient_fake_transport_set_connected(&fake, true);
    assert(companion_core_publish_snapshot(
               &restarted_core, &transport, 1202, 2100,
               encode_test_snapshot, NULL, encode_buffer,
               sizeof(encode_buffer)) == AMBIENT_TRANSPORT_OK);
    assert(ambient_transport_receive(&transport, receive_buffer,
                                     sizeof(receive_buffer), &received_bytes)
           == AMBIENT_TRANSPORT_OK);
    assert(received_bytes == 3);
    assert(receive_buffer[0] == AMBIENT_STATUS_IDLE);
    assert(receive_buffer[1] == 0 && receive_buffer[2] == 1);

    weekly_state = restarted_core.rollout_quota_source.reset_detector.long_window;
    assert(companion_core_snapshot(&restarted_core, 1203, 2100, &snapshot));
    assert(!snapshot.state_store_write_attempted);
    assert(!restarted_core.rollout_quota_source.reset_detector
                .short_window.has_baseline);
    assert(restarted_core.rollout_quota_source.reset_detector
               .long_window.baseline_reset_marker == weekly_state.baseline_reset_marker);

    assert(unlink(rollout_path) == 0);
    assert(unlink(state_path) == 0);
}

static rollout_rate_limits_input_t make_quota_input(
    bool has_short,
    double short_percent,
    int64_t short_reset,
    bool has_long,
    double long_percent,
    int64_t long_reset)
{
    rollout_rate_limits_input_t limits = {0};

    memcpy(limits.limit_id, "codex", sizeof("codex"));
    limits.has_primary = has_short;
    if (has_short) {
        limits.primary = (rollout_quota_window_input_t){
            .used_percent_present = true,
            .used_percent = short_percent,
            .window_minutes_present = true,
            .window_minutes = 300,
            .resets_at_present = true,
            .resets_at = short_reset,
        };
    }
    limits.has_secondary = has_long;
    if (has_long) {
        limits.secondary = (rollout_quota_window_input_t){
            .used_percent_present = true,
            .used_percent = long_percent,
            .window_minutes_present = true,
            .window_minutes = 10080,
            .resets_at_present = true,
            .resets_at = long_reset,
        };
    }
    return limits;
}

static void test_lifecycle_notification_kinds(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[3];
    ambient_notification_key_t dedup_entries[8];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 3, dedup_entries, 8, &notifications, NULL);
    ambient_event_t event;
    bool notified;

    assert(companion_core_init(&core, &options));
    event = make_event(AMBIENT_EVENT_ATTENTION_REQUIRED, 11, 0, 1);
    assert(companion_core_apply_event(&core, &event, 10, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(notified && notifications.count == 1);
    assert(notifications.entries[0].kind == COMPANION_NOTIFICATION_ATTENTION);
    assert(companion_core_apply_event(&core, &event, 11, &notified)
           == AMBIENT_REDUCER_REPLAY);
    assert(!notified && notifications.count == 1);
    event = make_event(AMBIENT_EVENT_ATTENTION_CLEARED, 11, 0, 2);
    assert(companion_core_apply_event(&core, &event, 12, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified && notifications.count == 1);
    companion_snapshot_t snapshot;
    assert(companion_core_snapshot(&core, 12, 30, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);

    event = make_event(AMBIENT_EVENT_TURN_STARTED, 22, 202, 1);
    assert(companion_core_apply_event(&core, &event, 20, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified);
    event = make_event(AMBIENT_EVENT_TURN_SUCCEEDED, 22, 202, 2);
    assert(companion_core_apply_event(&core, &event, 21, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(notified && notifications.count == 2);
    assert(notifications.entries[1].kind == COMPANION_NOTIFICATION_COMPLETED);
    assert(companion_core_apply_event(&core, &event, 22, &notified)
           == AMBIENT_REDUCER_REPLAY);
    assert(!notified && notifications.count == 2);

    event = make_event(AMBIENT_EVENT_TURN_STARTED, 33, 303, 1);
    assert(companion_core_apply_event(&core, &event, 30, &notified)
           == AMBIENT_REDUCER_APPLIED);
    event = make_event(AMBIENT_EVENT_TURN_FAILED, 33, 303, 2);
    assert(companion_core_apply_event(&core, &event, 31, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(notified && notifications.count == 3);
    assert(notifications.entries[2].kind == COMPANION_NOTIFICATION_ERROR);
    assert(companion_core_apply_event(&core, &event, 32, &notified)
           == AMBIENT_REDUCER_REPLAY);
    assert(!notified && notifications.count == 3);
}

static void test_fresh_companion_without_sessions_is_idle(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[1];
    ambient_notification_key_t dedup_entries[4];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 1, dedup_entries, 4, &notifications, NULL);
    companion_snapshot_t snapshot;

    assert(companion_core_init(&core, &options));
    assert(companion_core_snapshot(&core, 10, 30, &snapshot));
    assert(snapshot.generated_at_ms == 10);
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
    assert(snapshot.lifecycle.fresh_session_count == 0);
}

static void test_aborted_turn_is_silent_and_returns_to_idle(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[1];
    ambient_notification_key_t dedup_entries[4];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 1, dedup_entries, 4, &notifications, NULL);
    companion_snapshot_t snapshot;
    bool notified;

    assert(companion_core_init(&core, &options));
    ambient_event_t event = make_event(AMBIENT_EVENT_TURN_STARTED, 11, 101, 1);
    assert(companion_core_apply_event(&core, &event, 10, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified);
    event = make_event(AMBIENT_EVENT_TURN_ABORTED, 11, 101, 2);
    assert(companion_core_apply_event(&core, &event, 20, &notified)
           == AMBIENT_REDUCER_APPLIED);
    assert(!notified && notifications.count == 0);
    assert(companion_core_snapshot(&core, 20, 30, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_IDLE);
}

static void test_quota_reset_notification_kinds_and_dedup(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[1];
    ambient_notification_key_t dedup_entries[4];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 1, dedup_entries, 4, &notifications, NULL);
    companion_quota_update_t update;
    rollout_rate_limits_input_t limits;

    assert(companion_core_init(&core, &options));
    /* Initial observation and candidate-only update do not notify. */
    limits = make_quota_input(true, 80.0, 1000, false, 0.0, 0);
    assert(companion_core_update_rollout_quota(&core, &limits, 100, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    limits = make_quota_input(true, 50.0, 1100, false, 0.0, 0);
    assert(companion_core_update_rollout_quota(&core, &limits, 101, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(notifications.count == 0);

    /* The next same-boundary observation confirms exactly one short reset. */
    assert(companion_core_update_rollout_quota(&core, &limits, 102, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask
           == AMBIENT_QUOTA_RESET_CONFIRMED_SHORT);
    assert(notifications.count == 1);
    assert(notifications.entries[0].kind
           == COMPANION_NOTIFICATION_QUOTA_5H_RESET);
    assert(notifications.entries[0].payload.quota_reset.window_minutes == 300);
    assert(companion_core_update_rollout_quota(&core, &limits, 103, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(notifications.count == 1);

    /* A separate long-window confirmation has its own notification kind. */
    notification_capture_t long_notifications = {0};
    ambient_session_slot_t long_slots[1];
    ambient_notification_key_t long_dedup[4];
    companion_core_t long_core;
    options = make_options(long_slots, 1, long_dedup, 4,
                           &long_notifications, NULL);
    assert(companion_core_init(&long_core, &options));
    limits = make_quota_input(false, 0.0, 0, true, 70.0, 2000);
    assert(companion_core_update_rollout_quota(&long_core, &limits, 100, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    limits = make_quota_input(false, 0.0, 0, true, 40.0, 2100);
    assert(companion_core_update_rollout_quota(&long_core, &limits, 101, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(long_notifications.count == 0);
    assert(companion_core_update_rollout_quota(&long_core, &limits, 102, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask
           == AMBIENT_QUOTA_RESET_CONFIRMED_LONG);
    assert(long_notifications.count == 1);
    assert(long_notifications.entries[0].kind
           == COMPANION_NOTIFICATION_QUOTA_WEEK_RESET);
    assert(long_notifications.entries[0].payload.quota_reset.window_minutes
           == 10080);

    /* Both independent detector windows confirmed together emit two notices. */
    notification_capture_t both_notifications = {0};
    ambient_session_slot_t both_slots[1];
    ambient_notification_key_t both_dedup[4];
    companion_core_t both_core;
    options = make_options(both_slots, 1, both_dedup, 4,
                           &both_notifications, NULL);
    assert(companion_core_init(&both_core, &options));
    limits = make_quota_input(true, 80.0, 1000, true, 70.0, 2000);
    assert(companion_core_update_rollout_quota(&both_core, &limits, 100, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    limits = make_quota_input(true, 50.0, 1100, true, 40.0, 2100);
    assert(companion_core_update_rollout_quota(&both_core, &limits, 101, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(both_notifications.count == 0);
    assert(companion_core_update_rollout_quota(&both_core, &limits, 102, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask
           == (AMBIENT_QUOTA_RESET_CONFIRMED_SHORT
               | AMBIENT_QUOTA_RESET_CONFIRMED_LONG));
    assert(both_notifications.count == 2);
    assert(both_notifications.entries[0].kind
           == COMPANION_NOTIFICATION_QUOTA_5H_RESET);
    assert(both_notifications.entries[1].kind
           == COMPANION_NOTIFICATION_QUOTA_WEEK_RESET);
    assert(companion_core_update_rollout_quota(&both_core, &limits, 103, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(both_notifications.count == 2);
}

static void test_unavailable_to_available_rebaseline_is_silent(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[1];
    ambient_notification_key_t dedup_entries[4];
    notification_capture_t notifications = {0};
    companion_core_options_t options = make_options(
        slots, 1, dedup_entries, 4, &notifications, NULL);
    companion_quota_update_t update;
    companion_snapshot_t snapshot;
    rollout_rate_limits_input_t limits =
        make_quota_input(true, 80.0, 1000, false, 0.0, 0);

    assert(companion_core_init(&core, &options));
    assert(companion_core_update_rollout_quota(&core, &limits, 100, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(notifications.count == 0);

    assert(companion_core_snapshot(&core, 101, 1000, &snapshot));
    assert(snapshot.quota_result == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!core.rollout_quota_source.has_snapshot);
    assert(!core.rollout_quota_source.reset_detector.short_window.has_baseline);

    limits = make_quota_input(true, 50.0, 1100, false, 0.0, 0);
    assert(companion_core_update_rollout_quota(&core, &limits, 1001, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(notifications.count == 0);
    limits.primary.used_percent = 55.0;
    assert(companion_core_update_rollout_quota(&core, &limits, 1002, &update)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(update.confirmed_reset_windows_mask == 0);
    assert(notifications.count == 0);
}

int main(void)
{
    test_aggregation_freshness_and_notification_dedup();
    test_rollout_quota_persistence_snapshot_and_fake_transport();
    test_fresh_companion_without_sessions_is_idle();
    test_lifecycle_notification_kinds();
    test_aborted_turn_is_silent_and_returns_to_idle();
    test_quota_reset_notification_kinds_and_dedup();
    test_unavailable_to_available_rebaseline_is_silent();
    return 0;
}
