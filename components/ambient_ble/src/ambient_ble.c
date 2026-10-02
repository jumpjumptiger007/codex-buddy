/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ambient_ble_internal.h"

#include <stdlib.h>
#include <string.h>

#ifndef AMBIENT_BLE_HOST_TEST
#include "nvs_flash.h"
#endif

static const char *TAG = "ambient_ble";
ambient_ble_t *g_ambient_ble_active;

static bool ambient_ble_address_valid(const uint8_t address[6])
{
    uint8_t any = 0;

    for (size_t i = 0; i < 6; i++) {
        any |= address[i];
    }
    return any != 0;
}

static void ambient_ble_delete_allocations(ambient_ble_t *ble)
{
    if (ble == NULL) {
        return;
    }
    if (g_ambient_ble_active == ble) {
        g_ambient_ble_active = NULL;
    }
    if (ble->tx_queue != NULL) {
        vQueueDelete(ble->tx_queue);
    }
    if (ble->state_mutex != NULL) {
        vSemaphoreDelete(ble->state_mutex);
    }
    if (ble->peer_store_mutex != NULL) {
        vSemaphoreDelete(ble->peer_store_mutex);
    }
    if (ble->host_stopped != NULL) {
        vSemaphoreDelete(ble->host_stopped);
    }
    if (ble->tx_stopped != NULL) {
        vSemaphoreDelete(ble->tx_stopped);
    }
    if (ble->peer_nvs_open) {
        nvs_close(ble->peer_nvs);
    }
    free(ble);
}

void ambient_ble_reset_link_locked(ambient_ble_t *ble)
{
    memset(&ble->link, 0, sizeof(ble->link));
    ble->link.connection_handle = BLE_HS_CONN_HANDLE_NONE;
    ble->link.att_mtu = AMBIENT_BLE_DEFAULT_ATT_MTU;
    ambient_ble_ring_reset(&ble->rx_ring);
}

void ambient_ble_refresh_security_locked(ambient_ble_t *ble)
{
    struct ble_gap_conn_desc desc;

    ble->link.encrypted = false;
    ble->link.authenticated = false;
    ble->link.bonded = false;
    ble->link.secure_connections = false;
    ble->link.peer_identity_valid = false;
    ble->link.peer_identity_accepted = false;
    memset(ble->link.peer_identity, 0, sizeof(ble->link.peer_identity));
    ble->link.peer_identity_type = 0;

    if (ble->link.connection_handle == BLE_HS_CONN_HANDLE_NONE ||
        ble_gap_conn_find(ble->link.connection_handle, &desc) != 0) {
        return;
    }

    ble->link.encrypted = desc.sec_state.encrypted != 0;
    ble->link.authenticated = desc.sec_state.authenticated != 0;
    ble->link.bonded = desc.sec_state.bonded != 0;
    /* Secure Connections Only rejects legacy pairing at the NimBLE SM layer. */
    ble->link.secure_connections = ble->link.encrypted &&
                                   ble->link.authenticated &&
                                   ble_hs_cfg.sm_sc_only != 0;
    ble->link.peer_identity_type = desc.peer_id_addr.type;
    memcpy(ble->link.peer_identity, desc.peer_id_addr.val,
           sizeof(ble->link.peer_identity));
    ble->link.peer_identity_valid =
        ambient_ble_address_valid(ble->link.peer_identity);
    /* A NimBLE bond is trusted only if its identity was locally approved by
     * this project and that approval was committed to our own NVS key. */
    ble->link.peer_identity_accepted = ble->link.bonded &&
        ble->link.peer_identity_valid && ble->accepted_peer_valid &&
        ble->link.peer_identity_type == ble->accepted_peer_identity_type &&
        memcmp(ble->link.peer_identity, ble->accepted_peer_identity,
               sizeof(ble->accepted_peer_identity)) == 0;
}

static bool ambient_ble_link_equal(const ambient_ble_link_facts_t *a,
                                   const ambient_ble_link_facts_t *b)
{
    return a->connected == b->connected &&
           a->tx_notify_subscribed == b->tx_notify_subscribed &&
           a->encrypted == b->encrypted &&
           a->authenticated == b->authenticated &&
           a->bonded == b->bonded &&
           a->secure_connections == b->secure_connections &&
           a->peer_identity_valid == b->peer_identity_valid &&
           a->peer_identity_accepted == b->peer_identity_accepted &&
           a->peer_identity_type == b->peer_identity_type &&
           memcmp(a->peer_identity, b->peer_identity, sizeof(a->peer_identity)) == 0 &&
           a->connection_handle == b->connection_handle &&
           a->att_mtu == b->att_mtu &&
           a->link_incarnation == b->link_incarnation;
}

