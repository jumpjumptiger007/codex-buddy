#pragma once
#include "ambient_auth.h"
#include <string.h>
/* Deliberately noncryptographic operations, solely portable state-machine tests. */
typedef struct {uint8_t key;unsigned nonce;bool fail_rng,fail_sign,deny_pin,fail_pin;} auth_fake_t;
static bool fake_random(void*ctx,uint8_t*p,size_t n){auth_fake_t*f=ctx;if(f->fail_rng)return false;memset(p,++f->nonce,n);return true;}
static bool fake_hash(void*ctx,const uint8_t*p,size_t n,uint8_t out[32]){(void)ctx;memset(out,0,32);for(size_t i=0;i<n;i++)out[i%32]^=p[i];return true;}
static bool fake_sign(void*ctx,const uint8_t hash[32],uint8_t sig[64]){auth_fake_t*f=ctx;if(f->fail_sign)return false;memcpy(sig,hash,32);memset(sig+32,f->key,32);return true;}
static bool fake_verify(void*ctx,const uint8_t key[65],const uint8_t hash[32],const uint8_t sig[64]){(void)ctx;if(memcmp(sig,hash,32))return false;for(size_t i=32;i<64;i++)if(sig[i]!=key[1])return false;return true;}
static bool fake_pin(void*ctx,const uint8_t key[65],const char*fp){auth_fake_t*f=ctx;(void)fp;if(f->deny_pin||f->fail_pin)return false;f->key=key[1];return true;}
static ambient_auth_crypto_t fake_crypto(auth_fake_t*f){return(ambient_auth_crypto_t){fake_random,fake_hash,fake_sign,fake_verify,fake_pin,f};}
static void fake_key(uint8_t key[65],uint8_t value){memset(key,value,65);key[0]=4;}
