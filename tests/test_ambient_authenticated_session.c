#include "ambient_authenticated_session.h"
#include "ambient_auth_fake.h"
#include "ambient_fake_transport.h"
#include <assert.h>
#include <stdio.h>
static ambient_session_link_t link(void){return(ambient_session_link_t){.connected=true,.tx_notify_subscribed=true,.encrypted=true,.mitm_authenticated=true,.bonded=true,.secure_connections=true,.peer_identity_valid=true,.known_peer_accepted=true,.link_incarnation=1};}
int main(void){
 ambient_authenticated_session_t device={0};ambient_session_link_t facts=link();auth_fake_t h={.key=9},d={.key=9};ambient_auth_crypto_t hc=fake_crypto(&h),dc=fake_crypto(&d);uint8_t key[65];fake_key(key,9);
 ambient_fake_transport_t fake;uint8_t storage[516];size_t lengths[4];ambient_transport_t transport;assert(ambient_fake_transport_init(&fake,storage,sizeof(storage),lengths,4,129,129,&transport));ambient_fake_transport_set_connected(&fake,true);ambient_auth_t host={0};
 assert(!ambient_session_open(&device.session,&facts));assert(ambient_authenticated_session_open(&device,&facts,key,false,&dc));assert(!device.session.active);
 uint8_t early[]="v1|H";assert(!ambient_authenticated_session_feed(&device,&facts,early,sizeof(early)-1,&transport)&&!device.session.active);
 assert(ambient_authenticated_session_open(&device,&facts,key,false,&dc));assert(ambient_auth_open(&host,false,1,true,key,false,&hc));size_t n;const uint8_t*out=ambient_auth_output(&host,&n);
 assert(ambient_authenticated_session_feed(&device,&facts,out,20,&transport));assert(ambient_authenticated_session_feed(&device,&facts,out+20,n-20,&transport));assert(ambient_auth_output_committed(&host));assert(!device.session.active);
 assert(ambient_authenticated_session_flush(&device,&facts,&transport)==AMBIENT_TRANSPORT_OK);uint8_t response[129];assert(ambient_transport_receive(&transport,response,sizeof(response),&n)==AMBIENT_TRANSPORT_OK);assert(ambient_auth_feed(&host,1,true,response,n));
 uint8_t combined[129];out=ambient_auth_output(&host,&n);memcpy(combined,out,n);size_t confirm=n;assert(ambient_auth_output_committed(&host)&&ambient_auth_complete(&host,1));
 ambient_wire_message_t hello={.kind=AMBIENT_WIRE_HELLO,.version=AMBIENT_WIRE_VERSION,.generation=123,.body.hello={.offered=AMBIENT_WIRE_CAP_KNOWN,.required=AMBIENT_WIRE_CAP_REQUIRED}};size_t hello_length;assert(ambient_wire_encode(&hello,combined+confirm,sizeof(combined)-confirm,&hello_length));
 assert(ambient_authenticated_session_feed(&device,&facts,combined,confirm+hello_length,&transport));assert(device.session.active&&device.session.generation==123&&device.session.wire.ready);assert(ambient_transport_receive(&transport,response,sizeof(response),&n)==AMBIENT_TRANSPORT_OK);ambient_wire_message_t reply;assert(ambient_wire_decode(response,n-1,&reply)&&reply.kind==AMBIENT_WIRE_HELLO&&reply.generation==123);
 assert(!ambient_authenticated_session_feed(&device,&facts,combined,confirm,&transport));assert(!device.session.active&&!ambient_auth_complete(&device.auth,1));
 assert(ambient_authenticated_session_open(&device,&facts,key,false,&dc));facts.link_incarnation=2;assert(!ambient_authenticated_session_feed(&device,&facts,combined,1,&transport)&&!device.session.active);facts=link();facts.encrypted=false;assert(!ambient_authenticated_session_open(&device,&facts,key,false,&dc));
 facts=link();assert(ambient_authenticated_session_open(&device,&facts,key,false,&dc));
 assert(ambient_auth_open(&host,false,1,true,key,false,&hc));out=ambient_auth_output(&host,&n);assert(ambient_authenticated_session_feed(&device,&facts,out,n,&transport));assert(ambient_auth_output_committed(&host));assert(ambient_authenticated_session_flush(&device,&facts,&transport)==AMBIENT_TRANSPORT_OK);assert(ambient_transport_receive(&transport,response,sizeof(response),&n)==AMBIENT_TRANSPORT_OK);assert(ambient_auth_feed(&host,1,true,response,n));out=ambient_auth_output(&host,&n);assert(ambient_authenticated_session_feed(&device,&facts,out,n,&transport));assert(ambient_auth_output_committed(&host));assert(device.session.active&&device.session.generation==0); /* Disconnect after auth, before R1. */
 ambient_authenticated_session_close(&device);assert(!device.session.active&&!device.auth.incarnation);puts("R3 Passport auth-to-R1 multiplex/order/incarnation tests: PASS (fake crypto)");
}
