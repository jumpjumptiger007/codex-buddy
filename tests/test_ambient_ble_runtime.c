#include "ambient_ble_internal.h"
#include <assert.h>
#include <stdio.h>
static ambient_ble_config_t config;
static ambient_ble_t *start(void) {ambient_ble_t *b=NULL;assert(ambient_ble_start(&config,&b)==ESP_OK);return b;}
static void finish(ambient_ble_t *b){fake.auto_ack=true;fake.deinit=fake.stop=0;assert(ambient_ble_stop(b)==ESP_OK);assert(!g_ambient_ble_active&&fake.live==0);}
static void authorize(ambient_ble_t *b) {
 b->accepted_peer_valid=true;b->accepted_peer_identity_type=0;memset(b->accepted_peer_identity,0,6);b->accepted_peer_identity[0]=1;
 fake.desc.sec_state.encrypted=fake.desc.sec_state.authenticated=fake.desc.sec_state.bonded=true;
 struct ble_gap_event e={.type=BLE_GAP_EVENT_CONNECT,.connect={.conn_handle=1}};ambient_ble_gap_event(&e,b);
 e=(struct ble_gap_event){.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.conn_handle=1,.attr_handle=b->tx_value_handle,.cur_notify=true}};ambient_ble_gap_event(&e,b);
}
static void startup_faults(void) {
 for(unsigned i=1;i<=6;i++){ambient_fake_reset();fake.fail_allocation=i;ambient_ble_t*b=NULL;assert(ambient_ble_start(&config,&b)!=ESP_OK);assert(!b&&!g_ambient_ble_active&&!fake.live);}
 int *failures[]={&fake.nvs_init,&fake.nvs_open,&fake.nvs_load,&fake.nimble_init,&fake.gatt_count,&fake.gatt_add};
 for(size_t i=0;i<6;i++){ambient_fake_reset();*failures[i]=ESP_FAIL;ambient_ble_t*b=NULL;assert(ambient_ble_start(&config,&b)!=ESP_OK);assert(!b&&!g_ambient_ble_active&&!fake.live);}
 for(unsigned task=1;task<=2;task++){ambient_fake_reset();fake.fail_task=task;ambient_ble_t*b=NULL;assert(ambient_ble_start(&config,&b)!=ESP_OK);assert(!b&&!g_ambient_ble_active&&!fake.live);}
 ambient_fake_reset();fake.fail_task=2;fake.auto_ack=false;ambient_ble_t*b=NULL;
 assert(ambient_ble_start(&config,&b)==ESP_ERR_TIMEOUT&&b==g_ambient_ble_active&&b->shutting_down);
 assert(ambient_ble_start(&config,&b)==ESP_ERR_INVALID_STATE);b=g_ambient_ble_active;finish(b);
 ambient_fake_reset();fake.gatt_add=ESP_FAIL;fake.deinit=ESP_FAIL;b=NULL;
 assert(ambient_ble_start(&config,&b)!=ESP_OK&&b==g_ambient_ble_active);finish(b);
}
static void stop_retry(void) {
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);fake.auto_ack=false;
 assert(ambient_ble_stop(b)==ESP_ERR_TIMEOUT);assert(b==g_ambient_ble_active&&b->tx_task_started&&b->stop_handle==1);
 fake.auto_ack=true;fake.stop=1;assert(ambient_ble_stop(b)==ESP_FAIL&&!b->tx_task_started);
 assert(fake.terminations==1&&b->stop_handle==BLE_HS_CONN_HANDLE_NONE);
 fake.stop=0;fake.auto_ack=false;b->host_stopped->available=false;
 assert(ambient_ble_stop(b)==ESP_ERR_TIMEOUT&&b->host_stop_requested);
 unsigned stops=fake.stops;fake.auto_ack=true;fake.deinit=1;
 assert(ambient_ble_stop(b)!=ESP_OK&&!b->host_task_started&&b->initialized);assert(fake.stops==stops);
 finish(b);assert(ambient_ble_stop(NULL)==ESP_OK);
 for(unsigned i=0;i<50;i++){ambient_fake_reset();finish(start());}
}
static void data_and_gap(void) {
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
 uint8_t frame[95]={1},out[129];size_t length;
 assert(ambient_ble_send(b,frame,sizeof(frame))==ESP_OK);
 assert(ambient_ble_send(b,frame,1)==ESP_ERR_TIMEOUT);
 ambient_fake_tx_once(b);assert(fake.notifications==5);
 assert(ambient_ble_send(b,frame,1)==ESP_OK);fake.mbuf_fail=true;ambient_fake_tx_once(b);assert(fake.terminations==1&&!b->link.connected);fake.mbuf_fail=false;authorize(b);
 assert(ambient_ble_send(b,frame,1)==ESP_OK);fake.notify=1;ambient_fake_tx_once(b);assert(fake.terminations==2&&!b->link.connected);fake.notify=0;authorize(b);
 struct os_mbuf mb={.len=20};memset(mb.bytes,1,20);struct ble_gatt_access_ctxt access={BLE_GATT_ACCESS_OP_WRITE_CHR,&mb};
 assert(b->gatt_chars[0].access_cb(1,0,&access,b)==0);assert(ambient_ble_read(b,out,sizeof(out),&length)==ESP_OK&&length==20);
 for(unsigned i=0;i<25;i++)assert(b->gatt_chars[0].access_cb(1,0,&access,b)==0);
 assert(b->gatt_chars[0].access_cb(1,0,&access,b)==BLE_ATT_ERR_INSUFFICIENT_RES);
 ambient_ble_metrics_t measured;assert(ambient_ble_get_metrics(b,&measured)==ESP_OK&&measured.rx_bytes==500&&measured.rx_high_water_bytes==500);
 mb.len=21;assert(b->gatt_chars[0].access_cb(1,0,&access,b)==BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);mb.len=20;
 assert(ambient_ble_send(b,frame,1)==ESP_OK);
 struct ble_gap_event e={.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.conn_handle=2,.cur_notify=false,.attr_handle=b->tx_value_handle}};
 uint64_t incarnation=b->link.link_incarnation;ambient_ble_gap_event(&e,b);assert(b->link.link_incarnation==incarnation&&b->link.tx_notify_subscribed);
 e.subscribe.conn_handle=1;ambient_ble_gap_event(&e,b);assert(!b->rx_ring.length&&!b->tx_queue->count);
 assert(ambient_ble_read(b,out,sizeof(out),&length)==ESP_ERR_INVALID_STATE&&length==0);
 e.subscribe.cur_notify=true;ambient_ble_gap_event(&e,b);assert(ambient_ble_send(b,frame,1)==ESP_OK);
 fake.desc.sec_state.encrypted=false;e=(struct ble_gap_event){.type=BLE_GAP_EVENT_ENC_CHANGE,.enc_change={.conn_handle=1}};
 ambient_ble_gap_event(&e,b);assert(!b->tx_queue->count&&!b->link.encrypted);
 finish(b);
}
static void pairing_unpair_store(void) {
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
 struct ble_gap_event e={.type=BLE_GAP_EVENT_PASSKEY_ACTION,.passkey={.conn_handle=1,.params={.action=BLE_SM_IOACT_DISP}}};
 ambient_ble_gap_event(&e,b);assert(fake.terminations==1&&!b->pending_pair_approved);
 struct ble_gap_event rejected={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 ambient_ble_gap_event(&rejected,b);authorize(b);
 uint8_t identity[6]={1};fake.nvs_commit=1;assert(!ambient_ble_persist_accepted_peer(b,0,identity));fake.nvs_commit=0;
 assert(ambient_ble_persist_accepted_peer(b,0,identity));
 fake.has_peer=fake.has_bond=true;fake.nvs_erase=1;
 assert(ambient_ble_unpair_local(b)==ESP_FAIL&&b->unpairing);
 uint8_t value=1,out[1];size_t n;assert(ambient_ble_send(b,&value,1)==ESP_ERR_INVALID_STATE);assert(ambient_ble_read(b,out,1,&n)==ESP_ERR_INVALID_STATE);
 fake.nvs_erase=0;fake.bond_enum=1;assert(ambient_ble_unpair_local(b)==ESP_FAIL&&b->unpairing);
 fake.bond_enum=0;fake.bond_delete=1;assert(ambient_ble_unpair_local(b)==ESP_FAIL&&b->unpairing);
 fake.bond_delete=0;fake.terminate=1;assert(ambient_ble_unpair_local(b)==ESP_FAIL&&b->unpairing);
 fake.terminate=0;assert(ambient_ble_unpair_local(b)==ESP_OK&&!b->unpairing);finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.fail_lock=fake.lock_calls+1;
 assert(ambient_ble_unpair_local(b)==ESP_ERR_TIMEOUT&&b->unpairing);
 assert(ambient_ble_send(b,&value,1)==ESP_ERR_INVALID_STATE);fake.fail_lock=0;assert(ambient_ble_unpair_local(b)==ESP_OK);finish(b);
}
static bool approve(void *ctx,const uint8_t peer[6],uint8_t type,uint32_t passkey)
{ (void)ctx;(void)peer;(void)type;(void)passkey;return true; }
static void worker_trust_faults(void) {
 ambient_fake_reset();config.pairing_approval=approve;ambient_ble_t*b=start();authorize(b);
 b->accepted_peer_valid=false;b->link.peer_identity_accepted=false;
 struct ble_gap_event pair={.type=BLE_GAP_EVENT_PASSKEY_ACTION,.passkey={.conn_handle=1,.params={.action=BLE_SM_IOACT_DISP}}};
 ambient_ble_gap_event(&pair,b);assert(b->pending_pair_approved);
 uint8_t filler=1;ambient_ble_tx_item_t queued={.kind=AMBIENT_BLE_TX_ITEM_FRAME};assert(xQueueSend(b->tx_queue,&queued,0));
 struct ble_gap_event secure={.type=BLE_GAP_EVENT_ENC_CHANGE,.enc_change={.conn_handle=1}};
 ambient_ble_gap_event(&secure,b);assert(fake.terminations==1&&!b->link.peer_identity_accepted);
 struct ble_gap_event disconnected={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 ambient_ble_gap_event(&disconnected,b);authorize(b);b->accepted_peer_valid=false;b->link.peer_identity_accepted=false;
 ambient_ble_clear_io(b);ambient_ble_gap_event(&pair,b);ambient_ble_gap_event(&secure,b);assert(b->tx_queue->count==1);
 fake.nvs_commit=1;ambient_fake_tx_once(b);assert(fake.terminations==2&&!b->link.connected&&!b->accepted_peer_valid);
 fake.nvs_commit=0;authorize(b);assert(ambient_ble_send(b,&filler,1)==ESP_OK);
 uint64_t old=b->link.link_incarnation;b->link.link_incarnation++;
 unsigned sent=fake.notifications;ambient_fake_tx_once(b);assert(fake.notifications==sent&&b->link.link_incarnation>old);
 finish(b);config.pairing_approval=NULL;
}
static void metrics(void){
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);ambient_ble_metrics_t m;
 assert(ambient_ble_get_metrics(b,&m)==ESP_OK&&!m.rx_bytes&&!m.rx_high_water_bytes&&!m.tx_high_water_slots);
 uint8_t bytes[1]={1};assert(ambient_ble_send(b,bytes,1)==ESP_OK);
 assert(ambient_ble_get_metrics(b,&m)==ESP_OK&&m.tx_high_water_slots==1&&!m.task_stack_available&&!m.mbuf_available);
 fake.fail_lock=fake.lock_calls+1;assert(ambient_ble_get_metrics(b,&m)==ESP_ERR_TIMEOUT);fake.fail_lock=0;
 assert(ambient_ble_get_metrics(NULL,&m)==ESP_ERR_INVALID_STATE);finish(b);
}
static void forced_advertising(void){
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
 struct ble_gap_event e={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 unsigned starts=fake.advertising_starts;
 assert(ambient_ble_unpair_local(b)==ESP_OK&&!b->link.connected);
 ambient_ble_gap_event(&e,b);assert(fake.advertising_starts==starts+1&&!b->forced_incarnation);
 ambient_ble_gap_event(&e,b);assert(fake.advertising_starts==starts+1);
 authorize(b);uint8_t byte=1;assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.notify=1;
 ambient_fake_tx_once(b);assert(!b->link.connected&&b->forced_incarnation);
 ambient_ble_gap_event(&e,b);assert(fake.advertising_starts==starts+2);fake.notify=0;
 authorize(b);assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.notify=1;ambient_fake_tx_once(b);
 uint64_t old=b->forced_incarnation;authorize(b);assert(b->link.link_incarnation>old);
 ambient_ble_gap_event(&e,b);assert(b->link.connected&&fake.advertising_starts==starts+2&&!b->forced_incarnation);
 fake.notify=0;assert(ambient_ble_unpair_local(b)==ESP_OK);b->shutting_down=true;
 ambient_ble_gap_event(&e,b);assert(fake.advertising_starts==starts+2);finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=BLE_HS_ENOTCONN;
 assert(ambient_ble_unpair_local(b)==ESP_OK&&fake.advertising_starts==1);fake.terminate=0;finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.nvs_erase=1;
 assert(ambient_ble_unpair_local(b)==ESP_FAIL&&b->unpairing);ambient_ble_gap_event(&e,b);assert(!fake.advertising_starts);
 fake.nvs_erase=0;assert(ambient_ble_unpair_local(b)==ESP_OK&&fake.advertising_starts==1);finish(b);
}
static void pending_disconnect(void){
 struct ble_gap_event e={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 uint8_t byte=1,out;size_t n;
 for(unsigned mode=0;mode<3;mode++){
  ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
  if(mode==1)assert(ambient_ble_unpair_local(b)==ESP_OK);
  if(mode==2){assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.notify=1;ambient_fake_tx_once(b);fake.notify=0;}
  fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&e,b);
  assert(b->disconnect_pending&&b->pending_disconnect_event&&!fake.advertising_starts);
  fake.fail_lock=0;
  assert(ambient_ble_send(b,&byte,1)==ESP_ERR_INVALID_STATE);
  assert(ambient_ble_read(b,&out,1,&n)==ESP_ERR_INVALID_STATE&&n==0);
  ambient_fake_tx_once(b);
  assert(!b->disconnect_pending&&!b->link.connected&&fake.advertising_starts==1);
  ambient_ble_gap_event(&e,b);assert(fake.advertising_starts==1);finish(b);
 }
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
 assert(ambient_ble_unpair_local(b)==ESP_OK);
 /* Event is safely recorded during unpair; the subsequent resume lock fails. */
 b->unpairing=true;ambient_ble_gap_event(&e,b);assert(b->forced_disconnected&&b->disconnect_pending);
 b->unpairing=false;fake.fail_lock=fake.lock_calls+1;ambient_ble_resume_after_forced(b);
 assert(b->disconnect_pending&&!fake.advertising_starts);fake.fail_lock=0;
 ambient_fake_tx_once(b);assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);
 fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&e,b);fake.fail_lock=0;
 uint64_t previous=b->link.link_incarnation;
 /* A different new owner can be established before the delayed worker runs. */
 b->link.connected=false;authorize(b);assert(b->link.link_incarnation>previous);
 ambient_fake_tx_once(b);assert(b->link.connected&&!b->disconnect_pending&&!fake.advertising_starts);finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=BLE_HS_ENOTCONN;fake.terminate_lock_failures=2;
 assert(ambient_ble_unpair_local(b)==ESP_OK&&b->disconnect_pending&&!fake.advertising_starts);
 assert(ambient_ble_send(b,&byte,1)==ESP_ERR_INVALID_STATE);
 fake.terminate=0;fake.terminate_lock_failures=0;ambient_fake_tx_once(b);
 assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);
 fake.fail_lock=fake.lock_calls+1;assert(ambient_ble_unpair_local(b)==ESP_ERR_TIMEOUT);fake.fail_lock=0;
 ambient_ble_gap_event(&e,b);assert(b->disconnect_pending&&b->unpairing&&!fake.advertising_starts);
 assert(ambient_ble_unpair_local(b)==ESP_OK);assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 /* A newer real disconnect supersedes the older deferred slot. */
 ambient_fake_reset();b=start();authorize(b);
 fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&e,b);fake.fail_lock=0;
 b->link.connected=false;authorize(b);ambient_ble_gap_event(&e,b);
 assert(!b->link.connected&&!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);
 fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&e,b);fake.fail_lock=0;b->shutting_down=true;
 ambient_ble_process_disconnect(b);assert(!fake.advertising_starts);finish(b);
}
static void tx_uncertainty(void){
 struct ble_gap_event e={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 uint8_t byte=1,out,peer[6]={1};size_t n;
 for(unsigned already_gone=0;already_gone<2;already_gone++){
  ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
  assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.notify=1;fake.notify_lock_failures=2;
  fake.terminate=already_gone?BLE_HS_ENOTCONN:0;
  ambient_fake_tx_once(b);
  assert(!b->unpairing&&b->disconnect_pending&&b->pending_disconnect_terminate&&b->link.connected);
  assert(ambient_ble_send(b,&byte,1)==ESP_ERR_INVALID_STATE);
  assert(ambient_ble_read(b,&out,1,&n)==ESP_ERR_INVALID_STATE&&!n);
  if(b->disconnect_pending)assert(!ambient_ble_persist_accepted_peer(b,0,peer));
  struct os_mbuf mb={.len=1};struct ble_gatt_access_ctxt access={BLE_GATT_ACCESS_OP_WRITE_CHR,&mb};
  assert(b->gatt_chars[0].access_cb(1,0,&access,b)!=0);
  fake.notify=0;fake.notify_lock_failures=0;ambient_fake_tx_once(b);
  assert(!b->link.connected&&!b->unpairing&&fake.terminations==1);
  if(!already_gone){
   assert(b->disconnect_pending&&!fake.advertising_starts);
   ambient_ble_request_disconnect(b,1,b->published_incarnation);
   ambient_ble_request_disconnect(b,1,b->published_incarnation);
   assert(fake.terminations==1&&b->disconnect_pending&&!fake.advertising_starts);
   ambient_ble_gap_event(&e,b);
  }
  assert(!b->disconnect_pending&&fake.advertising_starts==1);
  ambient_ble_gap_event(&e,b);ambient_fake_tx_once(b);assert(fake.advertising_starts==1);finish(b);
 }
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
 assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.fail_lock=fake.lock_calls+1;
 ambient_fake_tx_once(b);fake.fail_lock=0;
 assert(!b->link.connected&&b->disconnect_pending&&!b->unpairing&&fake.terminations==1);
 ambient_ble_gap_event(&e,b);assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);uint8_t frame[95]={1};
 assert(ambient_ble_send(b,frame,sizeof(frame))==ESP_OK);fake.notify_lock_failures=1;
 ambient_fake_tx_once(b);fake.notify_lock_failures=0;
 assert(fake.notifications==1&&fake.terminations==1&&b->disconnect_pending&&!b->link.connected&&!b->unpairing);
 ambient_ble_gap_event(&e,b);assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);uint64_t old=b->link.link_incarnation;
 assert(ambient_ble_send(b,&byte,1)==ESP_OK);
 ambient_ble_tx_item_t stale;memcpy(&stale,b->tx_queue->bytes,sizeof(stale));
 b->link.connected=false;authorize(b);
 /* Model an old worker-local item surviving the new link's queue cleanup. */
 assert(!b->tx_queue->count&&xQueueSend(b->tx_queue,&stale,0));
 assert(b->link.link_incarnation>old);fake.fail_lock=fake.lock_calls+1;ambient_fake_tx_once(b);fake.fail_lock=0;
 assert(b->link.connected&&!b->disconnect_pending&&!fake.terminations&&!fake.advertising_starts);
 ambient_ble_request_disconnect(b,1,old);assert(!b->disconnect_pending&&!fake.terminations);finish(b);
}
static void termination_budget(void){
 struct ble_gap_event disconnected={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 struct ble_gap_event pair={.type=BLE_GAP_EVENT_PASSKEY_ACTION,.passkey={.conn_handle=1,.params={.action=BLE_SM_IOACT_DISP}}};
 struct ble_gap_event secure={.type=BLE_GAP_EVENT_ENC_CHANGE,.enc_change={.conn_handle=1}};
 uint8_t byte=1;
 for(unsigned path=0;path<9;path++){
  ambient_fake_reset();ambient_ble_t*b=start();fake.terminate=ESP_FAIL;
  if(path==0)fake.security_initiate=ESP_FAIL;
  authorize(b);
  if(path==1)ambient_ble_gap_event(&pair,b);
  if(path==2){secure.enc_change.status=1;ambient_ble_gap_event(&secure,b);secure.enc_change.status=0;}
  if(path==3){fake.desc.sec_state.encrypted=false;ambient_ble_gap_event(&secure,b);}
  if(path==4){
   b->accepted_peer_valid=false;b->link.peer_identity_accepted=false;b->config.pairing_approval=approve;
   ambient_ble_gap_event(&pair,b);ambient_ble_tx_item_t filler={.kind=AMBIENT_BLE_TX_ITEM_FRAME};
   assert(xQueueSend(b->tx_queue,&filler,0));ambient_ble_gap_event(&secure,b);
  }
  if(path==5){b->config.pairing_approval=approve;fake.passkey_inject=ESP_FAIL;ambient_ble_gap_event(&pair,b);}
  if(path==6){assert(ambient_ble_send(b,&byte,1)==ESP_OK);fake.notify=ESP_FAIL;ambient_fake_tx_once(b);}
  if(path==7){struct ble_gap_event repeat={.type=BLE_GAP_EVENT_REPEAT_PAIRING,.repeat_pairing={.conn_handle=1}};
   assert(ambient_ble_gap_event(&repeat,b)==BLE_GAP_REPEAT_PAIRING_IGNORE);}
  if(path==8){fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&secure,b);fake.fail_lock=0;}
  assert(!b->link.connected&&!b->unpairing&&b->termination_attempts==1&&b->termination_retry&&b->disconnect_pending);
  uint64_t incarnation=b->published_incarnation;
  ambient_ble_request_disconnect(b,1,incarnation);ambient_ble_request_disconnect(b,1,incarnation);
  assert(b->termination_attempts==1&&fake.terminations==1);
  fake.terminate=0;fake.security_initiate=0;fake.passkey_inject=0;fake.notify=0;fake.ticks+=100;
  ambient_fake_tx_once(b);assert(b->termination_attempts==2&&!b->termination_retry&&b->disconnect_pending);
  ambient_ble_gap_event(&disconnected,b);assert(!b->disconnect_pending&&!b->termination_fault&&fake.advertising_starts==1);
  ambient_ble_gap_event(&disconnected,b);ambient_fake_tx_once(b);assert(fake.terminations==2&&fake.advertising_starts==1);finish(b);
 }
 ambient_fake_reset();ambient_ble_t*b=start();authorize(b);fake.terminate=ESP_FAIL;
 ambient_ble_request_disconnect(b,1,b->published_incarnation);
 for(unsigned i=0;i<8;i++){ambient_ble_request_disconnect(b,1,b->published_incarnation);fake.ticks+=100;ambient_fake_tx_once(b);}
 assert(fake.terminations==AMBIENT_BLE_TERMINATION_ATTEMPTS&&b->termination_fault&&!b->termination_retry&&b->disconnect_pending&&!b->unpairing);
 ambient_ble_metrics_t metrics;assert(ambient_ble_get_metrics(b,&metrics)==ESP_OK&&metrics.termination_fault&&metrics.termination_attempts==3);
 assert(ambient_ble_send(b,&byte,1)==ESP_ERR_INVALID_STATE&&!fake.advertising_starts);
 ambient_ble_gap_event(&disconnected,b);assert(!b->termination_fault&&!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=ESP_FAIL;
 ambient_ble_request_disconnect(b,1,b->published_incarnation);fake.terminate=BLE_HS_ENOTCONN;fake.ticks+=100;
 ambient_fake_tx_once(b);assert(fake.terminations==2&&!b->disconnect_pending&&fake.advertising_starts==1);fake.terminate=0;finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=ESP_FAIL;
 ambient_ble_request_disconnect(b,1,b->published_incarnation);ambient_ble_gap_event(&disconnected,b);fake.ticks+=100;
 ambient_fake_tx_once(b);assert(fake.terminations==1&&fake.advertising_starts==1);fake.terminate=0;finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=ESP_FAIL;
 uint64_t old=b->published_incarnation;ambient_ble_request_disconnect(b,1,old);fake.terminate=0;authorize(b);fake.ticks+=100;
 ambient_fake_tx_once(b);assert(b->link.connected&&b->published_incarnation>old&&fake.terminations==1&&!fake.advertising_starts);finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=ESP_FAIL;fake.ticks=UINT32_MAX-50U;
 ambient_ble_request_disconnect(b,1,b->published_incarnation);fake.terminate=BLE_HS_ENOTCONN;fake.ticks+=100;
 ambient_fake_tx_once(b);assert(fake.terminations==2&&!b->disconnect_pending&&fake.advertising_starts==1);fake.terminate=0;finish(b);
 ambient_fake_reset();b=start();authorize(b);fake.terminate=ESP_FAIL;
 ambient_ble_request_disconnect(b,1,b->published_incarnation);b->shutting_down=true;fake.ticks+=100;
 ambient_ble_worker_disconnect(b);assert(fake.terminations==1&&!fake.advertising_starts);fake.terminate=0;finish(b);
}
static void provisional_connections(void){
 struct ble_gap_event connected={.type=BLE_GAP_EVENT_CONNECT,.connect={.conn_handle=1}};
 struct ble_gap_event disconnected={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 uint8_t byte=1,out,peer[6]={1};size_t n;
 for(unsigned mode=0;mode<5;mode++){
  ambient_fake_reset();ambient_ble_t*b=start();
  if(mode==1 || mode==3 || mode==4)fake.terminate=ESP_FAIL;
  if(mode==2)fake.terminate=BLE_HS_ENOTCONN;
  fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&connected,b);fake.fail_lock=0;
  assert(!fake.security_initiations&&!b->published_incarnation&&!b->next_link_incarnation&&!b->link.connected&&!b->unpairing);
  assert(ambient_ble_send(b,&byte,1)==ESP_ERR_INVALID_STATE);
  assert(ambient_ble_read(b,&out,1,&n)==ESP_ERR_INVALID_STATE&&!n);
  if(b->disconnect_pending)assert(!ambient_ble_persist_accepted_peer(b,0,peer));
  struct os_mbuf mb={.len=1};struct ble_gatt_access_ctxt access={BLE_GATT_ACCESS_OP_WRITE_CHR,&mb};
  assert(b->gatt_chars[0].access_cb(1,0,&access,b)!=0);
  if(mode==0){assert(b->provisional_active&&fake.terminations==1);ambient_ble_gap_event(&disconnected,b);}
  if(mode==1){fake.terminate=0;fake.ticks+=100;ambient_fake_tx_once(b);assert(fake.terminations==2);ambient_ble_gap_event(&disconnected,b);}
  if(mode==3){for(unsigned i=0;i<5;i++){fake.ticks+=100;ambient_fake_tx_once(b);}
   assert(fake.terminations==3&&b->termination_fault&&b->disconnect_pending&&!fake.advertising_starts);
   ambient_ble_gap_event(&disconnected,b);
  }
  if(mode==4){ambient_ble_gap_event(&disconnected,b);fake.ticks+=100;ambient_fake_tx_once(b);assert(fake.terminations==1);}
  assert(!b->provisional_active&&!b->disconnect_pending&&!b->termination_fault&&fake.advertising_starts==1);
  ambient_ble_gap_event(&disconnected,b);assert(fake.advertising_starts==1);fake.terminate=0;finish(b);
 }
 for(unsigned mode=0;mode<3;mode++){
  ambient_fake_reset();ambient_ble_t*b=start();b->next_link_incarnation=UINT64_MAX;
  fake.terminate=mode==0?0:mode==1?BLE_HS_ENOTCONN:ESP_FAIL;
  ambient_ble_gap_event(&connected,b);
  assert(b->next_link_incarnation==UINT64_MAX&&!b->published_incarnation&&!b->link.link_incarnation&&!fake.security_initiations);
  if(mode==2){fake.terminate=0;fake.ticks+=100;ambient_fake_tx_once(b);assert(fake.terminations==2);}
  if(mode!=1)ambient_ble_gap_event(&disconnected,b);
  assert(!b->disconnect_pending&&fake.advertising_starts==1);fake.terminate=0;finish(b);
 }
 ambient_fake_reset();ambient_ble_t*b=start();fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&connected,b);fake.fail_lock=0;
 uint64_t serial=b->provisional_serial;authorize(b);uint64_t new_inc=b->link.link_incarnation;
 assert(b->link.connected&&!b->provisional_active&&b->provisional_serial>serial);
 ambient_ble_gap_event(&disconnected,b);fake.ticks+=100;ambient_fake_tx_once(b);
 assert(b->link.connected&&b->link.link_incarnation==new_inc&&!b->disconnect_pending&&fake.terminations==1&&!fake.advertising_starts);finish(b);
 ambient_fake_reset();b=start();fake.terminate=ESP_FAIL;fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&connected,b);fake.fail_lock=0;
 b->shutting_down=true;fake.ticks+=100;ambient_ble_worker_disconnect(b);ambient_ble_gap_event(&disconnected,b);
 assert(fake.terminations==1&&!fake.advertising_starts);fake.terminate=0;finish(b);
 ambient_fake_reset();b=start();fake.fail_lock_remaining=2;ambient_ble_gap_event(&connected,b);
 assert(b->provisional_active&&b->pending_disconnect_event&&!fake.security_initiations);
 authorize(b);new_inc=b->link.link_incarnation;ambient_fake_tx_once(b);
 assert(b->link.connected&&b->link.link_incarnation==new_inc&&!b->disconnect_pending&&!fake.terminations);finish(b);
 ambient_fake_reset();b=start();b->provisional_serial=UINT64_MAX;fake.terminate=BLE_HS_ENOTCONN;
 ambient_ble_gap_event(&connected,b);assert(b->provisional_serial==UINT64_MAX&&!fake.security_initiations&&!b->next_link_incarnation&&fake.advertising_starts==1);
 fake.terminate=0;finish(b);
 /* Failure to clean old facts does not permit handle reuse to reuse incarnation. */
 ambient_fake_reset();b=start();authorize(b);new_inc=b->link.link_incarnation;authorize(b);
 assert(b->link.link_incarnation>new_inc&&!b->provisional_active);finish(b);
}
static void provisional_observed_retirement(void){
 struct ble_gap_event connected={.type=BLE_GAP_EVENT_CONNECT,.connect={.conn_handle=1}};
 struct ble_gap_event disconnected={.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
 /* An already observed old callback must not mask a future current callback. */
 ambient_fake_reset();ambient_ble_t*b=start();
 fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&connected,b);fake.fail_lock=0;
 uint64_t old_serial=b->provisional_serial;
 fake.fail_lock=fake.lock_calls+1;ambient_ble_gap_event(&disconnected,b);fake.fail_lock=0;
 assert(b->pending_disconnect_event&&b->pending_disconnect_provisional&&!b->pending_disconnect_terminate);
 assert(b->pending_disconnect_incarnation==old_serial);
 authorize(b);uint64_t current=b->link.link_incarnation;
 ambient_fake_tx_once(b);
 assert(!b->retired_provisional_serial&&b->link.connected&&b->link.link_incarnation==current);
 assert(!b->disconnect_pending&&!fake.advertising_starts&&fake.terminations==1);
 ambient_ble_gap_event(&disconnected,b);
 assert(!b->link.connected&&!b->disconnect_pending&&fake.advertising_starts==1);
 ambient_ble_gap_event(&disconnected,b);assert(fake.advertising_starts==1);finish(b);
 /* A terminate request is not evidence that the old callback arrived. */
 ambient_fake_reset();b=start();fake.fail_lock_remaining=2;ambient_ble_gap_event(&connected,b);
 old_serial=b->provisional_serial;
 assert(b->pending_disconnect_event&&b->pending_disconnect_terminate);
 authorize(b);current=b->link.link_incarnation;ambient_fake_tx_once(b);
 assert(b->retired_provisional_serial==old_serial&&b->link.connected&&!fake.terminations);
 ambient_ble_gap_event(&disconnected,b);
 assert(!b->retired_provisional_serial&&b->link.connected&&b->link.link_incarnation==current&&!fake.advertising_starts);
 ambient_ble_gap_event(&disconnected,b);
 assert(!b->link.connected&&!b->disconnect_pending&&fake.advertising_starts==1);
 ambient_ble_gap_event(&disconnected,b);assert(fake.advertising_starts==1);finish(b);
}
static void link_observation_timeout(void){
 for(unsigned path=0;path<4;path++){
  ambient_fake_reset();ambient_ble_t*b=start();authorize(b);
  struct ble_gap_event e={0};
  if(path==0)e=(struct ble_gap_event){.type=BLE_GAP_EVENT_SUBSCRIBE,.subscribe={.conn_handle=1,.attr_handle=b->tx_value_handle,.cur_notify=false}};
  if(path==1)e=(struct ble_gap_event){.type=BLE_GAP_EVENT_ENC_CHANGE,.enc_change={.conn_handle=1}};
  if(path==2)e=(struct ble_gap_event){.type=BLE_GAP_EVENT_MTU,.mtu={.conn_handle=1,.channel_id=BLE_L2CAP_CID_ATT,.value=23}};
  if(path==3)e=(struct ble_gap_event){.type=BLE_GAP_EVENT_IDENTITY_RESOLVED,.identity_resolved={.conn_handle=1}};
  fake.fail_lock=fake.lock_calls+(path==1?2:1);ambient_ble_gap_event(&e,b);fake.fail_lock=0;
  assert(!b->link.connected&&b->disconnect_pending&&fake.terminations==1);
  e=(struct ble_gap_event){.type=BLE_GAP_EVENT_DISCONNECT,.disconnect={.conn={.conn_handle=1}}};
  ambient_ble_gap_event(&e,b);assert(!b->disconnect_pending&&fake.advertising_starts==1);finish(b);
 }
}
int main(void){provisional_observed_retirement();link_observation_timeout();provisional_connections();termination_budget();tx_uncertainty();pending_disconnect();forced_advertising();metrics();startup_faults();stop_retry();data_and_gap();pairing_unpair_store();worker_trust_faults();puts("R3 production BLE platform fault tests: PASS (fake APIs, actual sources)");}
