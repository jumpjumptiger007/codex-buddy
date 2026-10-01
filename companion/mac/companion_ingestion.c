#include "companion_ingestion.h"
#include <string.h>
bool companion_ingestion_init(companion_ingestion_t *i, uint64_t epoch)
{
    if (!i || !epoch) return false;
    memset(i, 0, sizeof(*i)); companion_identifier_registry_init(&i->registry);
    i->epoch = epoch; return true;
}
ambient_reducer_result_t companion_ingestion_apply(companion_ingestion_t *i,
    companion_core_t *core, codex_source_profile_t profile,
    const codex_source_observation_t *o, uint64_t now)
{
    codex_hook_fact_input_t f; ambient_event_t e; codex_hook_contract_t contract;
    if (!i || !core || !core->initialized || i->busy || !o || o->epoch != i->epoch
        || codex_source_adapter_validate(profile, o, &f) != CODEX_SOURCE_ACCEPTED)
        return AMBIENT_REDUCER_INVALID;
    i->busy = true;
    /* Transactional registry admission; rejected facts cannot consume capacity.
     * Copy is bounded host memory, never allocated on Passport. */
    companion_identifier_registry_t draft = i->registry;
    (void)codex_hook_contract_init(&contract, CODEX_HOOK_SOURCE_VERIFIED,
                                  companion_identifier_registry_map, &draft);
    ambient_reducer_result_t result = AMBIENT_REDUCER_INVALID;
    if (codex_hook_contract_map(&contract, &f, &e) == CODEX_HOOK_CONTRACT_MAPPED) {
        /* Commit mapping before callback; restore on reducer rejection. */
        companion_identifier_registry_t before = i->registry;
        i->registry = draft;
        ambient_key_t prior[AMBIENT_WIRE_MAX_SESSIONS] = {0};
        for (size_t j = 0; j < core->reducer.slot_capacity; ++j)
            if (core->reducer.slots[j].occupied) prior[j] = core->reducer.slots[j].session_key;
        result = companion_core_apply_event(core, &e, now, NULL);
        if (result == AMBIENT_REDUCER_APPLIED) {
            for (size_t j = 0; j < core->reducer.slot_capacity; ++j) {
                if (!prior[j]) continue;
                bool retained = false;
                for (size_t k = 0; k < core->reducer.slot_capacity; ++k)
                    if (core->reducer.slots[k].occupied && core->reducer.slots[k].session_key == prior[j])
                        retained = true;
                if (!retained) (void)companion_identifier_registry_retire(&i->registry, prior[j]);
            }
        }
        if (result != AMBIENT_REDUCER_APPLIED) i->registry = before;
    }
    i->busy = false; return result;
}
bool companion_ingestion_retire(companion_ingestion_t *i, companion_core_t *c,
    ambient_key_t session, uint64_t now)
{
    if (!i || !c || !c->initialized || i->busy) return false;
    ambient_session_view_t view;
    if (!ambient_reducer_get_session(&c->reducer, session, now, &view, NULL)
        || now < view.last_seen_ms || now - view.last_seen_ms <= c->reducer.freshness_ms)
        return false;
    return companion_identifier_registry_retire(&i->registry, session);
}
