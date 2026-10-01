#include "companion_r2_fixture.h"
#include <stdio.h>
int main(void)
{
    companion_runtime_t r = {0}; companion_runtime_lease_t lease = {0};
    companion_runtime_options_t options = r2_options(&lease, 1, NULL);
    assert(companion_runtime_start(&r, &options));
    assert(r2_apply(&r,"a","1",1,CODEX_HOOK_FACT_TURN_STARTED,0) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&r,"b","2",1,CODEX_HOOK_FACT_TURN_STARTED,0) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&r,"a","1",1,CODEX_HOOK_FACT_TURN_STARTED,1) == AMBIENT_REDUCER_REPLAY);
    assert(r2_apply(&r,"a","new",2,CODEX_HOOK_FACT_TURN_STARTED,1) == AMBIENT_REDUCER_APPLIED);
    size_t turns = r.ingestion.registry.turn_count;
    assert(r2_apply(&r,"a","1",3,CODEX_HOOK_FACT_TURN_SUCCEEDED,2) == AMBIENT_REDUCER_STALE_TURN);
    assert(r.ingestion.registry.turn_count == turns);
    assert(r2_apply(&r,"a",NULL,3,CODEX_HOOK_FACT_ATTENTION_REQUIRED,2) == AMBIENT_REDUCER_APPLIED);
    assert(r2_status(&r,2) == AMBIENT_STATUS_ATTENTION);
    assert(r2_apply(&r,"a",NULL,4,CODEX_HOOK_FACT_ATTENTION_CLEARED,3) == AMBIENT_REDUCER_APPLIED);
    assert(r2_status(&r,3) == AMBIENT_STATUS_WORKING);
    assert(r2_apply(&r,"a","new",5,CODEX_HOOK_FACT_TURN_SUCCEEDED,4) == AMBIENT_REDUCER_APPLIED);
    assert(r2_status(&r,4) == AMBIENT_STATUS_WORKING); /* b still active. */
    assert(r2_apply(&r,"b","2",2,CODEX_HOOK_FACT_TURN_FAILED,4) == AMBIENT_REDUCER_APPLIED);
    assert(r2_status(&r,4) == AMBIENT_STATUS_DONE);
    assert(r2_apply(&r,"b","3",3,CODEX_HOOK_FACT_TURN_STARTED,5) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&r,"b","3",4,CODEX_HOOK_FACT_TURN_ABORTED,6) == AMBIENT_REDUCER_APPLIED);
    assert(r2_status(&r,24) == AMBIENT_STATUS_IDLE);
    assert(!companion_ingestion_retire(&r.ingestion,&r.core,1,24));
    assert(companion_ingestion_retire(&r.ingestion,&r.core,1,105));
    assert(r2_apply(&r,"a","new",6,CODEX_HOOK_FACT_TURN_STARTED,106) == AMBIENT_REDUCER_INVALID);
    codex_source_observation_t old = r2_observation(9,"c","t",1,CODEX_HOOK_FACT_TURN_STARTED);
    assert(companion_ingestion_apply(&r.ingestion,&r.core,CODEX_SOURCE_SYNTHETIC,&old,107) == AMBIENT_REDUCER_INVALID);
    r.ingestion.busy = true;
    old.epoch = 1;
    assert(companion_ingestion_apply(&r.ingestion,&r.core,CODEX_SOURCE_SYNTHETIC,&old,107) == AMBIENT_REDUCER_INVALID);
    r.ingestion.busy = false;
    assert(r2_apply(&r,"b","last",UINT64_MAX,CODEX_HOOK_FACT_TURN_STARTED,108) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&r,"b","last",1,CODEX_HOOK_FACT_TURN_SUCCEEDED,109) == AMBIENT_REDUCER_REPLAY);
    companion_runtime_stop(&r);
    options.epoch=2; assert(companion_runtime_start(&r,&options));
    char s[16];
    for (unsigned j=0;j<8;j++) {
        snprintf(s,sizeof(s),"s%u",j);
        assert(r2_apply(&r,s,"t",1,CODEX_HOOK_FACT_TURN_STARTED,0)==AMBIENT_REDUCER_APPLIED);
    }
    assert(r2_apply(&r,"extra","t",1,CODEX_HOOK_FACT_TURN_STARTED,1)==AMBIENT_REDUCER_NO_CAPACITY);
    assert(r.ingestion.registry.session_count==8);
    assert(r2_apply(&r,"replacement","t",1,CODEX_HOOK_FACT_TURN_STARTED,101)==AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&r,"s0","t",2,CODEX_HOOK_FACT_TURN_SUCCEEDED,102)==AMBIENT_REDUCER_INVALID);
    companion_runtime_stop(&r);
    puts("serialized ingestion tests: PASS");
}
