#include "codex_source_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    codex_source_observation_t o = {.schema=1, .epoch=1, .ordinal=1,
        .fact=CODEX_HOOK_FACT_TURN_STARTED, .session_id="s", .session_length=1,
        .turn_id="t", .turn_length=1};
    codex_hook_fact_input_t f;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_ACCEPTED);
    assert(f.sequence == 1 && f.fact == CODEX_HOOK_FACT_TURN_STARTED);
    assert(codex_source_adapter_validate(CODEX_SOURCE_CURRENT_HOOK, &o, &f) == CODEX_SOURCE_UNVERIFIED);
    codex_hook_fact_input_t zero = {0}; assert(!memcmp(&f, &zero, sizeof(f)));
    assert(codex_source_adapter_validate(CODEX_SOURCE_DISABLED, &o, &f) == CODEX_SOURCE_UNAVAILABLE);
    o.schema = 2;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_UNSUPPORTED);
    o.schema = 1; o.session_id = NULL;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_INVALID);
    o.session_id = "s"; o.turn_id = NULL;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_INVALID);
    o.turn_id = "t"; o.ordinal = 0;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_INVALID);
    o.ordinal = 1; o.fact = 999;
    assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_INVALID);
    for (int j = CODEX_HOOK_FACT_SESSION_IDLE; j <= CODEX_HOOK_FACT_ATTENTION_CLEARED; j++) {
        o.fact = j; bool turn = j >= CODEX_HOOK_FACT_TURN_STARTED && j <= CODEX_HOOK_FACT_TURN_ABORTED;
        o.turn_id = turn ? "t" : NULL; o.turn_length = turn ? 1 : 0;
        assert(codex_source_adapter_validate(CODEX_SOURCE_SYNTHETIC, &o, &f) == CODEX_SOURCE_ACCEPTED);
    }
    puts("source adapter tests: PASS (synthetic; live remains unverified)");
}
