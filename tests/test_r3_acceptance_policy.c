#include "r3_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 ambient_ble_link_facts_t facts={.connected=true,.tx_notify_subscribed=true,.encrypted=true,
 .authenticated=true,.bonded=true,.secure_connections=true,.peer_identity_valid=true,
 .peer_identity_accepted=true,.link_incarnation=4};
 ambient_session_link_t link=r3_acceptance_link(&facts);assert(r3_acceptance_ble_authorized(&link));
 assert(!link.application_authenticated && !link.auth_incarnation);
 bool *fields[]={&link.connected,&link.tx_notify_subscribed,&link.encrypted,&link.mitm_authenticated,
 &link.bonded,&link.secure_connections,&link.peer_identity_valid,&link.known_peer_accepted};
 for(unsigned i=0;i<8;i++){*fields[i]=false;assert(!r3_acceptance_ble_authorized(&link));*fields[i]=true;}
 link.link_incarnation=0;assert(!r3_acceptance_ble_authorized(&link));
 puts("R3 acceptance device real-facts mapping: PASS");
}
