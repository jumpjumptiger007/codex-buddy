#include "codex_hook_contract.h"

#include <string.h>

static bool codex_hook_fact_uses_turn(codex_hook_fact_t fact)
{
    return fact == CODEX_HOOK_FACT_TURN_STARTED
        || fact == CODEX_HOOK_FACT_TURN_SUCCEEDED
        || fact == CODEX_HOOK_FACT_TURN_FAILED
        || fact == CODEX_HOOK_FACT_TURN_ABORTED;
}

static ambient_event_kind_t codex_hook_fact_event_kind(codex_hook_fact_t fact)
{
    switch (fact) {
    case CODEX_HOOK_FACT_SESSION_IDLE:
        return AMBIENT_EVENT_SESSION_IDLE;
    case CODEX_HOOK_FACT_TURN_STARTED:
        return AMBIENT_EVENT_TURN_STARTED;
    case CODEX_HOOK_FACT_TURN_SUCCEEDED:
        return AMBIENT_EVENT_TURN_SUCCEEDED;
    case CODEX_HOOK_FACT_TURN_FAILED:
        return AMBIENT_EVENT_TURN_FAILED;
    case CODEX_HOOK_FACT_TURN_ABORTED:
        return AMBIENT_EVENT_TURN_ABORTED;
    case CODEX_HOOK_FACT_ATTENTION_REQUIRED:
        return AMBIENT_EVENT_ATTENTION_REQUIRED;
    case CODEX_HOOK_FACT_ATTENTION_CLEARED:
        return AMBIENT_EVENT_ATTENTION_CLEARED;
    case CODEX_HOOK_FACT_NON_LIFECYCLE:
    default:
        return AMBIENT_EVENT_INVALID;
    }
}

static bool codex_hook_identifier_is_valid(const char *identifier,
                                           size_t identifier_length)
{
    if (!identifier || identifier_length == 0
        || identifier_length > CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES) {
        return false;
    }
    for (size_t i = 0; i < identifier_length; i++) {
        if (identifier[i] == '\0') {
            return false;
        }
    }
    return true;
}

static bool codex_hook_map_identifier(
    const codex_hook_contract_t *contract,
    codex_hook_identifier_kind_t kind,
    const char *identifier,
    size_t identifier_length,
    ambient_key_t *key)
{
    if (!codex_hook_identifier_is_valid(identifier, identifier_length)) {
        return false;
    }
    *key = 0;
    return contract->map_identifier(contract->identifier_context, kind,
                                    identifier, identifier_length, key)
        && *key != 0;
}

bool codex_hook_contract_init(codex_hook_contract_t *contract,
                              codex_hook_source_state_t source_state,
                              codex_hook_identifier_map_fn map_identifier,
                              void *identifier_context)
{
    if (!contract || source_state > CODEX_HOOK_SOURCE_VERIFIED
        || source_state < CODEX_HOOK_SOURCE_UNAVAILABLE
        || (source_state == CODEX_HOOK_SOURCE_VERIFIED && !map_identifier)) {
        return false;
    }
    *contract = (codex_hook_contract_t){
        .source_state = source_state,
        .map_identifier = map_identifier,
        .identifier_context = identifier_context,
    };
    return true;
}

codex_hook_contract_result_t codex_hook_contract_map(
    const codex_hook_contract_t *contract,
    const codex_hook_fact_input_t *input,
    ambient_event_t *event)
{
    ambient_event_t mapped = {0};
    ambient_event_kind_t event_kind;

    if (event) memset(event, 0, sizeof(*event));
    if (!contract || !input || !event) {
        return CODEX_HOOK_CONTRACT_INVALID;
    }

    switch (contract->source_state) {
    case CODEX_HOOK_SOURCE_UNAVAILABLE:
        return CODEX_HOOK_CONTRACT_SOURCE_UNAVAILABLE;
    case CODEX_HOOK_SOURCE_UNVERIFIED:
        return CODEX_HOOK_CONTRACT_SOURCE_UNVERIFIED;
    case CODEX_HOOK_SOURCE_VERIFIED:
        if (!contract->map_identifier) {
            return CODEX_HOOK_CONTRACT_INVALID;
        }
        break;
    default:
        return CODEX_HOOK_CONTRACT_INVALID;
    }

    if (input->fact == CODEX_HOOK_FACT_NON_LIFECYCLE) {
        return CODEX_HOOK_CONTRACT_IGNORED;
    }
    event_kind = codex_hook_fact_event_kind(input->fact);
    if (event_kind == AMBIENT_EVENT_INVALID) {
        return CODEX_HOOK_CONTRACT_UNSUPPORTED;
    }
    if (input->sequence == 0
        || !codex_hook_identifier_is_valid(input->session_id,
                                            input->session_id_length)) {
        return CODEX_HOOK_CONTRACT_INVALID;
    }

    if (codex_hook_fact_uses_turn(input->fact)) {
        if (!codex_hook_identifier_is_valid(input->turn_id, input->turn_id_length)) {
            return CODEX_HOOK_CONTRACT_INVALID;
        }
    } else if (input->turn_id || input->turn_id_length != 0) {
        return CODEX_HOOK_CONTRACT_INVALID;
    }

    mapped.kind = event_kind;
    mapped.event_key = input->sequence;
    if (!codex_hook_map_identifier(contract, CODEX_HOOK_IDENTIFIER_SESSION,
                                   input->session_id,
                                   input->session_id_length,
                                   &mapped.session_key)) {
        return CODEX_HOOK_CONTRACT_IDENTIFIER_REJECTED;
    }

    if (codex_hook_fact_uses_turn(input->fact)) {
        if (!codex_hook_identifier_is_valid(input->turn_id,
                                             input->turn_id_length)) {
            return CODEX_HOOK_CONTRACT_INVALID;
        }
        if (!codex_hook_map_identifier(contract, CODEX_HOOK_IDENTIFIER_TURN,
                                       input->turn_id,
                                       input->turn_id_length,
                                       &mapped.turn_key)) {
            return CODEX_HOOK_CONTRACT_IDENTIFIER_REJECTED;
        }
    } else if (input->turn_id || input->turn_id_length != 0) {
        return CODEX_HOOK_CONTRACT_INVALID;
    }

    *event = mapped;
    return CODEX_HOOK_CONTRACT_MAPPED;
}
