/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ambient_ble.h"
#include "ambient_ble_ring.h"

#ifdef AMBIENT_BLE_HOST_TEST
#include "ambient_ble_platform_fake.h"
#else
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/config/ble_store_config.h"
#include "nvs.h"
#endif
#include <stdatomic.h>

#define AMBIENT_BLE_NAME_MAX 31U
#define AMBIENT_BLE_ATT_WRITE_PAYLOAD_MAX (AMBIENT_BLE_DEFAULT_ATT_MTU - 3U)
#define AMBIENT_BLE_TX_TASK_STACK 4096U
#define AMBIENT_BLE_TX_TASK_PRIORITY 5U
#define AMBIENT_BLE_STOP_TIMEOUT_MS 2000U
#define AMBIENT_BLE_TERMINATION_ATTEMPTS 3U
#define AMBIENT_BLE_TERMINATION_RETRY_MS 100U

typedef struct {
    uint8_t kind;
    uint64_t link_incarnation;
    uint16_t connection_handle;
    uint16_t length;
    uint8_t peer_identity_type;
    uint8_t peer_identity[6];
    uint8_t bytes[AMBIENT_BLE_MAX_TX_FRAME_BYTES];
} ambient_ble_tx_item_t;

_Static_assert(sizeof(ambient_ble_tx_item_t) <= 160U,
               "one R3 TX queue slot must stay within 160 bytes");

enum {
    AMBIENT_BLE_TX_ITEM_FRAME = 1,
    AMBIENT_BLE_TX_ITEM_ACCEPT_PEER = 2,
};

struct ambient_ble {
    ambient_ble_config_t config;
    char advertising_name[AMBIENT_BLE_NAME_MAX + 1U];
    SemaphoreHandle_t state_mutex;
    SemaphoreHandle_t peer_store_mutex;
    SemaphoreHandle_t host_stopped;
    SemaphoreHandle_t tx_stopped;
    QueueHandle_t tx_queue;
    nvs_handle_t peer_nvs;
    TaskHandle_t host_task;
    TaskHandle_t tx_task;
    ambient_ble_link_facts_t link;
    ambient_ble_ring_t rx_ring;
    size_t rx_high_water;
    atomic_uint tx_high_water;
    uint8_t accepted_peer_identity[6];
    uint8_t accepted_peer_identity_type;
    struct ble_gatt_chr_def gatt_chars[3];
    struct ble_gatt_svc_def gatt_svcs[2];
    uint64_t next_link_incarnation;
    uint64_t pairing_link_incarnation;
    uint8_t own_addr_type;
    uint8_t pending_pair_identity[6];
    uint8_t pending_pair_identity_type;
    uint16_t tx_value_handle;
    bool initialized;
    bool accepted_peer_valid;
    atomic_bool unpairing;
    uint16_t unpair_handle;
    uint16_t stop_handle;
    uint16_t forced_handle;
    uint64_t forced_incarnation;
    bool forced_disconnected;
    bool forced_provisional;
    unsigned termination_attempts;
    TickType_t termination_last_attempt;
    bool termination_retry;
    bool termination_fault;
    /* Single fixed slot. The short critical section only publishes/copies
     * numeric lifecycle ownership; it never calls NimBLE or waits for a mutex. */
    portMUX_TYPE disconnect_mux;
    atomic_bool disconnect_pending;
    bool pending_disconnect_event;
    bool pending_disconnect_terminate;
    bool pending_disconnect_provisional;
    uint16_t pending_disconnect_handle;
    uint64_t pending_disconnect_incarnation;
    uint64_t published_incarnation;
    uint16_t published_handle;
    /* Physical admission only, never an authenticated/R1 generation. */
    bool provisional_active;
    uint16_t provisional_handle;
    uint64_t provisional_serial;
    uint16_t retired_provisional_handle;
    uint64_t retired_provisional_serial;
    bool host_stop_requested;
    bool peer_nvs_open;
    bool pending_pair_approved;
    bool host_task_started;
    bool tx_task_started;
    atomic_bool shutting_down;
};

extern ambient_ble_t *g_ambient_ble_active;

void ambient_ble_prepare_gatt(ambient_ble_t *ble);
void ambient_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                  void *arg);
void ambient_ble_start_advertising(ambient_ble_t *ble);
void ambient_ble_clear_io(ambient_ble_t *ble);
void ambient_ble_reset_link_locked(ambient_ble_t *ble);
void ambient_ble_refresh_security_locked(ambient_ble_t *ble);
void ambient_ble_notify_link_change(ambient_ble_t *ble,
                                    ambient_ble_link_facts_t before,
                                    ambient_ble_link_facts_t after);
int ambient_ble_gap_event(struct ble_gap_event *event, void *arg);
void ambient_ble_on_reset(int reason);
void ambient_ble_on_sync(void);
void ambient_ble_host_task(void *arg);
void ambient_ble_tx_task(void *arg);
bool ambient_ble_load_accepted_peer(ambient_ble_t *ble);
bool ambient_ble_persist_accepted_peer(ambient_ble_t *ble,
                                      uint8_t identity_type,
                                      const uint8_t identity[6]);
bool ambient_ble_clear_accepted_peer(ambient_ble_t *ble);

bool ambient_ble_forced_disconnected(ambient_ble_t *, uint16_t);
void ambient_ble_resume_after_forced(ambient_ble_t *);
void ambient_ble_process_disconnect(ambient_ble_t *);
void ambient_ble_request_disconnect(ambient_ble_t *, uint16_t, uint64_t);
void ambient_ble_worker_disconnect(ambient_ble_t *);
