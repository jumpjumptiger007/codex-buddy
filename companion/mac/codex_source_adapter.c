#include "codex_source_adapter.h"
#include "identifier_registry.h"
#include <string.h>
codex_source_result_t codex_source_adapter_validate(codex_source_profile_t profile,
    const codex_source_observation_t *o, codex_hook_fact_input_t *f)
{
    if (f) memset(f, 0, sizeof(*f));
    if (!o || !f) return CODEX_SOURCE_INVALID;
    if (profile == CODEX_SOURCE_DISABLED) return CODEX_SOURCE_UNAVAILABLE;
    if (profile == CODEX_SOURCE_CURRENT_HOOK) return CODEX_SOURCE_UNVERIFIED;
    if (profile != CODEX_SOURCE_SYNTHETIC || o->schema != 1) return CODEX_SOURCE_UNSUPPORTED;
    if (!o->epoch || !o->ordinal || o->fact < CODEX_HOOK_FACT_SESSION_IDLE
        || o->fact > CODEX_HOOK_FACT_ATTENTION_CLEARED
        || !companion_identifier_valid(o->session_id, o->session_length)) return CODEX_SOURCE_INVALID;
    bool turn = o->fact >= CODEX_HOOK_FACT_TURN_STARTED && o->fact <= CODEX_HOOK_FACT_TURN_ABORTED;
    if (turn ? !companion_identifier_valid(o->turn_id, o->turn_length)
             : (o->turn_id != NULL || o->turn_length != 0)) return CODEX_SOURCE_INVALID;
    *f = (codex_hook_fact_input_t){.fact=o->fact, .session_id=o->session_id,
        .session_id_length=o->session_length, .turn_id=o->turn_id,
        .turn_id_length=o->turn_length, .sequence=o->ordinal};
    return CODEX_SOURCE_ACCEPTED;
}
