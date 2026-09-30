#include "companion_core.h"

#include <string.h>

enum {
    COMPANION_QUOTA_STATE_PATH_MAX_BYTES = 4096,
};

static size_t companion_bounded_string_length(const char *value,
                                               size_t limit)
{
    size_t length = 0;

    if (!value) {
        return 0;
    }
    while (length < limit && value[length] != '\0') {
        length++;
    }
    return length;
}

static bool companion_quota_window_state_equal(
    const ambient_quota_reset_window_state_t *left,
    const ambient_quota_reset_window_state_t *right)
{
    return left->has_baseline == right->has_baseline
        && left->baseline_used_percent == right->baseline_used_percent
        && left->baseline_reset_marker == right->baseline_reset_marker
        && left->highest_reset_marker == right->highest_reset_marker
        && left->has_candidate == right->has_candidate
        && left->candidate_reset_marker == right->candidate_reset_marker;
}

static bool companion_quota_detector_state_equal(
    const ambient_quota_reset_detector_t *left,
    const ambient_quota_reset_detector_t *right)
{
    return companion_quota_window_state_equal(&left->short_window,
                                               &right->short_window)
        && companion_quota_window_state_equal(&left->long_window,
                                               &right->long_window);
}

static void companion_persist_quota_detector_if_changed(
    companion_core_t *core,
    const ambient_quota_reset_detector_t *before,
    bool *write_attempted,
    quota_reset_state_store_result_t *write_result)
{
    if (write_attempted) {
        *write_attempted = false;
    }
    if (write_result) {
        *write_result = QUOTA_RESET_STATE_STORE_INVALID;
    }
    if (!core || !before || !core->quota_reset_state_path
        || companion_quota_detector_state_equal(
               before, &core->rollout_quota_source.reset_detector)) {
        return;
    }

    if (write_attempted) {
        *write_attempted = true;
    }
    if (write_result) {
        *write_result = quota_reset_state_store_save_atomic(
            core->quota_reset_state_path,
            &core->rollout_quota_source.reset_detector);
    } else {
        (void)quota_reset_state_store_save_atomic(
            core->quota_reset_state_path,
            &core->rollout_quota_source.reset_detector);
    }
}

static companion_notification_kind_t companion_event_notification_kind(
    ambient_event_kind_t kind)
{
    switch (kind) {
    case AMBIENT_EVENT_ATTENTION_REQUIRED:
        return COMPANION_NOTIFICATION_ATTENTION;
    case AMBIENT_EVENT_TURN_SUCCEEDED:
        return COMPANION_NOTIFICATION_COMPLETED;
    case AMBIENT_EVENT_TURN_FAILED:
        return COMPANION_NOTIFICATION_ERROR;
    default:
        return COMPANION_NOTIFICATION_NONE;
    }
}

static void companion_emit_quota_reset_notifications(
    companion_core_t *core,
    const ambient_quota_snapshot_t *snapshot,
    uint8_t confirmed_reset_windows_mask,
    int64_t now_unix_seconds)
{
    const struct {
        uint8_t mask;
        uint32_t duration_minutes;
        companion_notification_kind_t kind;
    } windows[] = {
        {
            .mask = AMBIENT_QUOTA_RESET_CONFIRMED_SHORT,
            .duration_minutes = 300,
            .kind = COMPANION_NOTIFICATION_QUOTA_5H_RESET,
        },
        {
            .mask = AMBIENT_QUOTA_RESET_CONFIRMED_LONG,
            .duration_minutes = 10080,
            .kind = COMPANION_NOTIFICATION_QUOTA_WEEK_RESET,
        },
    };

    if (!core || !snapshot || !core->on_notification) {
        return;
    }
    for (size_t i = 0; i < sizeof(windows) / sizeof(windows[0]); i++) {
        const ambient_quota_window_t *window;
        companion_notification_t notification = {0};

        if ((confirmed_reset_windows_mask & windows[i].mask) == 0) {
            continue;
        }
        window = ambient_quota_find(snapshot, windows[i].duration_minutes);
        if (!window || !window->reset_marker_present) {
            continue;
        }

        notification.kind = windows[i].kind;
        notification.payload.quota_reset.window_minutes =
            windows[i].duration_minutes;
        notification.payload.quota_reset.reset_marker = window->reset_marker;
        notification.payload.quota_reset.observed_at_unix_seconds =
            now_unix_seconds;
        core->on_notification(core->notification_context, &notification);
    }
}

static bool companion_identifier_to_key(companion_core_t *core,
                                        companion_identifier_kind_t kind,
                                        const char *identifier,
                                        size_t identifier_length,
                                        ambient_key_t *key)
{
    if (!core || !core->map_identifier || !identifier
        || identifier_length == 0 || !key) {
        return false;
    }
    *key = 0;
    return core->map_identifier(core->identifier_context, kind, identifier,
                                 identifier_length, key)
        && *key != 0;
}

