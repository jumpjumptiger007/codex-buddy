#pragma once
#include "ambient_auth.h"
#include "ambient_session.h"
typedef struct { ambient_auth_t auth; ambient_session_t session; } ambient_authenticated_session_t;
/* Caller serializes, provides current NimBLE facts each feed/flush and closes on
 * disconnect/security/subscription loss. R1 session remains closed until confirm. */
bool ambient_authenticated_session_open(ambient_authenticated_session_t *,
    const ambient_session_link_t *,const uint8_t[65],bool enrollment_authorized,
    const ambient_auth_crypto_t *);
void ambient_authenticated_session_close(ambient_authenticated_session_t *);
bool ambient_authenticated_session_feed(ambient_authenticated_session_t *,
    const ambient_session_link_t *,const uint8_t *,size_t,const ambient_transport_t *);
ambient_transport_result_t ambient_authenticated_session_flush(ambient_authenticated_session_t *,
    const ambient_session_link_t *,const ambient_transport_t *);
