#pragma once
#include "codex_source_adapter.h"
#include "quota_reset_state_store.h"
#define COMPANION_DIAGNOSTICS_MAX_BYTES 128U
typedef struct {
    uint32_t accepted, rejected, queue_dropped, notice_dropped, source_losses;
    uint32_t persistence_failures;
    uint8_t queued, sessions, turns, candidate_mask, confirmed_mask;
    bool running, source_available, persistence_degraded;
    codex_source_profile_t profile;
    quota_reset_state_store_result_t load_result, write_result;
    uint64_t short_marker, long_marker;
} companion_diagnostics_t;
void companion_diagnostic_increment(uint32_t *counter);
/* Fixed numeric record; no generic content fields or format string. */
bool companion_diagnostics_encode(const companion_diagnostics_t *diagnostics,
                                  char *output, size_t capacity, size_t *length);