bool companion_core_init(companion_core_t *core,
                         const companion_core_options_t *options)
{
    size_t path_length;

    if (!core || !options || !options->session_slots
        || !options->notification_entries || !options->map_identifier
        || !options->on_notification || options->session_capacity == 0
        || options->notification_capacity == 0
        || options->session_freshness_ms == 0) {
        return false;
    }
    if (options->quota_reset_state_path) {
        path_length = companion_bounded_string_length(
            options->quota_reset_state_path,
            COMPANION_QUOTA_STATE_PATH_MAX_BYTES + 1U);
        if (path_length == 0
            || path_length > COMPANION_QUOTA_STATE_PATH_MAX_BYTES) {
            return false;
        }
    }

    memset(core, 0, sizeof(*core));
    if (!ambient_reducer_init(&core->reducer, options->session_slots,
                              options->session_capacity,
                              options->session_freshness_ms,
                              options->done_hold_ms)
        || !ambient_dedup_init(&core->notification_dedup,
                               options->notification_entries,
                               options->notification_capacity)
        || !rollout_quota_source_init(
               &core->rollout_quota_source,
               options->minimum_quota_reset_drop_percent)) {
        return false;
    }

    core->quota_reset_state_path = options->quota_reset_state_path;
    core->map_identifier = options->map_identifier;
    core->identifier_context = options->identifier_context;
    core->on_notification = options->on_notification;
    core->notification_context = options->notification_context;
    core->quota_state_load_result = QUOTA_RESET_STATE_STORE_INVALID;

    if (core->quota_reset_state_path) {
        core->quota_state_load_result = quota_reset_state_store_load(
            core->quota_reset_state_path,
            &core->rollout_quota_source.reset_detector);
        if (core->quota_state_load_result == QUOTA_RESET_STATE_STORE_INVALID) {
            return false;
        }
    }

    core->initialized = true;
    return true;
}

ambient_reducer_result_t companion_core_apply_event(
    companion_core_t *core,
    const ambient_event_t *event,
    uint64_t now_ms,
    bool *notification_emitted)
{
    ambient_protocol_message_t message = {
        .kind = AMBIENT_MESSAGE_EVENT,
        .event = event,
    };
    ambient_reducer_result_t result;
    companion_notification_kind_t notification_kind;

    if (notification_emitted) {
        *notification_emitted = false;
    }
    if (!core || !core->initialized || !event
        || ambient_protocol_validate_message(&message, 0)
               != AMBIENT_PROTOCOL_VALID) {
        return AMBIENT_REDUCER_INVALID;
    }

    result = ambient_reducer_apply(&core->reducer, event, now_ms);
    notification_kind = companion_event_notification_kind(event->kind);
    if (result != AMBIENT_REDUCER_APPLIED
        || notification_kind == COMPANION_NOTIFICATION_NONE) {
        return result;
    }

    if (ambient_dedup_check_and_add(
            &core->notification_dedup,
            (ambient_notification_key_t){
                .session_key = event->session_key,
                .event_key = event->event_key,
            }) != AMBIENT_DEDUP_EMIT) {
        return result;
    }

    companion_notification_t notification = {
        .kind = notification_kind,
        .payload.lifecycle = {
            .event = *event,
            .observed_at_ms = now_ms,
        },
    };
    core->on_notification(core->notification_context, &notification);
    if (notification_emitted) {
        *notification_emitted = true;
    }
    return result;
}

ambient_reducer_result_t companion_core_ingest_rollout_event(
    companion_core_t *core,
    const rollout_lifecycle_input_t *input,
    uint64_t now_ms,
    bool *notification_emitted)
{
    ambient_key_t session_key;
    ambient_key_t turn_key = 0;
    ambient_event_t event;

    if (notification_emitted) {
        *notification_emitted = false;
    }
    if (!core || !core->initialized || !input || input->source_ordinal == 0
        || !companion_identifier_to_key(
               core, COMPANION_IDENTIFIER_SESSION, input->session_id,
               input->session_id_length, &session_key)) {
        return AMBIENT_REDUCER_INVALID;
    }
    if (input->turn_id || input->turn_id_length > 0) {
        if (!companion_identifier_to_key(
                core, COMPANION_IDENTIFIER_TURN, input->turn_id,
                input->turn_id_length, &turn_key)) {
            return AMBIENT_REDUCER_INVALID;
        }
    }

    event.kind = input->kind;
    event.session_key = session_key;
    event.turn_key = turn_key;
    event.event_key = input->source_ordinal;
    return companion_core_apply_event(core, &event, now_ms,
                                      notification_emitted);
}

