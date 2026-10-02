#include "ambient_identity.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "bootloader_random.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include <stdatomic.h>
#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include <stdlib.h>
#include <string.h>
#if !defined(MBEDTLS_ECDSA_C) || !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) || !defined(MBEDTLS_SHA256_C)
#error "R3 requires supported P256 ECDSA and SHA256"
#endif
struct ambient_identity_esp {
    nvs_handle_t nvs;
    bool entropy_enabled;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_ecdsa_context key;
    ambient_identity_t identity;
};
static atomic_bool owner;
static int entropy(void *context,unsigned char *out,size_t n)
{ambient_identity_esp_t*s=context;if(!s->entropy_enabled)return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;esp_fill_random(out,n);return 0;}
static int rng(void *context,unsigned char *out,size_t n)
{ambient_identity_esp_t*s=context;return mbedtls_ctr_drbg_random(&s->drbg,out,n);}
static bool random_bytes(void *context,uint8_t *out,size_t n)
{return out && n<=129 && rng(context,out,n)==0;}
static bool hash_bytes(void *context,const uint8_t *p,size_t n,uint8_t hash[32])
{(void)context;return mbedtls_sha256(p,n,hash,0)==0;}
static bool public_key(void *context,uint8_t out[65])
{ambient_identity_esp_t *s=context;size_t n=0;return mbedtls_ecp_point_write_binary(&s->key.MBEDTLS_PRIVATE(grp),&s->key.MBEDTLS_PRIVATE(Q),MBEDTLS_ECP_PF_UNCOMPRESSED,&n,out,65)==0 && n==65;}
static int load(void *context)
{
    ambient_identity_esp_t *s=context;uint8_t record[134],marker=0;size_t n=sizeof(record);
    esp_err_t mark=nvs_get_u8(s->nvs,"created",&marker);
    esp_err_t read=nvs_get_blob(s->nvs,"key",record,&n);
    if(mark==ESP_ERR_NVS_NOT_FOUND && read==ESP_ERR_NVS_NOT_FOUND){mbedtls_platform_zeroize(record,sizeof(record));return 0;}
    if(mark!=ESP_OK || marker!=1 || read!=ESP_OK || n!=sizeof(record) || memcmp(record,"PID1",4)){mbedtls_platform_zeroize(record,sizeof(record));return -1;}
    uint8_t hash[32],derived[65];bool ok=hash_bytes(NULL,record,102,hash) && memcmp(hash,record+102,32)==0;
    ok=ok && mbedtls_ecp_group_load(&s->key.MBEDTLS_PRIVATE(grp),MBEDTLS_ECP_DP_SECP256R1)==0 &&
        mbedtls_mpi_read_binary(&s->key.MBEDTLS_PRIVATE(d),record+4,32)==0 &&
        mbedtls_ecp_check_privkey(&s->key.MBEDTLS_PRIVATE(grp),&s->key.MBEDTLS_PRIVATE(d))==0 &&
        mbedtls_ecp_mul(&s->key.MBEDTLS_PRIVATE(grp),&s->key.MBEDTLS_PRIVATE(Q),&s->key.MBEDTLS_PRIVATE(d),&s->key.MBEDTLS_PRIVATE(grp).G,rng,s)==0 &&
        public_key(s,derived) && memcmp(derived,record+36,65)==0 && record[101]==1;
    mbedtls_platform_zeroize(record,sizeof(record));return ok?1:-1;
}
static bool mark_creation(void *context)
{ambient_identity_esp_t*s=context;return nvs_set_u8(s->nvs,"created",1)==ESP_OK && nvs_commit(s->nvs)==ESP_OK;}
static bool create(void *context)
{
    ambient_identity_esp_t*s=context;uint8_t record[134]={0};memcpy(record,"PID1",4);record[101]=1;
    bool ok=mbedtls_ecdsa_genkey(&s->key,MBEDTLS_ECP_DP_SECP256R1,rng,s)==0 &&
        mbedtls_mpi_write_binary(&s->key.MBEDTLS_PRIVATE(d),record+4,32)==0 &&
        public_key(s,record+36) && hash_bytes(NULL,record,102,record+102) &&
        nvs_set_blob(s->nvs,"key",record,sizeof(record))==ESP_OK && nvs_commit(s->nvs)==ESP_OK;
    mbedtls_platform_zeroize(record,sizeof(record));return ok;
}
static bool sign_digest(void *context,const uint8_t digest[32],uint8_t signature[64])
{
    ambient_identity_esp_t*s=context;mbedtls_mpi r,t;mbedtls_mpi_init(&r);mbedtls_mpi_init(&t);
    bool ok=mbedtls_ecdsa_sign(&s->key.MBEDTLS_PRIVATE(grp),&r,&t,&s->key.MBEDTLS_PRIVATE(d),digest,32,rng,s)==0 &&
        mbedtls_mpi_write_binary(&r,signature,32)==0 && mbedtls_mpi_write_binary(&t,signature+32,32)==0;
    mbedtls_mpi_free(&r);mbedtls_mpi_free(&t);return ok;
}
static bool auth_sign(void *context,const uint8_t digest[32],uint8_t signature[64])
{ambient_identity_esp_t*s=context;return ambient_identity_sign(&s->identity,digest,signature);}
bool ambient_identity_esp_open(ambient_identity_esp_t **out)
{
    if(!out)return false;
    *out=NULL;
    if(nvs_flash_init()!=ESP_OK)return false;
    ambient_identity_esp_t*s=calloc(1,sizeof(*s));if(!s)return false;
    bool expected=false;
    if(!atomic_compare_exchange_strong(&owner,&expected,true)){free(s);return false;}
    mbedtls_ecdsa_init(&s->key);mbedtls_ctr_drbg_init(&s->drbg);
    /* Factory must run before BSP/RF/ADC startup. Entropy enabled only while
     * seeding CTR_DRBG; later automatic reseed fails closed until re-entry. */
    bootloader_random_enable();s->entropy_enabled=true;
    static const unsigned char domain[]="Passport-identity-P256-v1";
    int seed=mbedtls_ctr_drbg_seed(&s->drbg,entropy,s,domain,sizeof(domain)-1);
    s->entropy_enabled=false;bootloader_random_disable();
    if(seed){mbedtls_ctr_drbg_free(&s->drbg);mbedtls_ecdsa_free(&s->key);free(s);atomic_store(&owner,false);return false;}
    if(nvs_open("passport_id",NVS_READWRITE,&s->nvs)!=ESP_OK){mbedtls_ctr_drbg_free(&s->drbg);mbedtls_ecdsa_free(&s->key);free(s);atomic_store(&owner,false);return false;}
    ambient_identity_backend_t backend={load,mark_creation,create,public_key,sign_digest,s};
    if(!ambient_identity_open(&s->identity,&backend)){ambient_identity_esp_close(s);return false;}
    *out=s;return true;
}
void ambient_identity_esp_close(ambient_identity_esp_t*s)
{if(s){ambient_identity_close(&s->identity);mbedtls_ecdsa_free(&s->key);mbedtls_ctr_drbg_free(&s->drbg);nvs_close(s->nvs);mbedtls_platform_zeroize(s,sizeof(*s));free(s);atomic_store(&owner,false);}}
bool ambient_identity_esp_public(ambient_identity_esp_t*s,uint8_t out[65])
{return s && ambient_identity_public(&s->identity,out);}
ambient_auth_crypto_t ambient_identity_esp_crypto(ambient_identity_esp_t*s)
{return (ambient_auth_crypto_t){.random=random_bytes,.hash=hash_bytes,.sign=auth_sign,.context=s};}
