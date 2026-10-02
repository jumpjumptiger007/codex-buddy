#include "companion_ble_central.h"
#include "companion_r2_fixture.h"
#include "ambient_auth_fake.h"
#include <assert.h>
#include <stdio.h>
typedef struct {uint32_t epoch;uint8_t output[516];size_t length, writes;ambient_transport_result_t result;} platform_t;
static bool read_epoch(void*ctx,uint32_t*v){*v=((platform_t*)ctx)->epoch;return true;}
static bool write_epoch(void*ctx,uint32_t v){((platform_t*)ctx)->epoch=v;return true;}
static bool random_part(void*ctx,uint32_t*v){(void)ctx;*v=42;return true;}
static ambient_transport_result_t write_bytes(void*ctx,const uint8_t*b,size_t n){platform_t*p=ctx;p->writes++;if(p->result)return p->result;assert(n<=20&&p->length+n<=sizeof(p->output));memcpy(p->output+p->length,b,n);p->length+=n;return AMBIENT_TRANSPORT_OK;}
static void discover(companion_ble_central_t*c){uint64_t id=c->link.link_incarnation;assert(!companion_ble_central_service(c,id,1,"wrong"));assert(companion_ble_central_service(c,id,1,COMPANION_BLE_SERVICE_UUID));assert(!companion_ble_central_characteristic(c,id,1,"wrong",COMPANION_BLE_RX_UUID,true,false));assert(!companion_ble_central_characteristic(c,id,1,COMPANION_BLE_SERVICE_UUID,"wrong",true,true));assert(companion_ble_central_characteristic(c,id,1,COMPANION_BLE_SERVICE_UUID,COMPANION_BLE_RX_UUID,true,false));assert(companion_ble_central_characteristic(c,id,1,COMPANION_BLE_SERVICE_UUID,COMPANION_BLE_TX_UUID,false,true));}
static void drain(companion_ble_central_t*c){assert(companion_ble_central_pump(c,c->link.link_incarnation,c->peer_token,0,1)==AMBIENT_TRANSPORT_OK);assert(!c->tx.length&&!c->rx.length);}
static void authenticate(companion_ble_central_t*c,platform_t*p,auth_fake_t*host,auth_fake_t*device){
 uint8_t key[65];fake_key(key,9);ambient_auth_crypto_t hc=fake_crypto(host),dc=fake_crypto(device);ambient_auth_t d={0};uint64_t id=c->link.link_incarnation;
 assert(companion_ble_central_authenticate(c,id,1,key,false,&hc));assert(!c->bridge.active);p->result=AMBIENT_TRANSPORT_WOULD_BLOCK;size_t attempts=p->writes;
 assert(companion_ble_central_pump(c,id,1,0,1)==AMBIENT_TRANSPORT_WOULD_BLOCK);
 assert(c->tx.length==38&&p->length==0&&p->writes==attempts+1);
 p->result=AMBIENT_TRANSPORT_OK;drain(c);assert(p->length==38);
 assert(ambient_auth_open(&d,true,id,true,key,false,&dc));assert(ambient_auth_feed(&d,id,true,p->output,p->length));p->length=0;
 size_t n;const uint8_t*response=ambient_auth_output(&d,&n);assert(n==102);assert(companion_ble_central_rx(c,id-1,1,response,n)==AMBIENT_TRANSPORT_DISCONNECTED);
 for(size_t offset=0;offset<n;offset+=20){size_t chunk=n-offset<20?n-offset:20;
  assert(companion_ble_central_rx(c,id,1,response+offset,chunk)==AMBIENT_TRANSPORT_OK);
  drain(c);assert(!c->rx.length);if(offset+chunk<n)assert(!c->bridge.active&&p->length==0);
 }
 assert(c->bridge.active&&p->length>38);
 assert(ambient_auth_output_committed(&d));assert(ambient_auth_feed(&d,id,true,p->output,38));assert(ambient_auth_complete(&d,id));
}
int main(void){
 companion_runtime_t runtime={0};companion_runtime_lease_t lease={0};companion_runtime_options_t options=r2_options(&lease,1,NULL);assert(companion_runtime_start(&runtime,&options));
 platform_t platform={0};ambient_generation_backend_t backend={read_epoch,write_epoch,random_part,&platform};companion_ble_central_platform_t ops={write_bytes,&platform,20};companion_ble_central_t c={0};auth_fake_t host={.key=9},device={.key=9};
 assert(companion_ble_central_begin(&c,1,&runtime,&backend,NULL));uint64_t id=c.link.link_incarnation;
 assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_DISCONNECTED);
 assert(!companion_ble_central_bind(&c,id-1,1,&ops));assert(companion_ble_central_bind(&c,id,1,&ops));
 assert(!companion_ble_central_bind(&c,id,1,&ops));discover(&c);uint8_t key[65];fake_key(key,9);ambient_auth_crypto_t crypto=fake_crypto(&host);
 assert(!companion_ble_central_authenticate(&c,id,1,key,false,&crypto));assert(companion_ble_central_subscription(&c,id,1,true));assert(!c.bridge.active&&platform.epoch==0);assert(ambient_transport_send(&c.transport,key,1)==AMBIENT_TRANSPORT_DISCONNECTED);
 authenticate(&c,&platform,&host,&device);assert(platform.epoch==1&&!runtime.wire.ready);uint64_t generation=c.bridge.generation;
 ambient_wire_message_t first;assert(ambient_wire_decode(platform.output+38,platform.length-39,&first)&&first.kind==AMBIENT_WIRE_HELLO&&first.generation==generation);platform.length=0;
 uint8_t frame[129];size_t length;assert(ambient_wire_encode(&first,frame,sizeof(frame),&length));
 assert(companion_ble_central_rx(&c,id,2,frame,1)==AMBIENT_TRANSPORT_DISCONNECTED);
 assert(companion_ble_central_rx(&c,id,1,frame,length/2)==AMBIENT_TRANSPORT_OK);assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_OK&&!runtime.wire.ready);
 assert(companion_ble_central_rx(&c,id,1,frame+length/2,length-length/2)==AMBIENT_TRANSPORT_OK);assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_OK&&runtime.wire.ready);assert(c.tx.length==95);
 platform.result=AMBIENT_TRANSPORT_WOULD_BLOCK;size_t pending=c.tx.length;assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_WOULD_BLOCK&&c.tx.length==pending);platform.result=AMBIENT_TRANSPORT_OK;
 /* Production write-ready event uses this exact pump; no new RX required. */
 assert(companion_ble_central_pump(&c,id-1,1,0,1)==AMBIENT_TRANSPORT_DISCONNECTED&&c.tx.length==pending);
 assert(companion_ble_central_pump(&c,id,1,0,1)==AMBIENT_TRANSPORT_OK&&!c.tx.length&&platform.length==95);assert(c.last_frame_bytes==95&&c.last_frame_chunks==5);
 platform.length=0;
 /* Coalescing must not claim an isolated frame or double count blocked writes. */
 size_t completed_bytes=c.last_frame_bytes, completed_chunks=c.last_frame_chunks;
 assert(ambient_transport_send(&c.transport,frame,1)==AMBIENT_TRANSPORT_OK);
 assert(ambient_transport_send(&c.transport,frame,1)==AMBIENT_TRANSPORT_OK);
 platform.result=AMBIENT_TRANSPORT_WOULD_BLOCK;uint64_t writes=c.platform_writes;
 assert(companion_ble_central_pump(&c,id,1,0,1)==AMBIENT_TRANSPORT_WOULD_BLOCK&&c.platform_writes==writes);
 platform.result=AMBIENT_TRANSPORT_OK;drain(&c);
 assert(c.last_frame_bytes==completed_bytes&&c.last_frame_chunks==completed_chunks);platform.length=0;
 /* Tiny generic platform chunk demonstrates the hard callback work cap. */
 c.platform.write_limit=1;uint8_t bounded[129]={1};
 for(unsigned i=0;i<4;i++)assert(ambient_transport_send(&c.transport,bounded,129)==AMBIENT_TRANSPORT_OK);
 size_t calls=platform.writes;
 assert(companion_ble_central_pump(&c,id,1,0,1)==AMBIENT_TRANSPORT_WOULD_BLOCK);
 assert(platform.writes-calls==COMPANION_BLE_PUMP_TICKS&&c.tx.length==516-COMPANION_BLE_PUMP_TICKS);
 memset(&c.tx,0,sizeof(c.tx));platform.length=0;c.platform.write_limit=20;
 uint8_t filler[130]={1};assert(companion_ble_central_rx(&c,id,1,filler,130)==AMBIENT_TRANSPORT_TOO_LARGE);
 for(unsigned i=0;i<4;i++)assert(companion_ble_central_rx(&c,id,1,filler,129)==AMBIENT_TRANSPORT_OK);assert(companion_ble_central_rx(&c,id,1,filler,1)==AMBIENT_TRANSPORT_WOULD_BLOCK);
 while(c.tx.length+129<=516)assert(ambient_transport_send(&c.transport,filler,129)==AMBIENT_TRANSPORT_OK);assert(ambient_transport_send(&c.transport,filler,129)==AMBIENT_TRANSPORT_WOULD_BLOCK);
 companion_ble_central_disconnect(&c,id-1,1);assert(c.alive);companion_ble_central_disconnect(&c,id,1);assert(!c.alive&&!runtime.wire.ready&&!c.tx.length&&!c.rx.length&&!c.auth.incarnation);
 size_t old_calls=platform.writes;assert(companion_ble_central_pump(&c,id,1,0,1)==AMBIENT_TRANSPORT_DISCONNECTED&&platform.writes==old_calls);assert(!companion_ble_central_subscription(&c,id,1,true));
 assert(companion_ble_central_begin(&c,1,&runtime,&backend,&ops));assert(c.link.link_incarnation>id);id=c.link.link_incarnation;discover(&c);assert(companion_ble_central_subscription(&c,id,1,true));platform.length=0;authenticate(&c,&platform,&host,&device);assert(c.bridge.generation>generation);
 platform.result=AMBIENT_TRANSPORT_DISCONNECTED;assert(ambient_transport_send(&c.transport,filler,1)==AMBIENT_TRANSPORT_OK);assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_DISCONNECTED);assert(!c.alive&&!c.bridge.active);
 platform.result=AMBIENT_TRANSPORT_OK;assert(companion_ble_central_begin(&c,1,&runtime,&backend,&ops));discover(&c);id=c.link.link_incarnation;assert(companion_ble_central_subscription(&c,id,1,true));assert(companion_ble_central_authenticate(&c,id,1,key,false,&crypto));uint32_t epoch=platform.epoch;
 assert(companion_ble_central_rx(&c,id,1,frame,length)==AMBIENT_TRANSPORT_OK);assert(companion_ble_central_tick(&c,0,1)==AMBIENT_TRANSPORT_INVALID);assert(platform.epoch==epoch&&!runtime.wire.ready);
 /* Notification loss is fatal both before and after authenticated R1. */
 for(unsigned fault=0;fault<3;fault++){
  companion_ble_central_teardown(&c);assert(companion_ble_central_begin(&c,1,&runtime,&backend,&ops));
  discover(&c);id=c.link.link_incarnation;assert(companion_ble_central_subscription(&c,id,1,true));
  assert(companion_ble_central_authenticate(&c,id,1,key,false,&crypto));epoch=platform.epoch;
  if(fault==0){
   for(unsigned i=0;i<4;i++)assert(companion_ble_central_rx(&c,id,1,filler,129)==AMBIENT_TRANSPORT_OK);
   assert(companion_ble_central_notification(&c,id,1,filler,1)==AMBIENT_TRANSPORT_WOULD_BLOCK);
  }else if(fault==1)assert(companion_ble_central_notification(&c,id,1,filler,130)==AMBIENT_TRANSPORT_TOO_LARGE);
  else assert(companion_ble_central_notification(&c,id,1,filler,0)==AMBIENT_TRANSPORT_INVALID);
  assert(!c.alive&&!c.rx.length&&!c.tx.length&&!c.auth.incarnation&&!c.bridge.active&&platform.epoch==epoch);
 }
 assert(companion_ble_central_begin(&c,1,&runtime,&backend,&ops));discover(&c);id=c.link.link_incarnation;
 assert(companion_ble_central_subscription(&c,id,1,true));platform.length=0;authenticate(&c,&platform,&host,&device);
 first.generation=c.bridge.generation;assert(ambient_wire_encode(&first,frame,sizeof(frame),&length));
 platform.length=0;assert(companion_ble_central_notification(&c,id,1,frame,length)==AMBIENT_TRANSPORT_OK);drain(&c);assert(runtime.wire.ready);
 generation=c.bridge.generation;
 for(unsigned i=0;i<4;i++)assert(companion_ble_central_rx(&c,id,1,filler,129)==AMBIENT_TRANSPORT_OK);
 assert(companion_ble_central_notification(&c,id,1,filler,1)==AMBIENT_TRANSPORT_WOULD_BLOCK);
 assert(!c.alive&&!runtime.wire.ready&&!c.tx.length&&!c.rx.length);uint64_t prior=id;
 assert(companion_ble_central_begin(&c,1,&runtime,&backend,&ops));discover(&c);id=c.link.link_incarnation;
 assert(id>prior&&companion_ble_central_subscription(&c,id,1,true));
 assert(companion_ble_central_notification(&c,prior,1,NULL,0)==AMBIENT_TRANSPORT_DISCONNECTED&&c.alive);
 platform.length=0;authenticate(&c,&platform,&host,&device);assert(c.bridge.generation>generation);
 companion_ble_central_teardown(&c);companion_runtime_stop(&runtime);puts("R3 Mac central state/byte/auth/generation gating: PASS (synthetic crypto/platform)");
}