rollout_quota_result_t companion_core_update_rollout_quota(
    companion_core_t *core,
    const rollout_rate_limits_input_t *rate_limits,
    int64_t now_unix_seconds,
    companion_quota_update_t *update)
{
    ambient_quota_reset_detector_t before;
    ambient_quota_snapshot_t current = {0};
    uint8_t reset_mask = 0;
    rollout_quota_result_t result;

    if (update) {
        memset(update, 0, sizeof(*update));
        update->source_result = ROLLOUT_QUOTA_INVALID;
        update->state_store_result = QUOTA_RESET_STATE_STORE_INVALID;
    }
    if (!core || !core->initialized || !update) {
        return ROLLOUT_QUOTA_INVALID;
    }

    before = core->rollout_quota_source.reset_detector;
    result = rollout_quota_source_update(
        &core->rollout_quota_source, rate_limits, now_unix_seconds,
        &current, &reset_mask);
    update->source_result = result;
    update->confirmed_reset_windows_mask = reset_mask;
    companion_persist_quota_detector_if_changed(
        core, &before, &update->state_store_write_attempted,
        &update->state_store_result);
    if (result == ROLLOUT_QUOTA_AVAILABLE && reset_mask != 0) {
        companion_emit_quota_reset_notifications(
            core, &current, reset_mask, now_unix_seconds);
    }
    return result;
}

bool companion_core_snapshot(companion_core_t *core,
                             uint64_t now_ms,
                             int64_t now_unix_seconds,
                             companion_snapshot_t *snapshot)
{
    ambient_quota_reset_detector_t before;

    if (!core || !core->initialized || !snapshot || now_unix_seconds < 0) {
        return false;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state_store_result = QUOTA_RESET_STATE_STORE_INVALID;
    if (!ambient_reducer_snapshot(&core->reducer, now_ms,
                                  &snapshot->lifecycle)) {
        return false;
    }
    /* A fresh Companion snapshot reports Codex inactivity as IDLE. */
    if (snapshot->lifecycle.status == AMBIENT_STATUS_OFFLINE) {
        snapshot->lifecycle.status = AMBIENT_STATUS_IDLE;
    }
    snapshot->generated_at_ms = now_ms;

    before = core->rollout_quota_source.reset_detector;
    snapshot->quota_result = rollout_quota_source_current(
        &core->rollout_quota_source, now_unix_seconds, &snapshot->quota);
    companion_persist_quota_detector_if_changed(
        core, &before, &snapshot->state_store_write_attempted,
        &snapshot->state_store_result);
    return true;
}

ambient_transport_result_t companion_core_publish_snapshot(
    companion_core_t *core,
    const ambient_transport_t *transport,
    uint64_t now_ms,
    int64_t now_unix_seconds,
    companion_snapshot_encode_fn encode,
    void *encode_context,
    uint8_t *buffer,
    size_t buffer_capacity)
{
    companion_snapshot_t snapshot;
    size_t encoded_bytes = 0;

    if (!core || !core->initialized || !transport || !encode || !buffer
        || buffer_capacity == 0
        || !companion_core_snapshot(core, now_ms, now_unix_seconds,
                                    &snapshot)
        || !encode(encode_context, &snapshot, buffer, buffer_capacity,
                   &encoded_bytes)) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    if (encoded_bytes == 0) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    if (encoded_bytes > buffer_capacity) {
        return AMBIENT_TRANSPORT_TOO_LARGE;
    }
    return ambient_transport_send(transport, buffer, encoded_bytes);
}

bool companion_core_rollout_context_init(
    companion_core_rollout_context_t *context,
    companion_core_t *core,
    uint64_t now_ms,
    int64_t now_unix_seconds)
{
    if (!context || !core || !core->initialized) {
        return false;
    }
    memset(context, 0, sizeof(*context));
    context->core = core;
    context->now_ms = now_ms;
    context->now_unix_seconds = now_unix_seconds;
    context->last_event_result = AMBIENT_REDUCER_INVALID;
    context->last_quota_update.source_result = ROLLOUT_QUOTA_INVALID;
    context->last_quota_update.state_store_result =
        QUOTA_RESET_STATE_STORE_INVALID;
    return true;
}

void companion_core_rollout_event_callback(
    void *context,
    const rollout_lifecycle_input_t *event)
{
    companion_core_rollout_context_t *poll = context;
    bool notification_emitted = false;

    if (!poll || !poll->core) {
        return;
    }
    poll->last_event_result = companion_core_ingest_rollout_event(
        poll->core, event, poll->now_ms, &notification_emitted);
    if (poll->last_event_result == AMBIENT_REDUCER_APPLIED) {
        poll->events_applied++;
        if (notification_emitted) {
            poll->notifications_emitted++;
        }
    } else {
        poll->events_rejected++;
    }
}

void companion_core_rollout_quota_callback(
    void *context,
    const rollout_rate_limits_input_t *rate_limits)
{
    companion_core_rollout_context_t *poll = context;

    if (!poll || !poll->core) {
        return;
    }
    (void)companion_core_update_rollout_quota(
        poll->core, rate_limits, poll->now_unix_seconds,
        &poll->last_quota_update);
    if (poll->last_quota_update.source_result != ROLLOUT_QUOTA_INVALID) {
        poll->quota_updates++;
        if (poll->last_quota_update.confirmed_reset_windows_mask
            & AMBIENT_QUOTA_RESET_CONFIRMED_SHORT) {
            poll->notifications_emitted++;
        }
        if (poll->last_quota_update.confirmed_reset_windows_mask
            & AMBIENT_QUOTA_RESET_CONFIRMED_LONG) {
            poll->notifications_emitted++;
        }
    }
}