void ambient_ble_notify_link_change(ambient_ble_t *ble,
                                    ambient_ble_link_facts_t before,
                                    ambient_ble_link_facts_t after)
{
    if (ble != NULL && !ambient_ble_link_equal(&before, &after) &&
        ble->config.link_changed != NULL) {
        ble->config.link_changed(ble->config.context, &after);
    }
}

void ambient_ble_clear_io(ambient_ble_t *ble)
{
    ambient_ble_tx_item_t discarded;

    if (ble == NULL) {
        return;
    }
    if (ble->state_mutex != NULL &&
        xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        ambient_ble_ring_reset(&ble->rx_ring);
        xSemaphoreGive(ble->state_mutex);
    }
    if (ble->tx_queue != NULL) {
        while (xQueueReceive(ble->tx_queue, &discarded, 0) == pdTRUE) {
        }
    }
}

esp_err_t ambient_ble_get_link_facts(ambient_ble_t *ble,
                                     ambient_ble_link_facts_t *out_facts)
{
    if (ble == NULL || out_facts == NULL || ble->state_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out_facts = ble->link;
    if (ble->unpairing || ble->shutting_down || ble->disconnect_pending) {
        out_facts->peer_identity_accepted = false;
    }
    xSemaphoreGive(ble->state_mutex);
    return ESP_OK;
}

esp_err_t ambient_ble_read(ambient_ble_t *ble,
                           uint8_t *out,
                           size_t capacity,
                           size_t *out_length)
{
    if (out_length != NULL) {
        *out_length = 0;
    }
    if (ble == NULL || out == NULL || capacity == 0 || out_length == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (ble->unpairing || ble->shutting_down || ble->disconnect_pending || !ble->link.connected || !ble->link.tx_notify_subscribed ||
        !ble->link.encrypted || !ble->link.authenticated ||
        !ble->link.bonded || !ble->link.secure_connections ||
        !ble->link.peer_identity_valid || !ble->link.peer_identity_accepted ||
        ble->link.link_incarnation == 0) {
        xSemaphoreGive(ble->state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    *out_length = ambient_ble_ring_read(&ble->rx_ring, out, capacity);
    xSemaphoreGive(ble->state_mutex);
    return ESP_OK;
}

esp_err_t ambient_ble_send(ambient_ble_t *ble,
                           const uint8_t *frame,
                           size_t frame_length)
{
    ambient_ble_link_facts_t facts;
    ambient_ble_tx_item_t item;
    esp_err_t err;

    if (ble == NULL || frame == NULL || frame_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame_length > AMBIENT_BLE_MAX_TX_FRAME_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }
    err = ambient_ble_get_link_facts(ble, &facts);
    if (err != ESP_OK) {
        return err;
    }
    if (!facts.connected || !facts.tx_notify_subscribed || !facts.encrypted ||
        !facts.authenticated || !facts.bonded || !facts.secure_connections ||
        !facts.peer_identity_valid || !facts.peer_identity_accepted) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&item, 0, sizeof(item));
    item.kind = AMBIENT_BLE_TX_ITEM_FRAME;
    item.connection_handle = facts.connection_handle;
    item.link_incarnation = facts.link_incarnation;
    item.length = (uint16_t)frame_length;
    memcpy(item.bytes, frame, frame_length);
    if (xQueueSend(ble->tx_queue, &item, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    atomic_store(&ble->tx_high_water,1);
    return ESP_OK;
}

static bool ambient_ble_create_resources(ambient_ble_t *ble)
{
    ble->state_mutex = xSemaphoreCreateMutex();
    ble->peer_store_mutex = xSemaphoreCreateMutex();
    ble->host_stopped = xSemaphoreCreateBinary();
    ble->tx_stopped = xSemaphoreCreateBinary();
    ble->tx_queue = xQueueCreate(AMBIENT_BLE_TX_QUEUE_DEPTH,
                                 sizeof(ambient_ble_tx_item_t));
    return ble->state_mutex != NULL && ble->peer_store_mutex != NULL &&
           ble->host_stopped != NULL &&
           ble->tx_stopped != NULL && ble->tx_queue != NULL;
}

static esp_err_t ambient_ble_start_failure(ambient_ble_t *ble,
    esp_err_t cause, ambient_ble_t **out_ble)
{
    esp_err_t cleanup = ambient_ble_stop(ble);
    if (cleanup != ESP_OK) {
        /* A failed start still returns recoverable ownership. Caller must stop
         * this object again; global owner stays reachable and no state is freed. */
        *out_ble = ble;
        return cleanup;
    }
    return cause;
}

esp_err_t ambient_ble_start(const ambient_ble_config_t *config,
                           ambient_ble_t **out_ble)
{
    ambient_ble_t *ble;
    const char *name;
    size_t name_length;
    esp_err_t err;
    int rc;

    if (out_ble != NULL) {
        *out_ble = NULL;
    }
    if (config == NULL || out_ble == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_ambient_ble_active != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    name = (config->advertising_name != NULL &&
            config->advertising_name[0] != '\0') ?
           config->advertising_name : "FoloPassport";
    name_length = strnlen(name, AMBIENT_BLE_NAME_MAX + 1U);
    if (name_length == 0 || name_length > AMBIENT_BLE_NAME_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed; no erase attempted (err=%s)",
                 esp_err_to_name(err));
        return err;
    }

    ble = calloc(1, sizeof(*ble));
    if (ble == NULL) {
        return ESP_ERR_NO_MEM;
    }
    atomic_init(&ble->tx_high_water,0);
    atomic_init(&ble->disconnect_pending,false);
    ble->disconnect_mux=(portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ble->config = *config;
    ble->unpair_handle = BLE_HS_CONN_HANDLE_NONE;
    ble->stop_handle = BLE_HS_CONN_HANDLE_NONE;
    ble->forced_handle = BLE_HS_CONN_HANDLE_NONE;
    memcpy(ble->advertising_name, name, name_length);
    ble->advertising_name[name_length] = '\0';
    ambient_ble_reset_link_locked(ble);
    if (!ambient_ble_create_resources(ble)) {
        ambient_ble_delete_allocations(ble);
        return ESP_ERR_NO_MEM;
    }
    err = nvs_open("ambient_r3", NVS_READWRITE, &ble->peer_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "peer store open failed: %s", esp_err_to_name(err));
        ambient_ble_delete_allocations(ble);
        return err;
    }
    ble->peer_nvs_open = true;
    if (!ambient_ble_load_accepted_peer(ble)) {
        ESP_LOGE(TAG, "accepted peer record invalid; BLE startup denied");
        ambient_ble_delete_allocations(ble);
        return ESP_ERR_INVALID_STATE;
    }

    g_ambient_ble_active = ble;
    ambient_ble_prepare_gatt(ble);
    err = nimble_port_init();
    if (err != ESP_OK) {
        return ambient_ble_start_failure(ble, err, out_ble);
    }
    ble->initialized = true;

    ble_hs_cfg.reset_cb = ambient_ble_on_reset;
    ble_hs_cfg.sync_cb = ambient_ble_on_sync;
    ble_hs_cfg.gatts_register_cb = ambient_ble_gatt_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_sc_only = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_gatts_count_cfg(ble->gatt_svcs);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(ble->gatt_svcs);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT registration failed rc=%d", rc);
        return ambient_ble_start_failure(ble, ESP_FAIL, out_ble);
    }

    if (xTaskCreate(ambient_ble_tx_task, "ambient_ble_tx",
                    AMBIENT_BLE_TX_TASK_STACK, ble,
                    AMBIENT_BLE_TX_TASK_PRIORITY, &ble->tx_task) != pdPASS) {
        return ambient_ble_start_failure(ble, ESP_ERR_NO_MEM, out_ble);
    }
    ble->tx_task_started = true;

    if (xTaskCreate(ambient_ble_host_task, "ambient_ble_host",
                    NIMBLE_HS_STACK_SIZE, ble,
                    configMAX_PRIORITIES - 4, &ble->host_task) != pdPASS) {
        return ambient_ble_start_failure(ble, ESP_ERR_NO_MEM, out_ble);
    }
    ble->host_task_started = true;
    *out_ble = ble;
    return ESP_OK;
}

esp_err_t ambient_ble_stop(ambient_ble_t *ble)
{
    ambient_ble_link_facts_t before;
    ambient_ble_link_facts_t after;
    int rc;

    if (ble == NULL) return g_ambient_ble_active == NULL ? ESP_OK : ESP_ERR_INVALID_ARG;
    if (ble != g_ambient_ble_active) {
        return ESP_ERR_INVALID_ARG;
    }
    ble->shutting_down = true;
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    before = ble->link;
    if (before.connection_handle != BLE_HS_CONN_HANDLE_NONE)
        ble->stop_handle = before.connection_handle;
    ambient_ble_reset_link_locked(ble);
    after = ble->link;
    xSemaphoreGive(ble->state_mutex);
    ambient_ble_notify_link_change(ble, before, after);
    ambient_ble_clear_io(ble);

    if (ble->tx_task_started) {
        if (xSemaphoreTake(ble->tx_stopped,
                           pdMS_TO_TICKS(AMBIENT_BLE_STOP_TIMEOUT_MS)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        ble->tx_task_started = false;
        ble->tx_task = NULL;
    }

    if (ble->host_task_started) {
        (void)ble_gap_adv_stop();
        if (ble->stop_handle != BLE_HS_CONN_HANDLE_NONE) {
            (void)ble_gap_terminate(ble->stop_handle, BLE_ERR_REM_USER_CONN_TERM);
            ble->stop_handle = BLE_HS_CONN_HANDLE_NONE;
        }
        if (!ble->host_stop_requested) {
            rc = nimble_port_stop();
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                ESP_LOGE(TAG, "NimBLE stop failed rc=%d", rc);
                return ESP_FAIL;
            }
            ble->host_stop_requested = true;
        }
        if (xSemaphoreTake(ble->host_stopped,
                           pdMS_TO_TICKS(AMBIENT_BLE_STOP_TIMEOUT_MS)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        ble->host_task_started = false;
        vTaskDelete(ble->host_task);
        ble->host_task = NULL;
    }

    if (ble->initialized) {
        rc = nimble_port_deinit();
        if (rc != ESP_OK) {
            ESP_LOGE(TAG, "NimBLE deinit failed rc=%d", rc);
            return rc;
        }
        ble->initialized = false;
    }
    ambient_ble_delete_allocations(ble);
    return ESP_OK;
}

esp_err_t ambient_ble_unpair_local(ambient_ble_t *ble)
{
    ble_addr_t peer[1];
    int peer_count = 0;
    if (ble == NULL || ble != g_ambient_ble_active || !ble->initialized ||
        ble->shutting_down) return ESP_ERR_INVALID_ARG;
    /* Atomic fail-closed latch precedes every fallible lock/store operation. */
    ble->unpairing = true;
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    ambient_ble_link_facts_t before = ble->link;
    if (before.connection_handle != BLE_HS_CONN_HANDLE_NONE)
        ble->unpair_handle = before.connection_handle;
    ble->pending_pair_approved = false;
    ble->pairing_link_incarnation = 0;
    memset(ble->pending_pair_identity, 0, sizeof(ble->pending_pair_identity));
    ble->accepted_peer_valid = false;
    memset(ble->accepted_peer_identity, 0, sizeof(ble->accepted_peer_identity));
    if(before.connected){ble->forced_handle=before.connection_handle;
        ble->forced_incarnation=before.link_incarnation;ble->forced_provisional=false;ble->forced_disconnected=false;}
    ambient_ble_reset_link_locked(ble);
    ambient_ble_link_facts_t after = ble->link;
    xSemaphoreGive(ble->state_mutex);
    ambient_ble_clear_io(ble);
    ambient_ble_notify_link_change(ble, before, after);
    if (!ambient_ble_clear_accepted_peer(ble)) return ESP_FAIL;
    if (ble_store_util_bonded_peers(peer, &peer_count, 1) != 0 ||
        peer_count < 0 || peer_count > 1) return ESP_FAIL;
    if (peer_count == 1 && ble_store_util_delete_peer(&peer[0]) != 0)
        return ESP_FAIL;
    if (ble->unpair_handle != BLE_HS_CONN_HANDLE_NONE) {
        int rc = ble_gap_terminate(ble->unpair_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc != 0 && rc != BLE_HS_ENOTCONN) return ESP_FAIL;
        if(rc==BLE_HS_ENOTCONN)ambient_ble_forced_disconnected(ble,ble->unpair_handle);
        ble->unpair_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    ble->unpairing = false;
    ambient_ble_resume_after_forced(ble);
    return ESP_OK;
}

void ambient_ble_host_task(void *arg)
{
    ambient_ble_t *ble = arg;

    nimble_port_run();
    if (ble != NULL && ble->host_stopped != NULL) {
        xSemaphoreGive(ble->host_stopped);
    }
    vTaskSuspend(NULL);
}

esp_err_t ambient_ble_get_metrics(ambient_ble_t *ble, ambient_ble_metrics_t *out)
{
    if (!ble || ble!=g_ambient_ble_active || !out || ble->shutting_down) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(ble->state_mutex,pdMS_TO_TICKS(20))!=pdTRUE) return ESP_ERR_TIMEOUT;
    memset(out,0,sizeof(*out));
    out->rx_bytes=ble->rx_ring.length;out->rx_high_water_bytes=ble->rx_high_water;
    out->tx_high_water_slots=atomic_load(&ble->tx_high_water);
    out->termination_attempts=ble->termination_attempts;
    out->termination_fault=ble->termination_fault;
#ifndef AMBIENT_BLE_HOST_TEST
    out->task_stack_available=ble->tx_task_started && ble->host_task_started;
    if(out->task_stack_available){
        out->tx_stack_free_min_bytes=uxTaskGetStackHighWaterMark(ble->tx_task);
        out->host_stack_free_min_bytes=uxTaskGetStackHighWaterMark(ble->host_task);
    }
#endif
    xSemaphoreGive(ble->state_mutex);return ESP_OK;
}
