#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_model.h"

#define CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES 127U

/* These are normalized semantic facts, not verified Codex Hook event names. */
typedef enum {
    CODEX_HOOK_SOURCE_UNAVAILABLE = 0,
    CODEX_HOOK_SOURCE_UNVERIFIED,
    CODEX_HOOK_SOURCE_VERIFIED,
} codex_hook_source_state_t;

typedef enum {
    CODEX_HOOK_FACT_NON_LIFECYCLE = 0,
    CODEX_HOOK_FACT_SESSION_IDLE,
    CODEX_HOOK_FACT_TURN_STARTED,
    CODEX_HOOK_FACT_TURN_SUCCEEDED,
    CODEX_HOOK_FACT_TURN_FAILED,
    CODEX_HOOK_FACT_TURN_ABORTED,
    CODEX_HOOK_FACT_ATTENTION_REQUIRED,
    CODEX_HOOK_FACT_ATTENTION_CLEARED,
} codex_hook_fact_t;

typedef enum {
    CODEX_HOOK_IDENTIFIER_SESSION = 1,
    CODEX_HOOK_IDENTIFIER_TURN,
} codex_hook_identifier_kind_t;

typedef bool (*codex_hook_identifier_map_fn)(
    void *context,
    codex_hook_identifier_kind_t kind,
    const char *identifier,
    size_t identifier_length,
    ambient_key_t *key);

typedef struct {
    codex_hook_source_state_t source_state;
    codex_hook_identifier_map_fn map_identifier;
    void *identifier_context;
} codex_hook_contract_t;

/* Raw identifiers are borrowed during mapping and never copied into events. */
typedef struct {
    codex_hook_fact_t fact;
    const char *session_id;
    size_t session_id_length;
    const char *turn_id;
    size_t turn_id_length;
    uint64_t sequence;
} codex_hook_fact_input_t;

typedef enum {
    CODEX_HOOK_CONTRACT_INVALID = 0,
    CODEX_HOOK_CONTRACT_MAPPED,
    CODEX_HOOK_CONTRACT_IGNORED,
    CODEX_HOOK_CONTRACT_UNSUPPORTED,
    CODEX_HOOK_CONTRACT_SOURCE_UNAVAILABLE,
    CODEX_HOOK_CONTRACT_SOURCE_UNVERIFIED,
    CODEX_HOOK_CONTRACT_IDENTIFIER_REJECTED,
} codex_hook_contract_result_t;

bool codex_hook_contract_init(codex_hook_contract_t *contract,
                              codex_hook_source_state_t source_state,
                              codex_hook_identifier_map_fn map_identifier,
                              void *identifier_context);

codex_hook_contract_result_t codex_hook_contract_map(
    const codex_hook_contract_t *contract,
    const codex_hook_fact_input_t *input,
    ambient_event_t *event);
