#include "ambient_auth.h"
#include <string.h>
static bool fail(ambient_auth_t *a)
{
    if (a) { ambient_auth_close(a); a->state = AMBIENT_AUTH_FAILED; }
    return false;
}
void ambient_auth_close(ambient_auth_t *a) { if(a) memset(a,0,sizeof(*a)); }
static bool emit(ambient_auth_t *a, uint8_t type, const uint8_t *p, size_t n)
{
    if (a->output_length || n+6 > sizeof(a->output)) return fail(a);
    a->output[0]='P'; a->output[1]='A'; a->output[2]=1; a->output[3]=type;
    a->output[4]=(uint8_t)(n>>8); a->output[5]=(uint8_t)n;
    if(n) memcpy(a->output+6,p,n);
    a->output_length=n+6; return true;
}
static bool transcript(ambient_auth_t *a)
{
    static const uint8_t domain[]="yliu.tech/Passport-auth/v1:Mac-challenge:Passport-proof:Mac-confirm";
    uint8_t bytes[sizeof(domain)-1+64];
    memcpy(bytes,domain,sizeof(domain)-1);
    memcpy(bytes+sizeof(domain)-1,a->challenge,32);
    memcpy(bytes+sizeof(domain)-1+32,a->nonce,32);
    return a->crypto.hash(a->crypto.context,bytes,sizeof(bytes),a->transcript_hash);
}
bool ambient_auth_fingerprint(const ambient_auth_crypto_t *c, const uint8_t key[65], char out[25])
{
    static const char hex[]="0123456789ABCDEF"; uint8_t hash[32];
    if (!c || !c->hash || !key || key[0]!=4 || !out || !c->hash(c->context,key,65,hash)) return false;
    for(size_t i=0;i<12;i++){out[2*i]=hex[hash[i]>>4];out[2*i+1]=hex[hash[i]&15];}
    out[24]=0;return true;
}
static bool challenge(ambient_auth_t *a)
{
    if(!a->crypto.random(a->crypto.context,a->challenge,32)) return fail(a);
    a->state=AMBIENT_AUTH_WAIT_RESPONSE;
    return emit(a,3,a->challenge,32);
}
bool ambient_auth_open(ambient_auth_t *a,bool passport,uint64_t incarnation,bool authorized,
    const uint8_t *key,bool enroll,const ambient_auth_crypto_t *c)
{
    if(!a)return false;
    ambient_auth_close(a);
    if(!incarnation || !authorized || !c || !c->hash || !c->random ||
       (passport ? (!key || !c->sign) : !c->verify) ||
       (!passport && !key && (!enroll || !c->approve_and_pin)) || (key && key[0]!=4)) return fail(a);
    a->passport=passport;a->incarnation=incarnation;a->crypto=*c;
    a->enrollment_allowed=enroll;
    if(key){memcpy(a->public_key,key,65);a->pinned=true;}
    if(passport){a->state=AMBIENT_AUTH_WAIT_CHALLENGE;return true;}
    if(key)return challenge(a);
    a->state=AMBIENT_AUTH_WAIT_KEY;return emit(a,1,NULL,0);
}
static bool record(ambient_auth_t *a,uint8_t type,const uint8_t *p,size_t n)
{
    if(a->passport && a->state==AMBIENT_AUTH_WAIT_CHALLENGE){
        if(type==1 && n==0 && a->enrollment_allowed){
            /* One enrollment request only; next record must be the challenge. */
            a->enrollment_allowed=false;return emit(a,2,a->public_key,65);
        }
        if(type!=3 || n!=32)return fail(a);
        memcpy(a->challenge,p,32);
        if(!a->crypto.random(a->crypto.context,a->nonce,32) || !transcript(a)) return fail(a);
        uint8_t response[96];memcpy(response,a->nonce,32);
        if(!a->crypto.sign(a->crypto.context,a->transcript_hash,response+32))return fail(a);
        a->state=AMBIENT_AUTH_WAIT_CONFIRM;return emit(a,4,response,96);
    }
    if(!a->passport && a->state==AMBIENT_AUTH_WAIT_KEY){
        char fingerprint[25];
        if(type!=2 || n!=65 || !ambient_auth_fingerprint(&a->crypto,p,fingerprint) ||
           !a->crypto.approve_and_pin(a->crypto.context,p,fingerprint))return fail(a);
        memcpy(a->public_key,p,65);a->pinned=true;return challenge(a);
    }
    if(!a->passport && a->state==AMBIENT_AUTH_WAIT_RESPONSE){
        if(type!=4 || n!=96)return fail(a);
        memcpy(a->nonce,p,32);
        if(!transcript(a) || !a->crypto.verify(a->crypto.context,a->public_key,a->transcript_hash,p+32))return fail(a);
        a->state=AMBIENT_AUTH_WAIT_CONFIRM;return emit(a,5,a->transcript_hash,32);
    }
    if(a->passport && a->state==AMBIENT_AUTH_WAIT_CONFIRM){
        if(type!=5 || n!=32)return fail(a);
        uint8_t difference=0;for(size_t i=0;i<32;i++)difference|=p[i]^a->transcript_hash[i];
        if(difference)return fail(a);
        a->state=AMBIENT_AUTH_COMPLETE;return true;
    }
    return fail(a);
}
bool ambient_auth_feed(ambient_auth_t *a,uint64_t incarnation,bool authorized,const uint8_t *bytes,size_t n)
{
    if(!a || !bytes || !n || n>129 || !authorized || !incarnation ||
        incarnation!=a->incarnation || a->state==AMBIENT_AUTH_CLOSED ||
        a->state==AMBIENT_AUTH_FAILED || a->state==AMBIENT_AUTH_COMPLETE || a->output_length) return fail(a);
    for(size_t i=0;i<n;i++){
        if(a->input_length>=sizeof(a->input))return fail(a);
        a->input[a->input_length++]=bytes[i];
        if((a->input_length==1 && a->input[0]!='P') ||
           (a->input_length==2 && a->input[1]!='A') ||
           (a->input_length==3 && a->input[2]!=1))return fail(a);
        if(a->input_length>=6){
            size_t payload=((size_t)a->input[4]<<8)|a->input[5];
            if(payload+6>sizeof(a->input))return fail(a);
            if(a->input_length==payload+6){
                if(!record(a,a->input[3],a->input+6,payload))return false;
                a->input_length=0;
                /* Coalesced data after an auth output/state transition rejected. */
                if(i+1<n)return fail(a);
            }
        }
    }
    return true;
}
const uint8_t *ambient_auth_output(const ambient_auth_t *a,size_t *n)
{ if(n)*n=a?a->output_length:0;return a && a->output_length?a->output:NULL; }
bool ambient_auth_output_committed(ambient_auth_t *a)
{
    if(!a || !a->output_length)return false;
    uint8_t type=a->output[3];memset(a->output,0,sizeof(a->output));a->output_length=0;
    if(!a->passport && type==5 && a->state==AMBIENT_AUTH_WAIT_CONFIRM)a->state=AMBIENT_AUTH_COMPLETE;
    return true;
}
bool ambient_auth_complete(const ambient_auth_t *a,uint64_t incarnation)
{ return a && incarnation && a->incarnation==incarnation && a->state==AMBIENT_AUTH_COMPLETE; }
