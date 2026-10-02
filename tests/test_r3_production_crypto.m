#define _DARWIN_C_SOURCE
#import <Foundation/Foundation.h>
#include "companion_auth_crypto.h"
#include "companion_pin_store.h"
#include "ambient_identity.h"
#include "../acceptance/r3_mac/fingerprint.h"
#include "nvs.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static void hex(const char *text,uint8_t*out,size_t n){for(size_t i=0;i<n;i++){unsigned value;assert(sscanf(text+2*i,"%2x",&value)==1);out[i]=(uint8_t)value;}}
static int fault;
static ssize_t wr(int fd,const void*p,size_t n){return fault==1?-1:write(fd,p,n);}
static int syncf(int fd){return fault==2?-1:fsync(fd);}
static int syncd(int fd){return fault==3?-1:fsync(fd);}
static int replace(const char*a,const char*b){return fault==4?-1:rename(a,b);}
static bool approval(void*ctx,const uint8_t key[65],const char*fp){assert(key[0]==4&&strlen(fp)==24);return *(bool*)ctx;}
static void remove_store(const char*path){char lock[1100];snprintf(lock,sizeof(lock),"%s.lock",path);assert(unlink(path)==0);assert(unlink(lock)==0);}
static void send_auth(ambient_auth_t*from,ambient_auth_t*to){size_t n;const uint8_t*p=ambient_auth_output(from,&n);uint8_t data[102];assert(p&&n);memcpy(data,p,n);assert(ambient_auth_output_committed(from));assert(ambient_auth_feed(to,1,true,data,n));}
int main(void){@autoreleasepool {
 ambient_auth_crypto_t mac=companion_auth_crypto();uint8_t key[65],sig[64],digest[32];key[0]=4;
 /* Published non-secret RFC6979 A.2.5 SHA256("sample") verification vector. */
 hex("60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6",key+1,32);
 hex("7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299",key+33,32);
 hex("EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8",sig,64);
 assert(mac.hash(NULL,(const uint8_t*)"sample",6,digest));assert(mac.verify(NULL,key,digest,sig));sig[0]^=1;assert(!mac.verify(NULL,key,digest,sig));sig[0]^=1;digest[0]^=1;assert(!mac.verify(NULL,key,digest,sig));digest[0]^=1;
 uint8_t nonce[32],other[32];assert(mac.random(NULL,nonce,32)&&mac.random(NULL,other,32)&&memcmp(nonce,other,32));
 ambient_identity_esp_t*device=NULL,*duplicate=NULL;assert(ambient_identity_esp_open(&device));assert(!ambient_identity_esp_open(&duplicate));uint8_t device_key[65],retained[65];assert(ambient_identity_esp_public(device,device_key));ambient_auth_crypto_t esp=ambient_identity_esp_crypto(device);
 assert(esp.sign(device,digest,sig)&&mac.verify(NULL,device_key,digest,sig));assert(!mac.verify(NULL,key,digest,sig));
 char fp[25],same[25];assert(ambient_auth_fingerprint(&mac,device_key,fp)&&ambient_auth_fingerprint(&esp,device_key,same)&&!strcmp(fp,same));
 char canonical[25],lowercase[25];assert(r3_fingerprint_normalize(fp,canonical)&&!strcmp(fp,canonical));
 for(unsigned i=0;i<25;i++)lowercase[i]=fp[i]>='A'&&fp[i]<='F'?(char)(fp[i]-'A'+'a'):fp[i];
 assert(r3_fingerprint_normalize(lowercase,canonical)&&!strcmp(fp,canonical));
 lowercase[0]=lowercase[0]=='0'?'1':'0';assert(r3_fingerprint_normalize(lowercase,canonical)&&strcmp(fp,canonical));
 ambient_auth_t host={0},passport={0};assert(ambient_auth_open(&passport,true,1,true,device_key,false,&esp));assert(ambient_auth_open(&host,false,1,true,device_key,false,&mac));send_auth(&host,&passport);send_auth(&passport,&host);send_auth(&host,&passport);assert(ambient_auth_complete(&host,1)&&ambient_auth_complete(&passport,1));
 ambient_identity_esp_close(device);assert(ambient_identity_esp_open(&device)&&ambient_identity_esp_public(device,retained)&&!memcmp(device_key,retained,65));ambient_identity_esp_close(device);
 for(int i=1;i<=7;i++){fake_identity_marker=fake_identity_has_key=false;fake_identity_fault=i;assert(!ambient_identity_esp_open(&device));fake_identity_fault=0;}
 fake_identity_marker=fake_identity_has_key=false;assert(ambient_identity_esp_open(&device));ambient_identity_esp_close(device);
 fake_identity_record[50]^=1;assert(!ambient_identity_esp_open(&device));fake_identity_record[50]^=1;
 fake_identity_has_key=false;assert(!ambient_identity_esp_open(&device));fake_identity_has_key=true;
 /* New-store commit failure leaves creation marker, never auto-regenerates. */
 fake_identity_has_key=fake_identity_marker=false;fake_identity_fault=6;assert(!ambient_identity_esp_open(&device));fake_identity_fault=0;assert(!ambient_identity_esp_open(&device));
 fake_identity_marker=fake_identity_has_key=false;assert(ambient_identity_esp_open(&device));esp=ambient_identity_esp_crypto(device);unsigned draws=0;while(draws<20000&&esp.random(device,nonce,32))draws++;assert(draws<20000);ambient_identity_esp_close(device);
 char directory[]="/tmp/codex-pin-test.XXXXXX";assert(mkdtemp(directory));char path[1024];snprintf(path,sizeof(path),"%s/pin",directory);
 companion_pin_store_t store={0},second={0};bool approved=false;assert(companion_pin_store_open(&store,path,NULL));assert(!companion_pin_store_open(&second,path,NULL));assert(!companion_pin_store_key(&store,retained));assert(!companion_pin_store_enroll(&store,key,&mac,approval,&approved));approved=true;assert(companion_pin_store_enroll(&store,key,&mac,approval,&approved));assert(!companion_pin_store_enroll(&store,device_key,&mac,approval,&approved));companion_pin_store_close(&store);assert(companion_pin_store_open(&store,path,NULL)&&companion_pin_store_key(&store,retained)&&!memcmp(key,retained,65));companion_pin_store_close(&store);
 assert(chmod(path,0644)==0);assert(!companion_pin_store_open(&store,path,NULL));assert(chmod(path,0600)==0);remove_store(path);
 companion_generation_io_t ops={.write_bytes=wr,.sync_file=syncf,.sync_directory=syncd,.replace_file=replace};
 for(int i=1;i<=4;i++){fault=0;assert(companion_pin_store_open(&store,path,&ops));fault=i;assert(!companion_pin_store_enroll(&store,key,&mac,approval,&approved)&&store.poisoned&&!companion_pin_store_key(&store,retained));fault=0;companion_pin_store_close(&store);assert(companion_pin_store_open(&store,path,NULL));companion_pin_store_close(&store);remove_store(path);}
 assert(companion_pin_store_open(&store,path,NULL));companion_pin_store_close(&store);int fd=open(path,O_WRONLY|O_TRUNC);assert(fd>=0&&write(fd,"bad",3)==3&&close(fd)==0);assert(!companion_pin_store_open(&store,path,NULL));remove_store(path);
 assert(companion_pin_store_open(&store,path,NULL));companion_pin_store_close(&store);assert(unlink(path)==0);assert(!companion_pin_store_open(&store,path,NULL));char lock[1100];snprintf(lock,sizeof(lock),"%s.lock",path);assert(unlink(lock)==0);
 /* Explicit production pin-owner enrollment drives real cross-backend auth. */
 assert(companion_pin_store_open(&store,path,NULL));
 companion_pin_auth_owner_t owner={&store,approval,&approved};mac=companion_pin_auth_crypto(&owner);
 assert(ambient_identity_esp_open(&device));assert(ambient_identity_esp_public(device,device_key));esp=ambient_identity_esp_crypto(device);
 assert(ambient_auth_open(&passport,true,1,true,device_key,true,&esp));assert(ambient_auth_open(&host,false,1,true,NULL,true,&mac));
 send_auth(&host,&passport);send_auth(&passport,&host);send_auth(&host,&passport);send_auth(&passport,&host);send_auth(&host,&passport);
 assert(ambient_auth_complete(&host,1)&&ambient_auth_complete(&passport,1)&&companion_pin_store_key(&store,retained)&&!memcmp(retained,device_key,65));
 ambient_identity_esp_close(device);companion_pin_store_close(&store);remove_store(path);assert(rmdir(directory)==0);
 puts("R3 production Apple Security/SHA256/RNG and pin-store tests: PASS");
 puts("R3 actual ESP identity source + IDF mbedTLS on host: PASS (NVS/entropy APIs stubbed; not device evidence)");
}}
