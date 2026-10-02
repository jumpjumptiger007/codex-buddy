#include "ambient_identity.h"
#include <string.h>
bool ambient_identity_open(ambient_identity_t *identity,const ambient_identity_backend_t *backend)
{
    if(!identity)return false;
    ambient_identity_close(identity);
    if(!backend || !backend->load || !backend->mark_creation || !backend->create_and_commit ||
       !backend->public_key || !backend->sign)return false;
    int status=backend->load(backend->context);
    if(status<0 || (status==0 && (!backend->mark_creation(backend->context) ||
       !backend->create_and_commit(backend->context))))return false;
    if(!backend->public_key(backend->context,identity->public_key) || identity->public_key[0]!=4){
        ambient_identity_close(identity);return false;
    }
    identity->backend=*backend;identity->ready=true;return true;
}
void ambient_identity_close(ambient_identity_t *identity){if(identity)memset(identity,0,sizeof(*identity));}
bool ambient_identity_public(const ambient_identity_t *identity,uint8_t out[65])
{if(!identity || !identity->ready || !out)return false;memcpy(out,identity->public_key,65);return true;}
bool ambient_identity_sign(void *context,const uint8_t digest[32],uint8_t signature[64])
{ambient_identity_t *identity=context;return identity && identity->ready && digest && signature && identity->backend.sign(identity->backend.context,digest,signature);}
