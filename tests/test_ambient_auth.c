#include "ambient_auth_fake.h"
#include "ambient_identity.h"
#include "../acceptance/r3_mac/fingerprint.h"
#include <assert.h>
#include <stdio.h>
static void deliver(ambient_auth_t*from,ambient_auth_t*to,uint64_t id){size_t n;const uint8_t*p=ambient_auth_output(from,&n);uint8_t bytes[102];assert(p&&n);memcpy(bytes,p,n);assert(ambient_auth_output_committed(from));for(size_t i=0;i<n;i++)assert(ambient_auth_feed(to,id,true,bytes+i,1));}
static void begin(ambient_auth_t*h,ambient_auth_t*d,auth_fake_t*host,auth_fake_t*device,uint64_t id,bool enroll){uint8_t key[65];fake_key(key,device->key);ambient_auth_crypto_t hc=fake_crypto(host),dc=fake_crypto(device);assert(ambient_auth_open(d,true,id,true,key,enroll,&dc));assert(ambient_auth_open(h,false,id,true,enroll?NULL:key,enroll,&hc));}
static void complete(ambient_auth_t*h,ambient_auth_t*d,uint64_t id){deliver(h,d,id);deliver(d,h,id);deliver(h,d,id);assert(ambient_auth_complete(h,id)&&ambient_auth_complete(d,id));}
typedef struct {int status;bool mark,create,public_fail,sign_fail;unsigned creates;} identity_fake_t;
static int load(void*c){return((identity_fake_t*)c)->status;}
static bool mark(void*c){identity_fake_t*f=c;if(!f->mark)return false;f->status=-1;return true;}
static bool create(void*c){identity_fake_t*f=c;f->creates++;if(!f->create)return false;f->status=1;return true;}
static bool pub(void*c,uint8_t out[65]){if(((identity_fake_t*)c)->public_fail)return false;fake_key(out,9);return true;}
static bool sign_id(void*c,const uint8_t h[32],uint8_t sig[64]){(void)h;memset(sig,0,64);return !((identity_fake_t*)c)->sign_fail;}
static void identity_tests(void){
 identity_fake_t f={.mark=true,.create=true};ambient_identity_backend_t b={load,mark,create,pub,sign_id,&f};ambient_identity_t id={0};uint8_t key[65];assert(ambient_identity_open(&id,&b)&&f.creates==1&&ambient_identity_public(&id,key));ambient_identity_close(&id);assert(ambient_identity_open(&id,&b)&&f.creates==1);
 f.status=-1;assert(!ambient_identity_open(&id,&b)&&!id.ready&&f.creates==1);f.status=0;f.mark=false;assert(!ambient_identity_open(&id,&b));f.mark=true;f.create=false;assert(!ambient_identity_open(&id,&b));unsigned count=f.creates;f.create=true;assert(!ambient_identity_open(&id,&b)&&f.creates==count);f.status=1;f.public_fail=true;assert(!ambient_identity_open(&id,&b));
}
int main(void){
 {auth_fake_t fake={.key=9};ambient_auth_crypto_t crypto=fake_crypto(&fake);uint8_t key[65];fake_key(key,9);
 char original[25],lower[25],normalized[25];assert(ambient_auth_fingerprint(&crypto,key,original));
 assert(r3_fingerprint_normalize(original,normalized)&&!strcmp(original,normalized));
 for(unsigned i=0;i<25;i++)lower[i]=original[i]>='A'&&original[i]<='F'?(char)(original[i]-'A'+'a'):original[i];
 assert(r3_fingerprint_normalize(lower,normalized)&&!strcmp(original,normalized));
 lower[0]=lower[0]=='0'?'1':'0';assert(r3_fingerprint_normalize(lower,normalized)&&strcmp(original,normalized));
 assert(!r3_fingerprint_normalize("",normalized)&&!r3_fingerprint_normalize("01234567890123456789012Z",normalized));
 assert(!r3_fingerprint_normalize("0123456789012345678901234",normalized));}

 ambient_auth_t h={0},d={0};auth_fake_t host={.key=9},device={.key=9};uint8_t key[65];fake_key(key,9);ambient_auth_crypto_t hc=fake_crypto(&host);
 assert(!ambient_auth_open(&h,false,1,true,NULL,false,&hc));assert(!ambient_auth_open(&h,false,1,false,key,false,&hc));
 begin(&h,&d,&host,&device,1,false);assert(!ambient_auth_complete(&h,1));complete(&h,&d,1);
 uint8_t r1[]="R1";assert(!ambient_auth_feed(&h,1,true,r1,2));assert(!ambient_auth_complete(&h,1));
 begin(&h,&d,&host,&device,2,true);deliver(&h,&d,2);deliver(&d,&h,2);complete(&h,&d,2);assert(h.pinned);
 char fp[25],again[25];assert(ambient_auth_fingerprint(&hc,key,fp)&&ambient_auth_fingerprint(&hc,key,again)&&!strcmp(fp,again));
 for(unsigned fault=0;fault<9;fault++){
  begin(&h,&d,&host,&device,10+fault,false);size_t n;uint8_t challenge[102];memcpy(challenge,ambient_auth_output(&h,&n),38);deliver(&h,&d,10+fault);
  uint8_t response[102];const uint8_t*p=ambient_auth_output(&d,&n);memcpy(response,p,n);assert(ambient_auth_output_committed(&d));
  if(fault==0)response[6]^=1; /* Wrong nonce. */
  if(fault==1)response[70]^=1; /* Wrong-key signature. */
  if(fault==2)response[40]^=1;
  if(fault==3)response[2]=2;
  if(fault==4)response[4]=1;
  if(fault==5)response[0]='X';
  if(fault<6)assert(!ambient_auth_feed(&h,10+fault,true,response,n));
  if(fault==6)assert(!ambient_auth_feed(&h,9,true,response,n));
  if(fault==7)assert(!ambient_auth_feed(&d,17,true,challenge,38)); /* Repeated challenge. */
  if(fault==8){assert(ambient_auth_feed(&h,18,true,response,n));assert(!ambient_auth_feed(&h,18,true,response,n));}
 }
 begin(&h,&d,&host,&device,30,false);deliver(&h,&d,30);size_t n;uint8_t previous[102];memcpy(previous,ambient_auth_output(&d,&n),102);ambient_auth_close(&h);ambient_auth_close(&d);begin(&h,&d,&host,&device,31,false);deliver(&h,&d,31);assert(!ambient_auth_feed(&h,31,true,previous,n));
 begin(&h,&d,&host,&device,32,false);deliver(&h,&d,32);deliver(&d,&h,32);uint8_t confirmation[38];memcpy(confirmation,ambient_auth_output(&h,&n),38);assert(ambient_auth_output_committed(&h));confirmation[7]^=1;assert(!ambient_auth_feed(&d,32,true,confirmation,38));
 begin(&h,&d,&host,&device,33,false);assert(!ambient_auth_feed(&d,33,true,r1,2));
 host.fail_rng=true;assert(!ambient_auth_open(&h,false,34,true,key,false,&hc));host.fail_rng=false;
 begin(&h,&d,&host,&device,35,false);device.fail_rng=true;size_t length;const uint8_t*challenge=ambient_auth_output(&h,&length);assert(!ambient_auth_feed(&d,35,true,challenge,length));device.fail_rng=false;
 begin(&h,&d,&host,&device,36,false);device.fail_sign=true;challenge=ambient_auth_output(&h,&length);assert(!ambient_auth_feed(&d,36,true,challenge,length));device.fail_sign=false;
 for(unsigned store=0;store<2;store++){begin(&h,&d,&host,&device,40+store,true);deliver(&h,&d,40+store);host.deny_pin=store==0;host.fail_pin=store==1;const uint8_t*p=ambient_auth_output(&d,&length);assert(!ambient_auth_feed(&h,40+store,true,p,length));host.deny_pin=host.fail_pin=false;}
 identity_tests();puts("R3 auth/parser/identity state tests: PASS (explicit fake crypto, not cryptographic evidence)");
}
