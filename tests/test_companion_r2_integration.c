#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "companion_r2_fixture.h"
#include "ambient_fake_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
static ambient_wire_message_t receive_frame(const ambient_transport_t *t)
{
    uint8_t frame[129]; size_t n; ambient_wire_message_t m;
    assert(ambient_transport_receive(t,frame,sizeof(frame),&n)==AMBIENT_TRANSPORT_OK);
    assert(n&&frame[n-1]=='\n'&&ambient_wire_decode(frame,n-1,&m)); return m;
}
static void connect_peer(companion_runtime_t *r, ambient_wire_session_t *peer, uint64_t gen)
{
    ambient_wire_message_t hello={.kind=AMBIENT_WIRE_HELLO,.version=1,.generation=gen,
        .body.hello={.offered=7,.required=7}};
    assert(companion_runtime_link_ready(r,gen,&hello));
    assert(ambient_wire_session_init(peer,gen,7,7)); assert(ambient_wire_negotiate(peer,&hello));
}
static void ack(companion_runtime_t *r, ambient_wire_kind_t kind, uint64_t id)
{
    ambient_wire_message_t a={.kind=AMBIENT_WIRE_ACK,.version=1,.generation=r->wire.generation,
        .body.ack={.target=kind,.id=id}};
    assert(ambient_wire_receive(&r->wire,&a)); assert(!ambient_wire_receive(&r->wire,&a));
}
static rollout_rate_limits_input_t limits(double used, uint64_t marker)
{
    return (rollout_rate_limits_input_t){.limit_id="codex",
        .has_primary=true,.primary={.window_minutes_present=true,.window_minutes=300,
        .used_percent_present=true,.used_percent=used,.resets_at_present=true,.resets_at=marker}};
}
static void test_source_loss_discards_unpublished_notices(void)
{
    companion_runtime_t r={0}; companion_runtime_lease_t lease={0};
    companion_runtime_options_t options=r2_options(&lease,1,NULL);
    assert(companion_runtime_start(&r,&options));
    ambient_wire_session_t peer; connect_peer(&r,&peer,30);
    uint8_t storage[516]; size_t lengths[4]; ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake,storage,sizeof(storage),lengths,4,129,129,&transport));
    ambient_fake_transport_set_connected(&fake,true);

    codex_source_observation_t event=r2_observation(1,"attention","turn-a",1,
                                                     CODEX_HOOK_FACT_TURN_STARTED);
    assert(companion_runtime_enqueue(&r,&event)&&companion_runtime_drain(&r,0)==1);
    event=r2_observation(1,"attention",NULL,2,CODEX_HOOK_FACT_ATTENTION_REQUIRED);
    assert(companion_runtime_enqueue(&r,&event)&&companion_runtime_drain(&r,1)==1);
    event=r2_observation(1,"failure","turn-b",1,CODEX_HOOK_FACT_TURN_STARTED);
    assert(companion_runtime_enqueue(&r,&event)&&companion_runtime_drain(&r,1)==1);
    event=r2_observation(1,"failure","turn-b",2,CODEX_HOOK_FACT_TURN_FAILED);
    assert(companion_runtime_enqueue(&r,&event)&&companion_runtime_drain(&r,2)==1);
    assert(r.pending_count==2); /* ATTENTION and ERROR, neither published. */

    companion_runtime_source_lost(&r,1);
    assert(!r.source_available&&!r.diagnostics.source_available);
    assert(r.pending_count==0&&r.wire.notice_count==0);
    assert(r2_status(&r,103)==AMBIENT_STATUS_OFFLINE);
    assert(companion_runtime_publish(&r,&transport,103,1)==AMBIENT_TRANSPORT_OK);
    ambient_wire_message_t snapshot=receive_frame(&transport);
    assert(snapshot.kind==AMBIENT_WIRE_SNAPSHOT);
    assert(ambient_wire_receive(&peer,&snapshot));
    assert(peer.received_snapshot.status==AMBIENT_STATUS_OFFLINE);
    assert(r.wire.notice_count==0);
    ack(&r,AMBIENT_WIRE_SNAPSHOT,snapshot.body.snapshot.revision);
    assert(ambient_wire_send_notice(&r.wire,&transport)==AMBIENT_TRANSPORT_INVALID);
    assert(fake.count==0&&r.pending_count==0&&r.wire.notice_count==0);
    companion_runtime_stop(&r);
}
int main(void)
{
    test_source_loss_discards_unpublished_notices();
    char dir[]="/tmp/r2-integration.XXXXXX",path[256]; assert(mkdtemp(dir));
    snprintf(path,sizeof(path),"%s/reset",dir);
    companion_runtime_t r={0}; companion_runtime_lease_t lease={0};
    companion_runtime_options_t options=r2_options(&lease,1,path);
    assert(companion_runtime_start(&r,&options)); assert(r.diagnostics.load_result==QUOTA_RESET_STATE_STORE_NOT_FOUND);
    ambient_wire_session_t peer; connect_peer(&r,&peer,10);
    uint8_t storage[516]; size_t lengths[4]; ambient_fake_transport_t fake; ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake,storage,sizeof(storage),lengths,4,129,129,&transport));
    ambient_fake_transport_set_connected(&fake,true);
    codex_source_observation_t event=r2_observation(1,"a","t",1,CODEX_HOOK_FACT_TURN_STARTED);
    assert(companion_runtime_enqueue(&r,&event)); assert(companion_runtime_drain(&r,0)==1);
    event=r2_observation(1,"a",NULL,2,CODEX_HOOK_FACT_ATTENTION_REQUIRED);
    assert(companion_runtime_enqueue(&r,&event)); assert(companion_runtime_drain(&r,1)==1);
    assert(companion_runtime_publish(&r,&transport,1,1)==AMBIENT_TRANSPORT_OK);
    ambient_wire_message_t m=receive_frame(&transport); assert(ambient_wire_receive(&peer,&m));
    assert(peer.received_snapshot.status==AMBIENT_STATUS_ATTENTION&&r.wire.notice_count==1);
    assert(ambient_wire_send_notice(&r.wire,&transport)!=AMBIENT_TRANSPORT_OK);
    ack(&r,AMBIENT_WIRE_SNAPSHOT,m.body.snapshot.revision);
    assert(ambient_wire_send_notice(&r.wire,&transport)==AMBIENT_TRANSPORT_OK);
    m=receive_frame(&transport); assert(ambient_wire_receive(&peer,&m));
    ambient_wire_snapshot_t prior=peer.received_snapshot;
    assert(!ambient_wire_receive(&peer,&m)&&!memcmp(&prior,&peer.received_snapshot,sizeof(prior)));
    ack(&r,AMBIENT_WIRE_NOTICE,m.body.notice.id);
    event=r2_observation(1,"a",NULL,3,CODEX_HOOK_FACT_ATTENTION_CLEARED);
    assert(companion_runtime_enqueue(&r,&event)); assert(companion_runtime_drain(&r,2)==1);
    event=r2_observation(1,"a","t",4,CODEX_HOOK_FACT_TURN_SUCCEEDED);
    assert(companion_runtime_enqueue(&r,&event)); assert(companion_runtime_drain(&r,3)==1);
    assert(companion_runtime_publish(&r,&transport,3,1)==AMBIENT_TRANSPORT_OK);
    m=receive_frame(&transport); assert(ambient_wire_receive(&peer,&m)&&peer.received_snapshot.status==AMBIENT_STATUS_DONE);
    rollout_rate_limits_input_t q=limits(80,1000);
    assert(companion_runtime_quota(&r,&q,1)==ROLLOUT_QUOTA_AVAILABLE);
    q=limits(10,2000); assert(companion_runtime_quota(&r,&q,2)==ROLLOUT_QUOTA_AVAILABLE);
    assert(r.diagnostics.candidate_mask==1&&!r.diagnostics.confirmed_mask);
    companion_runtime_stop(&r); options.epoch=2;
    assert(companion_runtime_start(&r,&options)); assert(!r.core.rollout_quota_source.has_snapshot);
    assert(companion_runtime_quota(&r,&q,3)==ROLLOUT_QUOTA_AVAILABLE);
    assert(r.diagnostics.confirmed_mask==1&&r.pending_count==1);
    companion_runtime_stop(&r); options.epoch=3;
    assert(companion_runtime_start(&r,&options));
    assert(companion_runtime_quota(&r,&q,4)==ROLLOUT_QUOTA_AVAILABLE&&!r.diagnostics.confirmed_mask);
    connect_peer(&r,&peer,20);
    assert(companion_runtime_publish(&r,&transport,0,2000)==AMBIENT_TRANSPORT_OK);
    m=receive_frame(&transport); assert(ambient_wire_receive(&peer,&m));
    assert(peer.received_snapshot.quota_present==0&&peer.received_snapshot.status==AMBIENT_STATUS_OFFLINE);
    companion_runtime_link_lost(&r); assert(!r.wire.ready&&!r.wire.notice_count);
    assert(!ambient_wire_receive(&r.wire,&m)); connect_peer(&r,&peer,21);
    ambient_fake_transport_set_connected(&fake,false);
    assert(companion_runtime_publish(&r,&transport,1,2001)==AMBIENT_TRANSPORT_DISCONNECTED);
    assert(r.wire.emitted_revision==0); ambient_fake_transport_set_connected(&fake,true);
    for(unsigned j=0;j<4;j++) assert(companion_runtime_publish(&r,&transport,j+2,2001)==AMBIENT_TRANSPORT_OK);
    assert(companion_runtime_publish(&r,&transport,6,2001)==AMBIENT_TRANSPORT_WOULD_BLOCK&&r.wire.emitted_revision==4);
    while(fake.count) (void)receive_frame(&transport);
    companion_runtime_stop(&r);
    /* Force atomic rename failure by making destination a directory. */
    assert(unlink(path)==0&&mkdir(path,0700)==0); options.epoch=4;
    assert(companion_runtime_start(&r,&options)); q=limits(80,3000);
    assert(companion_runtime_quota(&r,&q,1)==ROLLOUT_QUOTA_AVAILABLE);
    assert(r.diagnostics.persistence_failures==1&&r.diagnostics.write_result==QUOTA_RESET_STATE_STORE_IO_ERROR);
    assert(rmdir(path)==0); q=limits(80,3000);
    assert(companion_runtime_quota(&r,&q,2)==ROLLOUT_QUOTA_AVAILABLE);
    assert(r.diagnostics.write_result==QUOTA_RESET_STATE_STORE_OK&&!r.diagnostics.confirmed_mask&&!r.diagnostics.persistence_degraded);
    assert(companion_runtime_quota(&r,NULL,3)==ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!r.core.rollout_quota_source.has_snapshot);
    char diagnostic[128]; size_t n;
    assert(companion_diagnostics_encode(&r.diagnostics,diagnostic,sizeof(diagnostic),&n));
    assert(!strstr(diagnostic,path)&&!strstr(diagnostic,"codex")&&!strstr(diagnostic,"prompt"));
    companion_runtime_stop(&r); assert(unlink(path)==0);
    FILE *corrupt=fopen(path,"wb"); assert(corrupt);
    assert(fputs("corrupt",corrupt)>=0&&fclose(corrupt)==0);
    options.epoch=5; assert(companion_runtime_start(&r,&options));
    assert(r.diagnostics.load_result==QUOTA_RESET_STATE_STORE_CORRUPT);
    assert(!r.core.rollout_quota_source.has_snapshot);
    q=limits(10,4000); assert(companion_runtime_quota(&r,&q,1)==ROLLOUT_QUOTA_AVAILABLE);
    assert(!r.diagnostics.confirmed_mask); /* Corrupt history re-baselines. */
    companion_runtime_stop(&r); assert(unlink(path)==0);
    char missing[300]; snprintf(missing,sizeof(missing),"%s/missing/reset",dir);
    options.quota_state_path=missing; options.epoch=6;
    assert(companion_runtime_start(&r,&options));
    assert(companion_runtime_quota(&r,&q,1)==ROLLOUT_QUOTA_AVAILABLE);
    assert(r.diagnostics.persistence_degraded&&!r.diagnostics.confirmed_mask);
    ambient_status_t status=r2_status(&r,0);
    assert(!companion_diagnostics_encode(&r.diagnostics,diagnostic,1,&n));
    assert(r2_status(&r,0)==status); /* Diagnostic failure cannot change truth. */
    rollout_watcher_t watcher; char line[1024]; rollout_watcher_stats_t stats;
    assert(rollout_watcher_init(&watcher,line,sizeof(line),1024));
    assert(companion_runtime_poll_rollout(&r,&watcher,missing,1,2,&stats)==ROLLOUT_WATCHER_IO_ERROR);
    assert(!r.source_available&&!r.core.rollout_quota_source.has_snapshot);
    companion_runtime_stop(&r); assert(rmdir(dir)==0);
    puts("R2 Companion projection, fake transport, persistence integration: PASS");
}
