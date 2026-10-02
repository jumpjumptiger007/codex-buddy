#include "ambient_authenticated_session.h"
static bool ble_policy(const ambient_session_link_t *link)
{
    if(!link)return false;
    ambient_session_link_t checked=*link;checked.application_authenticated=true;checked.auth_incarnation=checked.link_incarnation;
    return ambient_session_link_authorized(&checked); /* BLE facts independent of app auth. */
}
void ambient_authenticated_session_close(ambient_authenticated_session_t*s)
{if(s){ambient_auth_close(&s->auth);ambient_session_close(&s->session);}}
bool ambient_authenticated_session_open(ambient_authenticated_session_t*s,const ambient_session_link_t*link,
    const uint8_t key[65],bool enrollment,const ambient_auth_crypto_t*crypto)
{
    if(!s)return false;
    ambient_authenticated_session_close(s);
    return ble_policy(link) && ambient_auth_open(&s->auth,true,link->link_incarnation,true,key,enrollment,crypto);
}
static bool current(ambient_authenticated_session_t*s,const ambient_session_link_t*link)
{return s && ble_policy(link) && s->auth.incarnation==link->link_incarnation;}
static ambient_session_link_t authenticated(const ambient_session_link_t*link)
{ambient_session_link_t result=*link;result.application_authenticated=true;result.auth_incarnation=result.link_incarnation;return result;}
ambient_transport_result_t ambient_authenticated_session_flush(ambient_authenticated_session_t*s,
    const ambient_session_link_t*link,const ambient_transport_t*transport)
{
    if(!current(s,link)){ambient_authenticated_session_close(s);return AMBIENT_TRANSPORT_DISCONNECTED;}
    size_t n;const uint8_t*out=ambient_auth_output(&s->auth,&n);
    if(out){ambient_transport_result_t r=ambient_transport_send(transport,out,n);
        if(r==AMBIENT_TRANSPORT_OK)(void)ambient_auth_output_committed(&s->auth);
        else if(r!=AMBIENT_TRANSPORT_WOULD_BLOCK)ambient_authenticated_session_close(s);
        return r;
    }
    if(s->session.active){ambient_session_link_t facts=authenticated(link);return ambient_session_flush_ack(&s->session,&facts,transport);}
    return AMBIENT_TRANSPORT_OK;
}
bool ambient_authenticated_session_feed(ambient_authenticated_session_t*s,const ambient_session_link_t*link,
    const uint8_t*bytes,size_t n,const ambient_transport_t*transport)
{
    if(!current(s,link) || !bytes || !n || n>129){ambient_authenticated_session_close(s);return false;}
    for(size_t i=0;i<n;i++){
        if(!ambient_auth_complete(&s->auth,link->link_incarnation)){
            if(!ambient_auth_feed(&s->auth,link->link_incarnation,true,bytes+i,1)){ambient_authenticated_session_close(s);return false;}
            if(ambient_auth_complete(&s->auth,link->link_incarnation)){
                ambient_session_link_t facts=authenticated(link);
                if(!ambient_session_open(&s->session,&facts)){ambient_authenticated_session_close(s);return false;}
            }
        }else{
            if(bytes[i]=='P'){ambient_authenticated_session_close(s);return false;}
            ambient_session_link_t facts=authenticated(link);
            (void)ambient_session_feed(&s->session,&facts,bytes+i,1,transport);
        }
    }
    return true;
}
