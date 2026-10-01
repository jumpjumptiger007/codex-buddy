#pragma once
#include "codex_hook_contract.h"
#define COMPANION_REGISTRY_SESSIONS 16U
#define COMPANION_REGISTRY_TURNS 32U
/* Eight live reducer slots plus eight retained retirements; no eviction.
 * Turn history is retained until the entire source epoch is discarded. */
typedef struct {
    char bytes[CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES];
    size_t length;
    ambient_key_t key;
    bool retired;
} companion_identifier_entry_t;
typedef struct {
    companion_identifier_entry_t sessions[COMPANION_REGISTRY_SESSIONS];
    companion_identifier_entry_t turns[COMPANION_REGISTRY_TURNS];
    uint64_t next_session, next_turn;
    size_t session_count, turn_count;
} companion_identifier_registry_t;
void companion_identifier_registry_init(companion_identifier_registry_t *registry);
bool companion_identifier_valid(const char *bytes, size_t length);
bool companion_identifier_registry_map(void *context, codex_hook_identifier_kind_t kind,
                                      const char *bytes, size_t length, ambient_key_t *key);
bool companion_identifier_registry_retire(companion_identifier_registry_t *registry,
                                          ambient_key_t session);
