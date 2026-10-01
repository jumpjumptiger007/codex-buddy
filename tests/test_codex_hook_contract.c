#include <assert.h>
#include <string.h>

#include "codex_hook_contract.h"
#include "ambient_reducer.h"

typedef struct {
    size_t calls;
    bool reject;
} map_state_t;

static bool map_identifier(void *context,
                           codex_hook_identifier_kind_t kind,
                           const char *identifier,
                           size_t identifier_length,
                           ambient_key_t *key)
{
    map_state_t *state = context;
    state->calls++;
    if (state->reject || !identifier || identifier_length == 0 || !key) {
        return false;
    }
    *key = kind == CODEX_HOOK_IDENTIFIER_SESSION ? 7U : 11U;
    return true;
}

int main(void)
{
    static const char session_id[] = "session-opaque";
    static const char turn_id[] = "turn-opaque";
    char too_long[CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES + 1U];
    char with_nul[] = {'s', 'e', '\0', 's'};
    map_state_t map_state = {0};
    codex_hook_contract_t contract;
    codex_hook_fact_input_t input = {
        .fact = CODEX_HOOK_FACT_TURN_STARTED,
        .session_id = session_id,
        .session_id_length = sizeof(session_id) - 1U,
        .turn_id = turn_id,
        .turn_id_length = sizeof(turn_id) - 1U,
        .sequence = 1,
    };
    ambient_event_t event;
    ambient_session_slot_t slot;
    ambient_reducer_t reducer;

    ambient_event_t nonzero = {.kind = AMBIENT_EVENT_TURN_STARTED,
        .session_key = 7, .turn_key = 11, .event_key = 19};
    event = nonzero;
    assert(codex_hook_contract_map(NULL, &input, &event) == CODEX_HOOK_CONTRACT_INVALID);
    assert(event.kind == 0 && event.session_key == 0 && event.turn_key == 0 && event.event_key == 0);
    assert(codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_VERIFIED, map_identifier, &map_state));
    event = nonzero;
    assert(codex_hook_contract_map(&contract, NULL, &event) == CODEX_HOOK_CONTRACT_INVALID);
    assert(event.kind == 0 && event.session_key == 0 && event.turn_key == 0 && event.event_key == 0);
    assert(codex_hook_contract_map(&contract, &input, NULL) == CODEX_HOOK_CONTRACT_INVALID);
    assert(!codex_hook_contract_init(NULL, CODEX_HOOK_SOURCE_VERIFIED,
                                     map_identifier, &map_state));
    assert(!codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_VERIFIED,
                                     NULL, &map_state));
    assert(codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_UNAVAILABLE,
                                    map_identifier, &map_state));
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_SOURCE_UNAVAILABLE);
    assert(event.session_key == 0 && map_state.calls == 0);
    assert(codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_UNVERIFIED,
                                    map_identifier, &map_state));
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_SOURCE_UNVERIFIED);
    assert(map_state.calls == 0);

    assert(codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_VERIFIED,
                                    map_identifier, &map_state));
    input.fact = CODEX_HOOK_FACT_NON_LIFECYCLE;
    input.session_id = NULL;
    input.session_id_length = 0;
    input.turn_id = NULL;
    input.turn_id_length = 0;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_IGNORED);
    assert(map_state.calls == 0);

    input.fact = CODEX_HOOK_FACT_TURN_STARTED;
    input.session_id = session_id;
    input.session_id_length = sizeof(session_id) - 1U;
    input.turn_id = turn_id;
    input.turn_id_length = sizeof(turn_id) - 1U;
    input.sequence = 1;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_MAPPED);
    assert(event.kind == AMBIENT_EVENT_TURN_STARTED);
    assert(event.session_key == 7 && event.turn_key == 11
           && event.event_key == 1);
    assert(ambient_reducer_init(&reducer, &slot, 1, 100, 10));
    assert(ambient_reducer_apply(&reducer, &event, 1)
           == AMBIENT_REDUCER_APPLIED);
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_MAPPED);
    assert(ambient_reducer_apply(&reducer, &event, 2)
           == AMBIENT_REDUCER_REPLAY);
    input.sequence = 0;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);
    input.sequence = 2;

    event = nonzero;
    input.fact = (codex_hook_fact_t)99;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_UNSUPPORTED);
    assert(event.kind == 0 && event.session_key == 0 && event.turn_key == 0 && event.event_key == 0);
    input.fact = CODEX_HOOK_FACT_TURN_SUCCEEDED;
    input.turn_id = NULL;
    input.turn_id_length = 0;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);
    input.fact = CODEX_HOOK_FACT_ATTENTION_REQUIRED;
    input.turn_id = turn_id;
    input.turn_id_length = sizeof(turn_id) - 1U;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);
    input.turn_id = NULL;
    input.turn_id_length = 0;
    input.session_id = "";
    input.session_id_length = 0;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);
    input.session_id = with_nul;
    input.session_id_length = sizeof(with_nul);
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);
    memset(too_long, 'x', sizeof(too_long));
    input.session_id = too_long;
    input.session_id_length = sizeof(too_long);
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_INVALID);

    map_state.reject = true;
    event = nonzero;
    input.session_id = session_id;
    input.session_id_length = sizeof(session_id) - 1U;
    input.fact = CODEX_HOOK_FACT_TURN_ABORTED;
    input.turn_id = turn_id;
    input.turn_id_length = sizeof(turn_id) - 1U;
    assert(codex_hook_contract_map(&contract, &input, &event)
           == CODEX_HOOK_CONTRACT_IDENTIFIER_REJECTED);
    assert(event.kind == 0 && event.session_key == 0 && event.turn_key == 0 && event.event_key == 0);
    map_state.reject = false;
    input.session_id = too_long;
    input.session_id_length = CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES;
    for (int fact = CODEX_HOOK_FACT_SESSION_IDLE; fact <= CODEX_HOOK_FACT_ATTENTION_CLEARED; fact++) {
        input.fact = (codex_hook_fact_t)fact;
        bool turn = fact >= CODEX_HOOK_FACT_TURN_STARTED && fact <= CODEX_HOOK_FACT_TURN_ABORTED;
        input.turn_id = turn ? turn_id : NULL;
        input.turn_id_length = turn ? sizeof(turn_id) - 1 : 0;
        assert(codex_hook_contract_map(&contract, &input, &event) == CODEX_HOOK_CONTRACT_MAPPED);
        assert(event.session_key && (turn ? event.turn_key != 0 : event.turn_key == 0));
    }
    input.fact = CODEX_HOOK_FACT_TURN_STARTED; input.turn_id = NULL; input.turn_id_length = 0;
    size_t calls = map_state.calls;
    assert(codex_hook_contract_map(&contract, &input, &event) == CODEX_HOOK_CONTRACT_INVALID);
    assert(map_state.calls == calls);
    contract.map_identifier = NULL;
    assert(codex_hook_contract_map(&contract, &input, &event) == CODEX_HOOK_CONTRACT_INVALID);
    return 0;
}
