#pragma once
#include "companion_secure_bridge.h"
#include "ambient_auth.h"

#define COMPANION_BLE_SERVICE_UUID "0913bf62-0732-53ce-b7c7-c90db3db3881"
#define COMPANION_BLE_RX_UUID "3b9da8eb-4a29-53bb-a219-d4ad4a6e1c11"
#define COMPANION_BLE_TX_UUID "90127ebc-fe65-5b74-8e8a-41f58a8c27af"
#define COMPANION_BLE_QUEUE_BYTES 516U

typedef struct {
    /* Serialized caller; write borrows bytes only for this call. No scan/connect. */
    ambient_transport_result_t (*write)(void *, const uint8_t *, size_t);
    void *context;
    size_t write_limit;
} companion_ble_central_platform_t;
typedef struct {
    uint8_t bytes[COMPANION_BLE_QUEUE_BYTES];
    size_t head, length;
} companion_ble_byte_queue_t;
typedef struct {
    bool alive, service_found, rx_found, tx_found;
    /* Byte-only read-only counters; candidate valid only if isolated at send. */
    uint64_t platform_writes;
    size_t measured_remaining, measured_bytes, measured_chunks;
    size_t last_frame_bytes, last_frame_chunks;
    uint64_t next_incarnation, peer_token;
    companion_secure_link_t link;
    companion_ble_byte_queue_t rx, tx;
    companion_ble_central_platform_t platform;
    ambient_transport_t transport;
    companion_secure_bridge_t bridge;
    ambient_auth_t auth;
    companion_runtime_t *runtime;
    const ambient_generation_backend_t *generation_backend;
} companion_ble_central_t;

/* Zero initialize. Re-entry preserves incarnation history; exhaustion fails closed.
 * peer_token is a Mac-local opaque peripheral identity, not proof of a BLE bond. */
bool companion_ble_central_begin(companion_ble_central_t *, uint64_t peer_token,
    companion_runtime_t *, const ambient_generation_backend_t *,
    const companion_ble_central_platform_t *);
bool companion_ble_central_service(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token, const char *uuid);
bool companion_ble_central_characteristic(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token, const char *service_uuid, const char *uuid, bool write, bool notify);
bool companion_ble_central_subscription(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token, bool subscribed);
/* Call only after discovery/subscription; missing pin requires explicit enrollment.
 * Crypto owner and durable pin store must outlive this link. No BLE guesses. */
bool companion_ble_central_authenticate(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token, const uint8_t *pinned_key, bool explicit_enrollment,
    const ambient_auth_crypto_t *);
ambient_transport_result_t companion_ble_central_rx(companion_ble_central_t *,
    uint64_t incarnation, uint64_t peer_token, const uint8_t *, size_t);
/* Finite work: one platform write + one <=129-byte RX dispatch per call. */
ambient_transport_result_t companion_ble_central_tick(companion_ble_central_t *,
    uint64_t now_ms, int64_t now_unix);
void companion_ble_central_disconnect(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token);
void companion_ble_central_teardown(companion_ble_central_t *);

/* Serialized event pump: at most 64 ticks, each <=129 RX bytes / one write.
 * Stale owner never dispatches. WOULD_BLOCK preserves work for write-ready.
 * Production wrapper uses 20-byte writes; the budget covers both bounded queues
 * plus auth/HELLO materialization. Smaller platform chunks may need another event. */
#define COMPANION_BLE_PUMP_TICKS 64U
ambient_transport_result_t companion_ble_central_pump(companion_ble_central_t *,
    uint64_t incarnation, uint64_t peer_token, uint64_t now_ms, int64_t now_unix);

/* Notifications cannot be retried after callback return: current-link admission
 * failure closes all transport/auth/bridge state. Stale callbacks never close
 * a newer link. Generic rx() retains its all-or-none admission contract. */
ambient_transport_result_t companion_ble_central_notification(companion_ble_central_t *,
    uint64_t incarnation, uint64_t peer_token, const uint8_t *, size_t);

/* begin(NULL platform) creates an unbound owner; no I/O/auth until bind.
 * Bind exactly once, on the same serialized queue, before auth. No dummy writer. */
bool companion_ble_central_bind(companion_ble_central_t *, uint64_t incarnation,
    uint64_t peer_token, const companion_ble_central_platform_t *);
