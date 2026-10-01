#pragma once
#include "identifier_registry.h"
#include "codex_source_adapter.h"
#include "companion_core.h"
typedef struct {
    companion_identifier_registry_t registry;
    uint64_t epoch;
    bool busy;
} companion_ingestion_t;
/* Exactly one owner calls ingestion/retirement; not a concurrent API.
 * Source ordinals are preserved. Arrival order is never guessed. */
bool companion_ingestion_init(companion_ingestion_t *ingestion, uint64_t epoch);
ambient_reducer_result_t companion_ingestion_apply(companion_ingestion_t *ingestion,
    companion_core_t *core, codex_source_profile_t profile,
    const codex_source_observation_t *observation, uint64_t now_ms);
bool companion_ingestion_retire(companion_ingestion_t *ingestion,
    companion_core_t *core, ambient_key_t session, uint64_t now_ms);
