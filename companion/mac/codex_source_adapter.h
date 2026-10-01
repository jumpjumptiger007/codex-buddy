#pragma once
#include "codex_hook_contract.h"
/* Source-specific decoded metadata only. No generic payload, text or JSON.
 * Synthetic profile is explicit and must never be selected by live ingress.
 * Current Hook docs do not establish source ordinals/outcomes; live profile
 * therefore emits no fact. R2 does not install or parse live Hook payloads. */
typedef enum { CODEX_SOURCE_DISABLED = 0, CODEX_SOURCE_CURRENT_HOOK,
               CODEX_SOURCE_SYNTHETIC } codex_source_profile_t;
typedef struct {
    uint32_t schema;
    uint64_t epoch, ordinal;
    codex_hook_fact_t fact;
    const char *session_id, *turn_id;
    size_t session_length, turn_length;
} codex_source_observation_t;
typedef enum { CODEX_SOURCE_INVALID = 0, CODEX_SOURCE_ACCEPTED,
               CODEX_SOURCE_UNAVAILABLE, CODEX_SOURCE_UNVERIFIED,
               CODEX_SOURCE_UNSUPPORTED } codex_source_result_t;
codex_source_result_t codex_source_adapter_validate(codex_source_profile_t profile,
    const codex_source_observation_t *observation, codex_hook_fact_input_t *fact);
