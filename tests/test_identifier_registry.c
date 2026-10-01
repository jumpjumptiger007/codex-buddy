#include "identifier_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    companion_identifier_registry_t r; companion_identifier_registry_init(&r);
    ambient_key_t a, b, t;
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, "same", 4, &a));
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, "same", 4, &b) && a == b);
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, "same", 4, &t));
    assert(r.session_count == 1 && r.turn_count == 1); /* Separate namespace. */
    /* Deliberately equal length / common prefix; no hash or prefix alias. */
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, "samb", 4, &b) && a != b);
    assert(companion_identifier_registry_retire(&r, a));
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, "same", 4, &b) && b == 0);
    char name[32];
    for (unsigned j = 2; j < COMPANION_REGISTRY_SESSIONS; j++) {
        int n = snprintf(name, sizeof(name), "session-%u", j);
        assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, name, n, &b));
    }
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_SESSION, "extra", 5, &b));
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, "t", 1, &b));
    char maximum[127]; memset(maximum, 'x', sizeof(maximum));
    assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, maximum, sizeof(maximum), &b));
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, maximum, 128, &b));
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, "a\0b", 3, &b));
    r.next_turn = UINT64_MAX;
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, "new", 3, &b));
    companion_identifier_registry_init(&r);
    assert(r.session_count == 0 && r.turn_count == 0); /* Whole epoch restart only. */
    for (unsigned j = 0; j < COMPANION_REGISTRY_TURNS; ++j) {
        int n = snprintf(name, sizeof(name), "turn-%u", j);
        assert(companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, name, n, &b));
    }
    assert(!companion_identifier_registry_map(&r, CODEX_HOOK_IDENTIFIER_TURN, "excess", 6, &b));
    puts("identifier registry tests: PASS");
}
