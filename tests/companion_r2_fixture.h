#pragma once
#include "companion_runtime.h"
#include <assert.h>
#include <string.h>
static inline companion_runtime_options_t r2_options(companion_runtime_lease_t *lease,
                                                      uint64_t epoch, const char *path)
{
    return (companion_runtime_options_t){.lease=lease, .epoch=epoch,
        .freshness_ms=100, .done_hold_ms=20, .profile=CODEX_SOURCE_SYNTHETIC,
        .quota_state_path=path};
}
static inline codex_source_observation_t r2_observation(uint64_t epoch,
    const char *session, const char *turn, uint64_t sequence, codex_hook_fact_t fact)
{
    return (codex_source_observation_t){.schema=1, .epoch=epoch, .ordinal=sequence,
        .fact=fact, .session_id=session, .session_length=strlen(session),
        .turn_id=turn, .turn_length=turn ? strlen(turn) : 0};
}
static inline ambient_reducer_result_t r2_apply(companion_runtime_t *r,
    const char *session, const char *turn, uint64_t sequence, codex_hook_fact_t fact, uint64_t now)
{
    codex_source_observation_t o = r2_observation(r->ingestion.epoch, session, turn, sequence, fact);
    return companion_ingestion_apply(&r->ingestion, &r->core, CODEX_SOURCE_SYNTHETIC, &o, now);
}
static inline ambient_status_t r2_status(companion_runtime_t *r, uint64_t now)
{
    companion_snapshot_t snapshot; assert(companion_core_snapshot(&r->core, now, 1, &snapshot));
    return snapshot.lifecycle.status;
}
