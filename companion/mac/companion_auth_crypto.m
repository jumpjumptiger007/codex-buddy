#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <CommonCrypto/CommonDigest.h>
#include "companion_auth_crypto.h"
static SecKeyRef public_key(const uint8_t key[65])
{
    if(!key || key[0]!=4)return NULL;
    NSDictionary *attributes=@{(__bridge id)kSecAttrKeyType:(__bridge id)kSecAttrKeyTypeECSECPrimeRandom,
        (__bridge id)kSecAttrKeyClass:(__bridge id)kSecAttrKeyClassPublic,(__bridge id)kSecAttrKeySizeInBits:@256};
    return SecKeyCreateWithData((__bridge CFDataRef)[NSData dataWithBytes:key length:65],
        (__bridge CFDictionaryRef)attributes,NULL);
}
bool companion_auth_public_key_valid(const uint8_t key[65])
{SecKeyRef k=public_key(key);if(!k)return false;CFRelease(k);return true;}
static bool random_bytes(void*context,uint8_t*out,size_t n)
{(void)context;return out && n<=129 && SecRandomCopyBytes(kSecRandomDefault,n,out)==errSecSuccess;}
static bool hash_bytes(void*context,const uint8_t*p,size_t n,uint8_t out[32])
{(void)context;return p && out && n<=1024 && CC_SHA256(p,(CC_LONG)n,out)!=NULL;}
/* Canonical raw r||s to X9.62 DER, using only bounded integer encoding. */
static size_t integer(const uint8_t *raw,uint8_t *out)
{
    size_t skip=0;while(skip<31 && raw[skip]==0)skip++;
    size_t n=32-skip;bool pad=(raw[skip]&128)!=0;
    out[0]=2;out[1]=(uint8_t)(n+(pad?1:0));if(pad)out[2]=0;
    memcpy(out+2+(pad?1:0),raw+skip,n);return 2+n+(pad?1:0);
}
static bool verify(void*context,const uint8_t key[65],const uint8_t digest[32],const uint8_t sig[64])
{
    (void)context;SecKeyRef k=public_key(key);if(!k || !digest || !sig){if(k)CFRelease(k);return false;}
    uint8_t der[72]={0x30};size_t r=integer(sig,der+2),s=integer(sig+32,der+2+r);der[1]=(uint8_t)(r+s);
    bool ok=SecKeyVerifySignature(k,kSecKeyAlgorithmECDSASignatureDigestX962SHA256,
        (__bridge CFDataRef)[NSData dataWithBytes:digest length:32],
        (__bridge CFDataRef)[NSData dataWithBytes:der length:r+s+2],NULL);
    CFRelease(k);return ok;
}
ambient_auth_crypto_t companion_auth_crypto(void)
{return(ambient_auth_crypto_t){.random=random_bytes,.hash=hash_bytes,.verify=verify};}
