#pragma once
#include "ambient_ble.h"
#include "ambient_session.h"
static inline ambient_session_link_t r3_acceptance_link(const ambient_ble_link_facts_t *f)
{
    return (ambient_session_link_t){.connected=f->connected,.tx_notify_subscribed=f->tx_notify_subscribed,
        .encrypted=f->encrypted,.mitm_authenticated=f->authenticated,.bonded=f->bonded,
        .secure_connections=f->secure_connections,.peer_identity_valid=f->peer_identity_valid,
        .known_peer_accepted=f->peer_identity_accepted,.link_incarnation=f->link_incarnation};
}
static inline bool r3_acceptance_ble_authorized(const ambient_session_link_t *l)
{
    return l->connected && l->tx_notify_subscribed && l->encrypted && l->mitm_authenticated &&
        l->bonded && l->secure_connections && l->peer_identity_valid && l->known_peer_accepted && l->link_incarnation;
}
