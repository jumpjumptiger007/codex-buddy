#include "identifier_registry.h"
#include <string.h>
void companion_identifier_registry_init(companion_identifier_registry_t *r)
{
    if (r) { memset(r, 0, sizeof(*r)); r->next_session = r->next_turn = 1; }
}
bool companion_identifier_valid(const char *s, size_t n)
{
    return s && n && n <= CODEX_HOOK_CONTRACT_MAX_IDENTIFIER_BYTES && !memchr(s, 0, n);
}
bool companion_identifier_registry_map(void *context, codex_hook_identifier_kind_t kind,
                                      const char *s, size_t n, ambient_key_t *key)
{
    companion_identifier_registry_t *r = context;
    if (key) *key = 0;
    if (!r || !key || !companion_identifier_valid(s, n)) return false;
    companion_identifier_entry_t *entries;
    size_t *count, capacity;
    uint64_t *next;
    if (kind == CODEX_HOOK_IDENTIFIER_SESSION) {
        entries = r->sessions; count = &r->session_count;
        capacity = COMPANION_REGISTRY_SESSIONS; next = &r->next_session;
    } else if (kind == CODEX_HOOK_IDENTIFIER_TURN) {
        entries = r->turns; count = &r->turn_count;
        capacity = COMPANION_REGISTRY_TURNS; next = &r->next_turn;
    } else return false;
    if (*count > capacity) return false;
    for (size_t i = 0; i < *count; i++) {
        if (entries[i].length == n && !memcmp(entries[i].bytes, s, n)) {
            if (entries[i].retired) return false;
            *key = entries[i].key; return *key != 0;
        }
    }
    if (*count == capacity || !*next || *next == UINT64_MAX) return false;
    companion_identifier_entry_t *e = &entries[*count];
    memset(e, 0, sizeof(*e)); memcpy(e->bytes, s, n); e->length = n;
    e->key = (*next)++; (*count)++; *key = e->key; return true;
}
bool companion_identifier_registry_retire(companion_identifier_registry_t *r, ambient_key_t key)
{
    if (!r || !key || r->session_count > COMPANION_REGISTRY_SESSIONS) return false;
    for (size_t i = 0; i < r->session_count; i++) {
        if (r->sessions[i].key == key) { r->sessions[i].retired = true; return true; }
    }
    return false;
}
