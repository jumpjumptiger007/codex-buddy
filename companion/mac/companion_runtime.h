#pragma once
#include "companion_ingestion.h"
#include "companion_diagnostics.h"
#define COMPANION_RUNTIME_QUEUE_DEPTH 8U
#define COMPANION_RUNTIME_NOTICE_DEPTH AMBIENT_WIRE_NOTICE_DEPTH
/* Project-local ownership lease, injected into tests. Not an OS process lock.
 * Owner must stop before destruction; explicit crash release models process loss.
 * highest_epoch survives runtime replacement; startup rejects old source streams. */
typedef struct { const void *owner; uint64_t highest_epoch; } companion_runtime_lease_t;
typedef bool (*companion_runtime_start_fn)(void *context);
typedef void (*companion_runtime_stop_fn)(void *context);
typedef struct {
    companion_runtime_lease_t *lease;
    uint64_t epoch, freshness_ms, done_hold_ms;
    codex_source_profile_t profile;
    const char *quota_state_path;
    companion_runtime_start_fn start_source;
    companion_runtime_stop_fn stop_source;
    void *source_context;
} companion_runtime_options_t;
typedef struct {
    codex_source_observation_t observation;
    char session[CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES];
    char turn[CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES];
} companion_runtime_record_t;
typedef struct {
    bool running, source_available;
    companion_runtime_options_t options;
    companion_ingestion_t ingestion;
    companion_core_t core;
    ambient_session_slot_t slots[AMBIENT_WIRE_MAX_SESSIONS];
    ambient_notification_key_t dedup[COMPANION_NOTIFICATION_DEDUP_MAX];
    companion_runtime_record_t queue[COMPANION_RUNTIME_QUEUE_DEPTH];
    size_t queue_count;
    ambient_wire_notice_code_t pending[COMPANION_RUNTIME_NOTICE_DEPTH];
    size_t pending_count;
    ambient_wire_session_t wire;
    uint64_t last_link_generation;
    companion_diagnostics_t diagnostics;
} companion_runtime_t;
/* Zero-initialize runtime and lease before first start. One serialized owner. */
bool companion_runtime_start(companion_runtime_t *runtime,
                             const companion_runtime_options_t *options);
void companion_runtime_stop(companion_runtime_t *runtime);
void companion_runtime_source_lost(companion_runtime_t *runtime, int64_t now_unix);
/* Recovery retains ordering and retired identities; no epoch reset. */
bool companion_runtime_source_recovered(companion_runtime_t *runtime, uint64_t epoch);
bool companion_runtime_enqueue(companion_runtime_t *runtime,
                               const codex_source_observation_t *observation);
size_t companion_runtime_drain(companion_runtime_t *runtime, uint64_t now_ms);
rollout_quota_result_t companion_runtime_quota(companion_runtime_t *runtime,
    const rollout_rate_limits_input_t *input, int64_t now_unix);
/* R3 must call only after its security predicate. Host tests simulate it.
 * R2 performs no security/authentication/BLE. New generation must increase for
 * this runtime object; R3 additionally owns uniqueness across process death. */
bool companion_runtime_link_ready(companion_runtime_t *runtime, uint64_t generation,
                                  const ambient_wire_message_t *hello);
void companion_runtime_link_lost(companion_runtime_t *runtime);
ambient_transport_result_t companion_runtime_publish(companion_runtime_t *runtime,
    const ambient_transport_t *transport, uint64_t now_ms, int64_t now_unix);
rollout_watcher_result_t companion_runtime_poll_rollout(companion_runtime_t *runtime,
    rollout_watcher_t *watcher, const char *path, uint64_t now_ms, int64_t now_unix,
    rollout_watcher_stats_t *stats);
