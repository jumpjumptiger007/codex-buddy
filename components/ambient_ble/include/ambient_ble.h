#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMBIENT_BLE_MAX_TX_FRAME_BYTES 129U
#define AMBIENT_BLE_RX_QUEUE_BYTES 516U
#define AMBIENT_BLE_TX_QUEUE_DEPTH 1U
#define AMBIENT_BLE_DEFAULT_ATT_MTU 23U

typedef struct ambient_ble ambient_ble_t;

/* Link facts are observations only. They never authorize product data. */
typedef struct {
    bool connected;
    bool tx_notify_subscribed;
    bool encrypted;
    bool authenticated;
    bool bonded;
    bool secure_connections;
    bool peer_identity_valid;
    bool peer_identity_accepted;
    uint8_t peer_identity_type;
    uint8_t peer_identity[6];
    uint16_t connection_handle;
    uint16_t att_mtu;
    uint64_t link_incarnation;
} ambient_ble_link_facts_t;

typedef void (*ambient_ble_link_changed_fn)(
    void *context, const ambient_ble_link_facts_t *facts);
typedef void (*ambient_ble_rx_available_fn)(void *context);

/* Synchronous, nonblocking check for a prior explicit local pairing approval.
 * This callback must not present a prompt or perform storage/network work. */
typedef bool (*ambient_ble_pairing_approval_fn)(
    void *context,
    const uint8_t peer_identity[6],
    uint8_t peer_identity_type,
    uint32_t passkey);

typedef struct {
    const char *advertising_name;
    ambient_ble_link_changed_fn link_changed;
    ambient_ble_rx_available_fn rx_available;
    ambient_ble_pairing_approval_fn pairing_approval;
    void *context;
} ambient_ble_config_t;

/* Starts one NimBLE peripheral instance. New pairing is denied when no local
 * pairing_approval delegate is supplied. On partial cleanup failure out_ble
 * returns the retained owner; retry stop before another start. Stop success
 * consumes the handle (caller must clear it). */
esp_err_t ambient_ble_start(const ambient_ble_config_t *config,
                           ambient_ble_t **out_ble);
esp_err_t ambient_ble_stop(ambient_ble_t *ble);

/* Byte-only boundary. The caller validates/authenticates R1 before calling
 * send or allowing read bytes to reach product state. */
esp_err_t ambient_ble_send(ambient_ble_t *ble,
                           const uint8_t *frame,
                           size_t frame_length);
esp_err_t ambient_ble_read(ambient_ble_t *ble,
                           uint8_t *out,
                           size_t capacity,
                           size_t *out_length);
esp_err_t ambient_ble_get_link_facts(ambient_ble_t *ble,
                                     ambient_ble_link_facts_t *out_facts);



/* Read-only lifetime metrics; ESP-IDF stack high-water units are bytes.
 * Caller serializes against stop. No private NimBLE structures are exposed. */
typedef struct {
    size_t rx_bytes, rx_high_water_bytes;
    unsigned tx_high_water_slots;
    uint32_t tx_stack_free_min_bytes, host_stack_free_min_bytes;
    bool task_stack_available;
    /* NimBLE mbuf occupancy has no stable supported accessor here. */
    bool mbuf_available;
    /* Current lifecycle: at most 3 attempts, spaced by at least 100ms. */
    unsigned termination_attempts;
    bool termination_fault;
} ambient_ble_metrics_t;
esp_err_t ambient_ble_get_metrics(ambient_ble_t *, ambient_ble_metrics_t *);

/* Explicit local-only bond removal. This never erases NVS or product settings. */
esp_err_t ambient_ble_unpair_local(ambient_ble_t *ble);

#ifdef __cplusplus
}
#endif
