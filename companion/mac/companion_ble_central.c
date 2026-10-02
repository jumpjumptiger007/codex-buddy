#include "companion_ble_central.h"
#include <string.h>

static bool current(const companion_ble_central_t *c, uint64_t incarnation, uint64_t peer)
{ return c && c->alive && c->link.connected && c->link.link_incarnation == incarnation && c->peer_token == peer; }
static bool transport_ready(const companion_ble_central_t *c)
{ return c && c->alive && c->link.connected && c->link.rx_notify_subscribed && c->service_found && c->rx_found && c->tx_found && c->platform.write && c->platform.write_limit; }
static bool ready(const companion_ble_central_t *c)
{ return c && c->alive && c->service_found && c->rx_found && c->tx_found && companion_secure_link_authorized(&c->link); }
static bool enqueue(companion_ble_byte_queue_t *q, const uint8_t *bytes, size_t n)
{
    if (!bytes || !n || n > sizeof(q->bytes) - q->length) return false;
    for (size_t i = 0; i < n; ++i) q->bytes[(q->head + q->length + i) % sizeof(q->bytes)] = bytes[i];
    q->length += n; return true;
}
static size_t peek(const companion_ble_byte_queue_t *q, uint8_t *out, size_t capacity)
{
    size_t n = q->length < capacity ? q->length : capacity;
    for (size_t i = 0; i < n; ++i) out[i] = q->bytes[(q->head + i) % sizeof(q->bytes)];
    return n;
}
static void consume(companion_ble_byte_queue_t *q, size_t n)
{
    for (size_t i = 0; i < n; ++i) q->bytes[(q->head + i) % sizeof(q->bytes)] = 0;
    q->head = (q->head + n) % sizeof(q->bytes); q->length -= n;
}
static bool connected(void *ctx) { return ready(ctx); }
static ambient_transport_result_t send_bytes(void *ctx, const uint8_t *bytes, size_t n)
{
    companion_ble_central_t *c = ctx;
    if (!ready(c)) return AMBIENT_TRANSPORT_DISCONNECTED;
    if (!bytes || !n) return AMBIENT_TRANSPORT_INVALID;
    if (n > AMBIENT_WIRE_MAX_FRAME_BYTES) return AMBIENT_TRANSPORT_TOO_LARGE;
    bool isolated=c->tx.length==0;
    if(!enqueue(&c->tx,bytes,n))return AMBIENT_TRANSPORT_WOULD_BLOCK;
    if(isolated){c->measured_remaining=n;c->measured_bytes=n;c->measured_chunks=0;}
    else c->measured_remaining=0; /* Coalesced frames cannot prove isolation. */
    return AMBIENT_TRANSPORT_OK;
}
static ambient_transport_result_t receive_bytes(void *ctx, uint8_t *out, size_t cap, size_t *length)
{
    companion_ble_central_t *c = ctx;
    if (length) *length = 0;
    if (!ready(c)) return AMBIENT_TRANSPORT_DISCONNECTED;
    if (!out || !cap || !length) return AMBIENT_TRANSPORT_INVALID;
    *length = peek(&c->rx, out, cap); consume(&c->rx, *length);
    return *length ? AMBIENT_TRANSPORT_OK : AMBIENT_TRANSPORT_WOULD_BLOCK;
}
static const ambient_transport_ops_t transport_ops = {connected, send_bytes, receive_bytes};
void companion_ble_central_teardown(companion_ble_central_t *c)
{
    if (!c) return;
    uint64_t history = c->next_incarnation;
    companion_secure_bridge_close(&c->bridge);
    memset(c, 0, sizeof(*c)); c->next_incarnation = history;
}
bool companion_ble_central_begin(companion_ble_central_t *c, uint64_t peer,
    companion_runtime_t *runtime, const ambient_generation_backend_t *backend,
    const companion_ble_central_platform_t *platform)
{
    if (!c || !peer || !runtime || !runtime->running || !backend || (platform && (!platform->write || !platform->write_limit)) || c->next_incarnation == UINT64_MAX) return false;
    companion_ble_central_teardown(c);
    c->next_incarnation++; c->peer_token = peer; c->alive = true;
    c->link.connected = true; c->link.link_incarnation = c->next_incarnation;
    c->runtime = runtime; c->generation_backend = backend; if (platform) c->platform = *platform;
    c->transport = (ambient_transport_t){&transport_ops, c, AMBIENT_WIRE_MAX_FRAME_BYTES};
    return true;
}
bool companion_ble_central_service(companion_ble_central_t *c, uint64_t id, uint64_t peer, const char *uuid)
{
    if (!current(c, id, peer) || !uuid || strcmp(uuid, COMPANION_BLE_SERVICE_UUID)) return false;
    c->service_found = true; c->link.service_discovered = true; return true;
}
bool companion_ble_central_characteristic(companion_ble_central_t *c, uint64_t id, uint64_t peer,
    const char *service, const char *uuid, bool write, bool notify)
{
    if (!current(c, id, peer) || !c->service_found || !service || !uuid || strcmp(service, COMPANION_BLE_SERVICE_UUID)) return false;
    if (!strcmp(uuid, COMPANION_BLE_RX_UUID) && write) { c->rx_found = true; c->link.characteristics_discovered = c->tx_found; return true; }
    if (!strcmp(uuid, COMPANION_BLE_TX_UUID) && notify) { c->tx_found = true; c->link.characteristics_discovered = c->rx_found; return true; }
    return false;
}
static bool reconcile(companion_ble_central_t *c)
{
    if (!ready(c)) {
        companion_secure_bridge_close(&c->bridge);
        memset(&c->rx, 0, sizeof(c->rx)); memset(&c->tx, 0, sizeof(c->tx));
        return false;
    }
    if (c->bridge.active) return true;
    return companion_secure_bridge_open(&c->bridge, c->runtime, &c->link, &c->transport, c->generation_backend);
}
bool companion_ble_central_subscription(companion_ble_central_t *c, uint64_t id, uint64_t peer, bool subscribed)
{
    if (!current(c, id, peer) || !c->rx_found || !c->tx_found) return false;
    c->link.rx_notify_subscribed = subscribed;
    if (!subscribed) { ambient_auth_close(&c->auth); c->link.application_authenticated=false; c->link.pinned_identity=false; c->link.auth_incarnation=0; }
    (void)reconcile(c); return true;
}
bool companion_ble_central_authenticate(companion_ble_central_t *c, uint64_t id, uint64_t peer,
    const uint8_t *key, bool enroll, const ambient_auth_crypto_t *crypto)
{
    if (!current(c,id,peer) || !transport_ready(c) || c->auth.state!=AMBIENT_AUTH_CLOSED) return false;
    return ambient_auth_open(&c->auth,false,id,true,key,enroll,crypto);
}
ambient_transport_result_t companion_ble_central_rx(companion_ble_central_t *c,
    uint64_t id, uint64_t peer, const uint8_t *bytes, size_t n)
{
    if (!current(c, id, peer) || !transport_ready(c)) return AMBIENT_TRANSPORT_DISCONNECTED;
    if (!bytes || !n) return AMBIENT_TRANSPORT_INVALID;
    if (n > AMBIENT_WIRE_MAX_FRAME_BYTES) return AMBIENT_TRANSPORT_TOO_LARGE;
    return enqueue(&c->rx, bytes, n) ? AMBIENT_TRANSPORT_OK : AMBIENT_TRANSPORT_WOULD_BLOCK;
}
ambient_transport_result_t companion_ble_central_tick(companion_ble_central_t *c, uint64_t now, int64_t unix_time)
{
    if (!transport_ready(c)) return AMBIENT_TRANSPORT_DISCONNECTED;
    uint8_t bytes[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t limit = c->platform.write_limit < sizeof(bytes) ? c->platform.write_limit : sizeof(bytes);
    size_t n = peek(&c->tx, bytes, limit);
    if (n) {
        ambient_transport_result_t result = c->platform.write(c->platform.context, bytes, n);
        if (result == AMBIENT_TRANSPORT_OK) {
            consume(&c->tx,n);if(c->platform_writes!=UINT64_MAX)c->platform_writes++;
            if(c->measured_remaining){
                c->measured_chunks++;
                if(n>=c->measured_remaining){
                    c->last_frame_bytes=c->measured_bytes;c->last_frame_chunks=c->measured_chunks;c->measured_remaining=0;
                }else c->measured_remaining-=n;
            }
        }
        else if (result == AMBIENT_TRANSPORT_WOULD_BLOCK) return result;
        else { companion_ble_central_teardown(c); return result; }
    }
    n = peek(&c->rx, bytes, sizeof(bytes));
    if (n) {
        consume(&c->rx, n);
        if (ambient_auth_complete(&c->auth,c->link.link_incarnation)) {
            if (bytes[0]=='P') { companion_ble_central_teardown(c); return AMBIENT_TRANSPORT_INVALID; }
            (void)companion_secure_bridge_feed(&c->bridge, &c->link, bytes, n, now, unix_time);
        } else {
            for(size_t i=0;i<n;i++) if(!ambient_auth_feed(&c->auth,c->link.link_incarnation,true,bytes+i,1)) {
                companion_ble_central_teardown(c); return AMBIENT_TRANSPORT_INVALID;
            }
        }
    }
    size_t auth_length=0;const uint8_t *auth_bytes=ambient_auth_output(&c->auth,&auth_length);
    if(auth_bytes){
        if(!enqueue(&c->tx,auth_bytes,auth_length))return AMBIENT_TRANSPORT_WOULD_BLOCK;
        (void)ambient_auth_output_committed(&c->auth);
    }
    if(ambient_auth_complete(&c->auth,c->link.link_incarnation) && !c->link.application_authenticated){
        c->link.pinned_identity=c->auth.pinned;c->link.application_authenticated=true;
        c->link.auth_incarnation=c->auth.incarnation;
        if(!reconcile(c)){companion_ble_central_teardown(c);return AMBIENT_TRANSPORT_INVALID;}
    }
    return AMBIENT_TRANSPORT_OK;
}
void companion_ble_central_disconnect(companion_ble_central_t *c, uint64_t id, uint64_t peer)
{ if (current(c, id, peer)) companion_ble_central_teardown(c); }

ambient_transport_result_t companion_ble_central_pump(companion_ble_central_t *c,
    uint64_t id, uint64_t peer, uint64_t now, int64_t unix_time)
{
    if (!current(c,id,peer) || !transport_ready(c)) return AMBIENT_TRANSPORT_DISCONNECTED;
    for (unsigned i=0; i<COMPANION_BLE_PUMP_TICKS; ++i) {
        size_t length=0;
        if (!c->rx.length && !c->tx.length && !ambient_auth_output(&c->auth,&length))
            return AMBIENT_TRANSPORT_OK;
        ambient_transport_result_t result=companion_ble_central_tick(c,now,unix_time);
        if (result!=AMBIENT_TRANSPORT_OK) return result;
        if (!current(c,id,peer)) return AMBIENT_TRANSPORT_DISCONNECTED;
    }
    return AMBIENT_TRANSPORT_WOULD_BLOCK;
}

ambient_transport_result_t companion_ble_central_notification(companion_ble_central_t *c,
    uint64_t id, uint64_t peer, const uint8_t *bytes, size_t length)
{
    if (!current(c,id,peer)) return AMBIENT_TRANSPORT_DISCONNECTED;
    ambient_transport_result_t result=companion_ble_central_rx(c,id,peer,bytes,length);
    if (result!=AMBIENT_TRANSPORT_OK) companion_ble_central_teardown(c);
    return result;
}

bool companion_ble_central_bind(companion_ble_central_t *c, uint64_t id, uint64_t peer,
    const companion_ble_central_platform_t *platform)
{
    if (!current(c,id,peer) || !platform || !platform->write || !platform->write_limit ||
        c->auth.state!=AMBIENT_AUTH_CLOSED || c->rx.length || c->tx.length || c->platform.write) return false;
    c->platform=*platform;return true;
}
