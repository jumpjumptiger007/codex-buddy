#include "companion_runtime.h"
#include <string.h>
_Static_assert(sizeof(companion_runtime_record_t) <= 336, "bounded source record");
_Static_assert(sizeof(companion_runtime_t) <= 16384, "host runtime budget");
static bool active(const companion_runtime_t *r)
{
    return r && r->running && r->options.lease
        && r->options.lease->owner == r
        && r->options.lease->highest_epoch == r->ingestion.epoch;
}
/* A selected live Hook profile is not a ready source adapter. At present only
 * the explicit synthetic host profile can deliver accepted observations. */
static bool source_profile_ready(codex_source_profile_t profile)
{
    return profile == CODEX_SOURCE_SYNTHETIC;
}
static bool unused_mapper(void *c, companion_identifier_kind_t k,
                          const char *s, size_t n, ambient_key_t *key)
{
    (void)c; (void)k; (void)s; (void)n;
    if (key) *key = 0;
    return false; /* Raw rollout bypass is deliberately unavailable. */
}
static void notice(void *context, const companion_notification_t *n)
{
    companion_runtime_t *r = context;
    ambient_wire_notice_code_t code;
    switch (n->kind) {
    case COMPANION_NOTIFICATION_ATTENTION: code = AMBIENT_NOTICE_ATTENTION; break;
    case COMPANION_NOTIFICATION_COMPLETED: code = AMBIENT_NOTICE_COMPLETED; break;
    case COMPANION_NOTIFICATION_ERROR: code = AMBIENT_NOTICE_ERROR; break;
    case COMPANION_NOTIFICATION_QUOTA_5H_RESET: code = AMBIENT_NOTICE_SHORT_RESET; break;
    case COMPANION_NOTIFICATION_QUOTA_WEEK_RESET: code = AMBIENT_NOTICE_LONG_RESET; break;
    default: return;
    }
    if (r->pending_count == COMPANION_RUNTIME_NOTICE_DEPTH) {
        companion_diagnostic_increment(&r->diagnostics.notice_dropped); return;
    }
    r->pending[r->pending_count++] = code;
}
static void persistence(companion_runtime_t *r, bool attempted,
                        quota_reset_state_store_result_t result)
{
    if (!attempted && r->diagnostics.persistence_degraded && r->core.quota_reset_state_path) {
        attempted = true;
        result = quota_reset_state_store_save_atomic(r->core.quota_reset_state_path,
            &r->core.rollout_quota_source.reset_detector);
    }
    if (!attempted) return;
    r->diagnostics.write_result = result;
    r->diagnostics.persistence_degraded = result != QUOTA_RESET_STATE_STORE_OK;
    if (r->diagnostics.persistence_degraded)
        companion_diagnostic_increment(&r->diagnostics.persistence_failures);
}
static void refresh(companion_runtime_t *r)
{
    r->diagnostics.queued = r->queue_count;
    r->diagnostics.sessions = r->ingestion.registry.session_count;
    r->diagnostics.turns = r->ingestion.registry.turn_count;
    ambient_quota_reset_detector_t *d = &r->core.rollout_quota_source.reset_detector;
    r->diagnostics.candidate_mask = (d->short_window.has_candidate ? 1 : 0)
        | (d->long_window.has_candidate ? 2 : 0);
    r->diagnostics.short_marker = d->short_window.highest_reset_marker;
    r->diagnostics.long_marker = d->long_window.highest_reset_marker;
}
bool companion_runtime_start(companion_runtime_t *r, const companion_runtime_options_t *o)
{
    if (!r || !o || !o->lease || !o->epoch || !o->freshness_ms
        || o->profile < CODEX_SOURCE_DISABLED || o->profile > CODEX_SOURCE_SYNTHETIC
        || (!!o->start_source != !!o->stop_source)) return false;
    if (r->running) return o->lease->owner == r && r->options.epoch == o->epoch;
    if (o->lease->owner || o->epoch <= o->lease->highest_epoch) return false;
    memset(r, 0, sizeof(*r)); r->options = *o; o->lease->owner = r;
    companion_core_options_t options = {.session_slots=r->slots,
        .session_capacity=AMBIENT_WIRE_MAX_SESSIONS, .notification_entries=r->dedup,
        .notification_capacity=COMPANION_NOTIFICATION_DEDUP_MAX,
        .session_freshness_ms=o->freshness_ms, .done_hold_ms=o->done_hold_ms,
        .minimum_quota_reset_drop_percent=AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT,
        .quota_reset_state_path=o->quota_state_path, .map_identifier=unused_mapper,
        .on_notification=notice, .notification_context=r};
    if (!companion_ingestion_init(&r->ingestion, o->epoch)
        || !companion_core_init(&r->core, &options)) {
        o->lease->owner = NULL; return false;
    }
    r->diagnostics.load_result = r->core.quota_state_load_result;
    r->diagnostics.profile = o->profile;
    if (o->start_source && !o->start_source(o->source_context)) {
        o->stop_source(o->source_context); /* Roll back partial worker start. */
        r->core.initialized = false; o->lease->owner = NULL; return false;
    }
    o->lease->highest_epoch = o->epoch;
    r->running = r->diagnostics.running = true;
    r->source_available = r->diagnostics.source_available = source_profile_ready(o->profile);
    return true;
}
void companion_runtime_link_lost(companion_runtime_t *r)
{
    if (!r) return;
    memset(&r->wire, 0, sizeof(r->wire));
    memset(r->pending, 0, sizeof(r->pending)); r->pending_count = 0;
}
void companion_runtime_stop(companion_runtime_t *r)
{
    if (!r || !r->running) return;
    if (active(r) && r->options.stop_source) r->options.stop_source(r->options.source_context);
    if (r->options.lease->owner == r) r->options.lease->owner = NULL;
    companion_runtime_link_lost(r);
    memset(r->queue, 0, sizeof(r->queue)); r->queue_count = 0;
    memset(r->slots, 0, sizeof(r->slots)); memset(&r->ingestion, 0, sizeof(r->ingestion));
    r->core.initialized = false; r->running = r->source_available = false;
    r->diagnostics.running = r->diagnostics.source_available = false;
    r->diagnostics.queued = r->diagnostics.sessions = r->diagnostics.turns = 0;
}
bool companion_runtime_source_recovered(companion_runtime_t *r, uint64_t epoch)
{
    if (!active(r) || epoch != r->ingestion.epoch
        || !source_profile_ready(r->options.profile)) return false;
    r->source_available = r->diagnostics.source_available = true; return true;
}
bool companion_runtime_enqueue(companion_runtime_t *r, const codex_source_observation_t *o)
{
    codex_hook_fact_input_t f;
    if (!active(r) || !r->source_available || !o
        || o->epoch != r->ingestion.epoch
        || codex_source_adapter_validate(r->options.profile, o, &f) != CODEX_SOURCE_ACCEPTED) {
        if (r) companion_diagnostic_increment(&r->diagnostics.rejected);
        return false;
    }
    if (r->queue_count == COMPANION_RUNTIME_QUEUE_DEPTH) {
        companion_diagnostic_increment(&r->diagnostics.queue_dropped); return false;
    }
    companion_runtime_record_t *record = &r->queue[r->queue_count++];
    memset(record, 0, sizeof(*record)); record->observation = *o;
    memcpy(record->session, o->session_id, o->session_length);
    if (o->turn_length) memcpy(record->turn, o->turn_id, o->turn_length);
    /* Pointers are rebound at drain, so FIFO moves cannot retain stale spans. */
    record->observation.session_id = record->observation.turn_id = NULL;
    refresh(r); return true;
}
size_t companion_runtime_drain(companion_runtime_t *r, uint64_t now)
{
    if (!active(r)) return 0;
    size_t applied = 0;
    for (size_t j = 0; j < r->queue_count; j++) {
        companion_runtime_record_t *record = &r->queue[j];
        codex_source_observation_t o = record->observation;
        o.session_id = record->session; o.turn_id = o.turn_length ? record->turn : NULL;
        if (companion_ingestion_apply(&r->ingestion, &r->core, r->options.profile,
                                      &o, now) == AMBIENT_REDUCER_APPLIED) {
            ++applied; companion_diagnostic_increment(&r->diagnostics.accepted);
        } else companion_diagnostic_increment(&r->diagnostics.rejected);
    }
    memset(r->queue, 0, sizeof(r->queue)); r->queue_count = 0; refresh(r); return applied;
}
rollout_quota_result_t companion_runtime_quota(companion_runtime_t *r,
    const rollout_rate_limits_input_t *input, int64_t now)
{
    if (!active(r)) return ROLLOUT_QUOTA_INVALID;
    companion_quota_update_t u;
    rollout_quota_result_t result = companion_core_update_rollout_quota(&r->core, input, now, &u);
    r->diagnostics.confirmed_mask = u.confirmed_reset_windows_mask;
    persistence(r, u.state_store_write_attempted, u.state_store_result);
    refresh(r); return result;
}
void companion_runtime_source_lost(companion_runtime_t *r, int64_t now)
{
    if (!active(r)) return;
    r->source_available = r->diagnostics.source_available = false;
    memset(r->queue, 0, sizeof(r->queue)); r->queue_count = 0;
    /* Notices not yet attached to a successfully published snapshot belong to
     * the source truth that was just lost. Invalidate only this R2 buffer;
     * already queued wire notices retain the frozen R1 supersession rules. */
    memset(r->pending, 0, sizeof(r->pending)); r->pending_count = 0;
    companion_diagnostic_increment(&r->diagnostics.source_losses);
    (void)companion_runtime_quota(r, NULL, now); refresh(r);
}
bool companion_runtime_link_ready(companion_runtime_t *r, uint64_t generation,
                                  const ambient_wire_message_t *hello)
{
    if (!active(r) || generation <= r->last_link_generation || !generation) return false;
    ambient_wire_session_t next;
    if (!ambient_wire_session_init(&next, generation, AMBIENT_WIRE_CAP_KNOWN, AMBIENT_WIRE_CAP_REQUIRED)
        || !ambient_wire_negotiate(&next, hello)) return false;
    companion_runtime_link_lost(r); r->wire = next; r->last_link_generation = generation; return true;
}
ambient_transport_result_t companion_runtime_publish(companion_runtime_t *r,
    const ambient_transport_t *transport, uint64_t now_ms, int64_t now_unix)
{
    if (!active(r)) return AMBIENT_TRANSPORT_INVALID;
    companion_snapshot_t snapshot; ambient_wire_snapshot_t wire;
    if (!companion_core_snapshot(&r->core, now_ms, now_unix, &snapshot)
        || !companion_core_wire_snapshot(&snapshot, &wire)) return AMBIENT_TRANSPORT_INVALID;
    persistence(r, snapshot.state_store_write_attempted, snapshot.state_store_result);
    refresh(r);
    ambient_transport_result_t result = ambient_wire_send_snapshot(&r->wire, &wire, transport);
    if (result == AMBIENT_TRANSPORT_OK) {
        for (size_t i = 0; i < r->pending_count; ++i)
            if (!ambient_wire_queue_notice(&r->wire, r->pending[i]))
                companion_diagnostic_increment(&r->diagnostics.notice_dropped);
        memset(r->pending, 0, sizeof(r->pending)); r->pending_count = 0;
    }
    return result;
}
typedef struct { companion_runtime_t *runtime; int64_t now; } poll_context_t;
static void rollout_quota(void *context, const rollout_rate_limits_input_t *input)
{
    poll_context_t *p = context; (void)companion_runtime_quota(p->runtime, input, p->now);
}
static void rollout_event(void *context, const rollout_lifecycle_input_t *input)
{
    poll_context_t *p = context;
    (void)input; /* Abort-only stream cannot establish an active turn. */
    companion_diagnostic_increment(&p->runtime->diagnostics.rejected);
}
rollout_watcher_result_t companion_runtime_poll_rollout(companion_runtime_t *r,
    rollout_watcher_t *watcher, const char *path, uint64_t now_ms, int64_t now,
    rollout_watcher_stats_t *stats)
{
    (void)now_ms;
    if (!active(r)) return ROLLOUT_WATCHER_INVALID_ARGUMENT;
    poll_context_t context = {.runtime=r, .now=now};
    rollout_watcher_result_t result = rollout_watcher_poll_with_quota(watcher, path,
        rollout_event, rollout_quota, &context, stats);
    if (result != ROLLOUT_WATCHER_OK) companion_runtime_source_lost(r, now);
    return result;
}
