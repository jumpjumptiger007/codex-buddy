#include "companion_r2_fixture.h"
#include <stdio.h>
static unsigned starts, stops;
static bool fail_start(void *c) { (void)c; starts++; return false; }
static void stop_source(void *c) { (void)c; stops++; }
int main(void)
{
    companion_runtime_t r={0}, other={0}; companion_runtime_lease_t lease={0};
    companion_runtime_options_t o=r2_options(&lease,1,NULL);
    o.start_source=fail_start; o.stop_source=stop_source;
    assert(!companion_runtime_start(&r,&o));
    assert(starts==1&&stops==1&&!lease.owner&&!r.running);
    o.start_source=NULL; o.stop_source=NULL;
    assert(companion_runtime_start(&r,&o)); assert(companion_runtime_start(&r,&o));
    assert(r.source_available && r.diagnostics.source_available
           && r.diagnostics.profile == CODEX_SOURCE_SYNTHETIC);
    assert(!companion_runtime_start(&other,&o));
    codex_source_observation_t event=r2_observation(1,"s","t",1,CODEX_HOOK_FACT_TURN_STARTED);
    for(unsigned j=0;j<8;j++) { event.ordinal=j+1; assert(companion_runtime_enqueue(&r,&event)); }
    event.ordinal=9; assert(!companion_runtime_enqueue(&r,&event));
    assert(r.diagnostics.queue_dropped==1&&r.queue_count==8);
    assert(companion_runtime_drain(&r,0)==8&&r.queue_count==0);
    assert(r2_status(&r,0)==AMBIENT_STATUS_WORKING);
    companion_runtime_source_lost(&r,1); assert(!companion_runtime_enqueue(&r,&event));
    assert(r2_status(&r,101)==AMBIENT_STATUS_OFFLINE);
    assert(!companion_runtime_source_recovered(&r,2));
    assert(companion_runtime_source_recovered(&r,1));
    assert(companion_runtime_enqueue(&r,&event)); assert(companion_runtime_drain(&r,102)==1);
    assert(r2_status(&r,102)==AMBIENT_STATUS_WORKING);
    for (uint64_t j=10;j<16;j++)
        assert(r2_apply(&r,"s",NULL,j,CODEX_HOOK_FACT_ATTENTION_REQUIRED,103)==AMBIENT_REDUCER_APPLIED);
    assert(r.pending_count==4&&r.diagnostics.notice_dropped==2);
    companion_runtime_stop(&r); companion_runtime_stop(&r);
    assert(!r.running&&!lease.owner&&!r.queue_count&&!r.wire.ready);
    assert(!companion_runtime_start(&r,&o)); o.epoch=2;
    assert(companion_runtime_start(&r,&o)); assert(r2_status(&r,0)==AMBIENT_STATUS_OFFLINE);
    event.epoch=1; assert(!companion_runtime_enqueue(&r,&event));
    /* Explicit process death simulation: supervisor releases volatile lease;
     * replacement requires a fresh epoch, no retained stale truth. */
    lease.owner=NULL; o.epoch=3;
    assert(companion_runtime_start(&other,&o)); assert(r2_status(&other,0)==AMBIENT_STATUS_OFFLINE);
    event.epoch=2; assert(!companion_runtime_enqueue(&r,&event));
    assert(companion_runtime_drain(&r,1)==0);
    companion_runtime_stop(&r); assert(lease.owner==&other);
    companion_runtime_stop(&other);
    o.epoch=4; o.profile=CODEX_SOURCE_DISABLED;
    assert(companion_runtime_start(&other,&o)); assert(!other.source_available);
    assert(r2_status(&other,0)==AMBIENT_STATUS_OFFLINE); companion_runtime_stop(&other);
    memset(&other, 0, sizeof(other)); o.epoch=5; o.profile=CODEX_SOURCE_CURRENT_HOOK;
    assert(companion_runtime_start(&other,&o));
    assert(other.running && !other.source_available
           && !other.diagnostics.source_available
           && other.diagnostics.profile == CODEX_SOURCE_CURRENT_HOOK);
    event=r2_observation(5,"unverified-session","unverified-turn",1,
                         CODEX_HOOK_FACT_TURN_STARTED);
    assert(!companion_runtime_enqueue(&other,&event));
    assert(!companion_runtime_source_recovered(&other,5));
    assert(!other.source_available && !other.diagnostics.source_available);
    assert(r2_status(&other,0)==AMBIENT_STATUS_OFFLINE
           && other.ingestion.registry.session_count==0);
    companion_runtime_stop(&other);
    puts("startup lifecycle, queue, loss/recovery/restart tests: PASS");
}
