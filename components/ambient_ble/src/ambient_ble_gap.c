/*
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 *
 * GAP lifecycle mechanics adapted from Espressif esp-desktop-buddy S5 at
 * b6bac05db208717676e70180e5269d79f32b2d68. This file owns no semantic model.
 */
#include "ambient_ble_internal.h"

#include <string.h>

static const char *TAG = "ambient_ble_gap";

/* UUIDv5 namespace: https://yliu.tech/ai-passport/ambient-ble/v1/ */
static const ble_uuid128_t AMBIENT_SERVICE_UUID =
    BLE_UUID128_INIT(0x81, 0x38, 0xdb, 0xb3, 0x0d, 0xc9, 0xc7, 0xb7,
                     0xce, 0x53, 0x32, 0x07, 0x62, 0xbf, 0x13, 0x09);

static bool ambient_ble_has_bond(void)
{
    ble_addr_t peer[1];
    int count = 0;

    return ble_store_util_bonded_peers(peer, &count, 1) != 0 || count != 0;
}

static void ambient_ble_clear_pending_pairing(ambient_ble_t *ble)
{
    ble->pending_pair_approved = false;
    ble->pairing_link_incarnation = 0;
    ble->pending_pair_identity_type = 0;
    memset(ble->pending_pair_identity, 0, sizeof(ble->pending_pair_identity));
}

static bool ambient_ble_identity_nonzero(const uint8_t identity[6])
{
    uint8_t any = 0;

    for (size_t i = 0; i < 6; ++i) {
        any |= identity[i];
    }
    return any != 0;
}

static bool ambient_ble_peer_matches(const ambient_ble_link_facts_t *facts,
                                     uint8_t identity_type,
                                     const uint8_t identity[6])
{
    return facts->peer_identity_valid &&
           facts->peer_identity_type == identity_type &&
           memcmp(facts->peer_identity, identity, sizeof(facts->peer_identity)) == 0;
}

static void ambient_ble_start_advertising_impl(ambient_ble_t *ble)
{
    struct ble_gap_adv_params params = {0};
    struct ble_hs_adv_fields adv = {0};
    struct ble_hs_adv_fields rsp = {0};
    int rc;

    if (ble == NULL || ble->shutting_down || ble->own_addr_type == 0xff) {
        return;
    }

    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.uuids128 = (ble_uuid128_t *)&AMBIENT_SERVICE_UUID;
    adv.num_uuids128 = 1;
    adv.uuids128_is_complete = 1;
    rc = ble_gap_adv_set_fields(&adv);
    if (rc != 0) {
        ESP_LOGE(TAG, "advertising fields failed rc=%d", rc);
        return;
    }

    rsp.name = (uint8_t *)ble->advertising_name;
    rsp.name_len = (uint8_t)strlen(ble->advertising_name);
    rsp.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan response fields failed rc=%d", rc);
        return;
    }

    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble->own_addr_type, NULL, BLE_HS_FOREVER,
                           &params, ambient_ble_gap_event, ble);
    if (rc != 0) {
        ESP_LOGE(TAG, "advertising start failed rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising project service");
}

void ambient_ble_start_advertising(ambient_ble_t *ble)
{
    ambient_ble_link_facts_t facts;

    if (ble == NULL || ble->shutting_down ||
        ambient_ble_get_link_facts(ble, &facts) != ESP_OK || facts.connected) {
        return;
    }
    ambient_ble_start_advertising_impl(ble);
}

void ambient_ble_on_reset(int reason)
{
    ambient_ble_t *ble = g_ambient_ble_active;
    ambient_ble_link_facts_t before;
    ambient_ble_link_facts_t after;

    ESP_LOGE(TAG, "NimBLE reset reason=%d", reason);
    if (ble == NULL || ble->state_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    before = ble->link;
    ambient_ble_reset_link_locked(ble);
    ble->pending_pair_approved = false;
    ble->pairing_link_incarnation = 0;
    ble->pending_pair_identity_type = 0;
    memset(ble->pending_pair_identity, 0, sizeof(ble->pending_pair_identity));
    after = ble->link;
    xSemaphoreGive(ble->state_mutex);
    ambient_ble_clear_io(ble);
    ambient_ble_notify_link_change(ble, before, after);
}

void ambient_ble_on_sync(void)
{
    ambient_ble_t *ble = g_ambient_ble_active;
    uint8_t addr[6];
    int rc;

    if (ble == NULL || ble->shutting_down) {
        return;
    }
    ble->own_addr_type = 0xff;
    rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &ble->own_addr_type);
    }
    if (rc == 0) {
        rc = ble_hs_id_copy_addr(ble->own_addr_type, addr, NULL);
    }
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(ble->advertising_name);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "NimBLE sync setup failed rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "NimBLE host synchronized");
    ambient_ble_start_advertising_impl(ble);
}

static bool ambient_ble_update_link(ambient_ble_t *ble,
                                    uint16_t conn_handle,
                                    bool connected,
                                    bool update_subscription,
                                    bool subscribed,
                                    bool refresh_security)
{
    ambient_ble_link_facts_t before;
    ambient_ble_link_facts_t after;

    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }
    before = ble->link;
    if (!connected && (!ble->link.connected ||
        ble->link.connection_handle != conn_handle)) {
        xSemaphoreGive(ble->state_mutex);
        return false;
    }
    if (connected) {
        /* Each successful CONNECT is a fresh physical link, even when the
         * controller reused a handle before old project facts were cleaned. */
        {
            if (ble->next_link_incarnation == UINT64_MAX) {
                xSemaphoreGive(ble->state_mutex);
                return false;
            }
            ambient_ble_clear_pending_pairing(ble);
            ambient_ble_ring_reset(&ble->rx_ring);
            ambient_ble_tx_item_t discarded;
            (void)xQueueReceive(ble->tx_queue,&discarded,0);
            ble->next_link_incarnation++;
            ble->termination_attempts=0;ble->termination_retry=false;
            ble->termination_fault=false;
            ble->link.link_incarnation = ble->next_link_incarnation;
            portENTER_CRITICAL(&ble->disconnect_mux);
            ble->published_incarnation=ble->next_link_incarnation;
            ble->published_handle=conn_handle;
            portEXIT_CRITICAL(&ble->disconnect_mux);
            ble->link.att_mtu = AMBIENT_BLE_DEFAULT_ATT_MTU;
            ble->link.tx_notify_subscribed = false;
        }
        ble->link.connected = true;
        ble->link.connection_handle = conn_handle;
        portENTER_CRITICAL(&ble->disconnect_mux);
        if(ble->provisional_active && ble->provisional_handle==conn_handle)
            ble->provisional_active=false;
        if(!ble->pending_disconnect_event)atomic_store(&ble->disconnect_pending,false);
        portEXIT_CRITICAL(&ble->disconnect_mux);
    }
    if (update_subscription) {
        ble->link.tx_notify_subscribed = subscribed;
    }
    if (refresh_security) {
        ambient_ble_refresh_security_locked(ble);
    }
    after = ble->link;
    xSemaphoreGive(ble->state_mutex);
    if (before.link_incarnation != after.link_incarnation ||
        (before.tx_notify_subscribed && !after.tx_notify_subscribed) ||
        (before.encrypted && !after.encrypted) ||
        (before.authenticated && !after.authenticated) ||
        (before.peer_identity_accepted && !after.peer_identity_accepted)) {
        ambient_ble_clear_io(ble);
    }
    ambient_ble_notify_link_change(ble, before, after);
    return true;
}

static void ambient_ble_request_provisional(ambient_ble_t *ble,uint16_t handle,uint64_t serial)
{
    portENTER_CRITICAL(&ble->disconnect_mux);
    if(!ble->provisional_active || ble->provisional_handle!=handle || ble->provisional_serial!=serial){
        portEXIT_CRITICAL(&ble->disconnect_mux);return;
    }
    if(!ble->pending_disconnect_event || !ble->pending_disconnect_provisional ||
       ble->pending_disconnect_incarnation!=serial){
        ble->pending_disconnect_handle=handle;ble->pending_disconnect_incarnation=serial;
        ble->pending_disconnect_event=true;ble->pending_disconnect_terminate=true;
        ble->pending_disconnect_provisional=true;
    }
    atomic_store(&ble->disconnect_pending,true);
    portEXIT_CRITICAL(&ble->disconnect_mux);
    ambient_ble_process_disconnect(ble);
}

static void ambient_ble_reject_current(ambient_ble_t *ble,uint16_t handle)
{
    portENTER_CRITICAL(&ble->disconnect_mux);
    uint64_t incarnation=ble->published_incarnation;
    bool current=handle==ble->published_handle;
    portEXIT_CRITICAL(&ble->disconnect_mux);
    if(current)ambient_ble_request_disconnect(ble,handle,incarnation);
}

int ambient_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    ambient_ble_t *ble = arg;
    struct ble_gap_conn_desc desc;
    ambient_ble_link_facts_t before;
    ambient_ble_link_facts_t after;
    int rc;

    if (ble == NULL || event == NULL || ble->shutting_down ||
        (ble->unpairing && event->type!=BLE_GAP_EVENT_DISCONNECT)) {
        return 0;
    }

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connection attempt failed status=%d",
                     event->connect.status);
            ambient_ble_start_advertising_impl(ble);
            return 0;
        }
        /* Record physical ownership before the first fallible state lock. */
        portENTER_CRITICAL(&ble->disconnect_mux);
        if(ble->provisional_active){
            ble->retired_provisional_handle=ble->provisional_handle;
            ble->retired_provisional_serial=ble->provisional_serial;
        }
        bool serial_available=ble->provisional_serial!=UINT64_MAX;
        if(serial_available)ble->provisional_serial++;
        ble->provisional_active=true;ble->provisional_handle=event->connect.conn_handle;
        uint64_t serial=ble->provisional_serial;
        atomic_store(&ble->disconnect_pending,true);
        portEXIT_CRITICAL(&ble->disconnect_mux);
        if(!serial_available || !ambient_ble_update_link(ble,event->connect.conn_handle,true,false,false,true)){
            ambient_ble_request_provisional(ble,event->connect.conn_handle,serial);
            return 0;
        }
        rc = ble_gap_security_initiate(event->connect.conn_handle);
        if (rc != 0 && rc != BLE_HS_EALREADY) {
            ESP_LOGW(TAG, "security initiation failed rc=%d", rc);
            ambient_ble_reject_current(ble,event->connect.conn_handle);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        (void)ambient_ble_forced_disconnected(ble,event->disconnect.conn.conn_handle);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (event->adv_complete.reason != 0) {
            ambient_ble_start_advertising(ble);
        }
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle != ble->tx_value_handle) {
            return 0;
        }
        if(!ambient_ble_update_link(ble,event->subscribe.conn_handle,false,true,
                                    event->subscribe.cur_notify!=0,true))
            ambient_ble_reject_current(ble,event->subscribe.conn_handle);
        return 0;

    case BLE_GAP_EVENT_MTU:
        if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            before = ble->link;
            if (ble->link.connected &&
                ble->link.connection_handle == event->mtu.conn_handle &&
                event->mtu.channel_id == BLE_L2CAP_CID_ATT &&
                event->mtu.value >= AMBIENT_BLE_DEFAULT_ATT_MTU) {
                ble->link.att_mtu = event->mtu.value;
            }
            after = ble->link;
            xSemaphoreGive(ble->state_mutex);
            ambient_ble_notify_link_change(ble, before, after);
        }else{ambient_ble_reject_current(ble,event->mtu.conn_handle);}
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
    {
        ambient_ble_tx_item_t approved_peer = {0};
        bool known_accepted;
        bool local_approval_matches;

        esp_err_t observed=ambient_ble_get_link_facts(ble,&before);
        if(observed!=ESP_OK){
            if(observed==ESP_ERR_TIMEOUT)ambient_ble_reject_current(ble,event->enc_change.conn_handle);
            return 0;
        }
        if(!before.connected || before.connection_handle!=event->enc_change.conn_handle)return 0;
        if(!ambient_ble_update_link(ble,event->enc_change.conn_handle,false,false,false,true)){
            ambient_ble_reject_current(ble,event->enc_change.conn_handle);return 0;
        }
        if (event->enc_change.status != 0) {
            ambient_ble_clear_pending_pairing(ble);
            ESP_LOGW(TAG, "pairing/encryption failed status=%d",
                     event->enc_change.status);
            ambient_ble_reject_current(ble,event->enc_change.conn_handle);
            return 0;
        }
        if (ambient_ble_get_link_facts(ble, &after) != ESP_OK ||
            !after.connected || !after.encrypted || !after.authenticated ||
            !after.bonded || !after.secure_connections ||
            !after.peer_identity_valid) {
            ambient_ble_clear_pending_pairing(ble);
            ESP_LOGW(TAG, "security requirements not satisfied");
            ambient_ble_reject_current(ble,event->enc_change.conn_handle);
            return 0;
        }
        known_accepted = after.peer_identity_accepted;
        local_approval_matches = ble->pending_pair_approved &&
            ble->pairing_link_incarnation == after.link_incarnation &&
            ambient_ble_peer_matches(&after, ble->pending_pair_identity_type,
                                     ble->pending_pair_identity);
        if (known_accepted) {
            ambient_ble_clear_pending_pairing(ble);
            return 0;
        }
        if (!local_approval_matches) {
            ambient_ble_clear_pending_pairing(ble);
            ESP_LOGW(TAG, "secure peer lacks a locally accepted identity");
            ambient_ble_reject_current(ble,event->enc_change.conn_handle);
            return 0;
        }
        approved_peer.kind = AMBIENT_BLE_TX_ITEM_ACCEPT_PEER;
        approved_peer.connection_handle = event->enc_change.conn_handle;
        approved_peer.link_incarnation = after.link_incarnation;
        approved_peer.peer_identity_type = after.peer_identity_type;
        memcpy(approved_peer.peer_identity, after.peer_identity,
               sizeof(approved_peer.peer_identity));
        if (xQueueSend(ble->tx_queue, &approved_peer, 0) != pdTRUE) {
            ambient_ble_clear_pending_pairing(ble);
            ESP_LOGW(TAG, "peer trust commit queue full");
            ambient_ble_reject_current(ble,event->enc_change.conn_handle);
            return 0;
        }
        atomic_store(&ble->tx_high_water,1);
        ambient_ble_clear_pending_pairing(ble);
        return 0;
    }

    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            before = ble->link;
            if (ble->link.connection_handle == event->identity_resolved.conn_handle) {
                ble->link.peer_identity_type = event->identity_resolved.peer_id_addr.type;
                memcpy(ble->link.peer_identity,
                       event->identity_resolved.peer_id_addr.val,
                       sizeof(ble->link.peer_identity));
                ble->link.peer_identity_valid =
                    ambient_ble_identity_nonzero(ble->link.peer_identity);
                ble->link.peer_identity_accepted = ble->link.bonded &&
                    ble->link.peer_identity_valid && ble->accepted_peer_valid &&
                    ble->link.peer_identity_type == ble->accepted_peer_identity_type &&
                    memcmp(ble->link.peer_identity, ble->accepted_peer_identity,
                           sizeof(ble->accepted_peer_identity)) == 0;
            }
            after = ble->link;
            xSemaphoreGive(ble->state_mutex);
            ambient_ble_notify_link_change(ble, before, after);
        }else{ambient_ble_reject_current(ble,event->identity_resolved.conn_handle);}
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Never erase an existing bond implicitly to recover pairing. */
        ESP_LOGW(TAG, "repeat pairing denied; stored bond retained");
        ambient_ble_reject_current(ble,event->repeat_pairing.conn_handle);
        return BLE_GAP_REPEAT_PAIRING_IGNORE;

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io io = {0};
        uint32_t passkey;
        uint64_t link_incarnation;
        bool reject=false;

        if (event->passkey.params.action != BLE_SM_IOACT_DISP ||
            ble->config.pairing_approval == NULL ||
            ambient_ble_has_bond() ||
            ble_gap_conn_find(event->passkey.conn_handle, &desc) != 0 ||
            !ambient_ble_identity_nonzero(desc.peer_id_addr.val)) {
            ESP_LOGW(TAG, "new pairing denied: no local approval path");
            ambient_ble_reject_current(ble,event->passkey.conn_handle);
            return 0;
        }
        passkey = esp_random() % 1000000U;
        if (!ble->config.pairing_approval(ble->config.context,
                                           desc.peer_id_addr.val,
                                           desc.peer_id_addr.type,
                                           passkey)) {
            ESP_LOGW(TAG, "new pairing denied by local approval path");
            ambient_ble_reject_current(ble,event->passkey.conn_handle);
            return 0;
        }
        io.action = BLE_SM_IOACT_DISP;
        io.passkey = passkey;
        rc = ble_sm_inject_io(event->passkey.conn_handle, &io);
        if (rc != 0) {
            ESP_LOGW(TAG, "passkey injection failed rc=%d", rc);
            ambient_ble_reject_current(ble,event->passkey.conn_handle);
        } else if (xSemaphoreTake(ble->state_mutex,
                                  pdMS_TO_TICKS(20)) == pdTRUE) {
            link_incarnation = ble->link.link_incarnation;
            if (!ble->unpairing && !ble->disconnect_pending && ble->link.connected &&
                ble->link.connection_handle == event->passkey.conn_handle &&
                link_incarnation != 0) {
                ble->pending_pair_approved = true;
                ble->pairing_link_incarnation = link_incarnation;
                ble->pending_pair_identity_type = desc.peer_id_addr.type;
                memcpy(ble->pending_pair_identity, desc.peer_id_addr.val,
                       sizeof(ble->pending_pair_identity));
            } else {
                reject=true;
            }
            xSemaphoreGive(ble->state_mutex);
            if(reject)ambient_ble_reject_current(ble,event->passkey.conn_handle);
        } else {
            ambient_ble_reject_current(ble,event->passkey.conn_handle);
        }
        return 0;
    }

    default:
        return 0;
    }
}

/* Also used for ordinary disconnects. A successfully published fixed slot is
 * handled even if the fallible state mutex cannot be acquired in this callback.
 * The existing TX worker retries at most once per iteration (100ms idle wait). */
bool ambient_ble_forced_disconnected(ambient_ble_t *ble,uint16_t handle)
{
    if(!ble || ble->shutting_down)return false;
    portENTER_CRITICAL(&ble->disconnect_mux);
    bool provisional=ble->provisional_active && handle==ble->provisional_handle;
    if(!provisional && ble->retired_provisional_serial && handle==ble->retired_provisional_handle){
        ble->retired_provisional_serial=0;
        portEXIT_CRITICAL(&ble->disconnect_mux);return false;
    }
    uint64_t incarnation=provisional?ble->provisional_serial:ble->published_incarnation;
    if(!incarnation || (!provisional && (ble->provisional_active || handle!=ble->published_handle))){
        portEXIT_CRITICAL(&ble->disconnect_mux);return false;
    }
    if(!ble->pending_disconnect_event || ble->pending_disconnect_provisional!=provisional ||
       ble->pending_disconnect_incarnation!=incarnation){
        ble->pending_disconnect_handle=handle;ble->pending_disconnect_incarnation=incarnation;
        ble->pending_disconnect_event=true;ble->pending_disconnect_provisional=provisional;
    }
    ble->pending_disconnect_terminate=false;
    atomic_store(&ble->disconnect_pending,true);
    portEXIT_CRITICAL(&ble->disconnect_mux);
    ambient_ble_process_disconnect(ble);
    return true;
}

void ambient_ble_request_disconnect(ambient_ble_t *ble,uint16_t handle,uint64_t incarnation)
{
    if(!ble || ble->shutting_down)return;
    portENTER_CRITICAL(&ble->disconnect_mux);
    if(ble->provisional_active || handle!=ble->published_handle || incarnation!=ble->published_incarnation || !incarnation){
        portEXIT_CRITICAL(&ble->disconnect_mux);
        return;
    }
    if(!ble->pending_disconnect_event || ble->pending_disconnect_provisional || ble->pending_disconnect_incarnation!=incarnation){
        ble->pending_disconnect_handle=handle;
        ble->pending_disconnect_incarnation=incarnation;
        ble->pending_disconnect_terminate=true;
        ble->pending_disconnect_provisional=false;
        ble->pending_disconnect_event=true;
    }
    atomic_store(&ble->disconnect_pending,true);
    portEXIT_CRITICAL(&ble->disconnect_mux);
    ambient_ble_process_disconnect(ble);
}

void ambient_ble_resume_after_forced(ambient_ble_t *ble)
{
    if(!ble || ble->shutting_down)return;
    atomic_store(&ble->disconnect_pending,true);
    ambient_ble_process_disconnect(ble);
}

static void ambient_ble_process_disconnect_impl(ambient_ble_t *ble,bool worker)
{
    if(!ble || ble->shutting_down || !ble->disconnect_pending)return;
    if(xSemaphoreTake(ble->state_mutex,pdMS_TO_TICKS(20))!=pdTRUE)return;
    ambient_ble_link_facts_t before=ble->link,after;
    bool changed=false,resume=false;
    portENTER_CRITICAL(&ble->disconnect_mux);
    bool event=ble->pending_disconnect_event;
    bool terminate=ble->pending_disconnect_terminate;
    bool provisional=ble->pending_disconnect_provisional;
    bool provisional_current=ble->provisional_active;
    uint64_t provisional_serial=ble->provisional_serial;
    uint16_t provisional_handle=ble->provisional_handle;
    uint16_t handle=ble->pending_disconnect_handle;
    uint64_t incarnation=ble->pending_disconnect_incarnation;
    /* A preserved observed callback already discharged this exact old owner.
     * Its stale slot must not leave a handle-only tombstone for a newer link.
     * Pending terminate requests still await their physical callback. */
    if(event && provisional && !terminate &&
       ble->retired_provisional_serial==incarnation &&
       ble->retired_provisional_handle==handle){
        ble->retired_provisional_serial=0;
        ble->retired_provisional_handle=BLE_HS_CONN_HANDLE_NONE;
    }
    ble->pending_disconnect_event=false;
    portEXIT_CRITICAL(&ble->disconnect_mux);
    if(event){
        if(provisional){
            if(provisional_current && provisional_serial==incarnation && provisional_handle==handle){
                bool same=ble->forced_provisional && ble->forced_incarnation==incarnation && ble->forced_handle==handle;
                if(!same){
                    ble->forced_handle=handle;ble->forced_incarnation=incarnation;ble->forced_provisional=true;
                    ble->forced_disconnected=false;ble->termination_attempts=0;
                    ble->termination_retry=terminate;ble->termination_fault=false;
                    ambient_ble_clear_pending_pairing(ble);ambient_ble_reset_link_locked(ble);changed=true;
                }
                if(!terminate)ble->forced_disconnected=true;
            }
        }else if(terminate){
            if(incarnation==ble->next_link_incarnation && before.connected &&
               before.connection_handle==handle){
                ble->forced_handle=handle;ble->forced_incarnation=incarnation;ble->forced_provisional=false;
                ble->forced_disconnected=false;
                ble->termination_attempts=0;ble->termination_retry=true;
                ble->termination_fault=false;
                ambient_ble_clear_pending_pairing(ble);
                ambient_ble_reset_link_locked(ble);changed=true;
            }
        }else if(ble->forced_incarnation && !ble->forced_provisional && ble->forced_handle==handle){
            if(ble->forced_incarnation==incarnation)ble->forced_disconnected=true;
        }else if(incarnation==ble->next_link_incarnation && before.connected &&
                 before.connection_handle==handle){
            ambient_ble_clear_pending_pairing(ble);
            ambient_ble_reset_link_locked(ble);
            changed=true;resume=true;
            if(ble->unpairing){
                /* A disconnect can follow an unpair whose first mutex failed,
                 * before that caller had installed its forced marker. */
                ble->forced_handle=handle;ble->forced_incarnation=incarnation;ble->forced_provisional=false;
                ble->forced_disconnected=true;resume=false;
            }
        }
    }
    portENTER_CRITICAL(&ble->disconnect_mux);
    bool owned=ble->forced_incarnation && (ble->forced_provisional?
        (ble->provisional_active && ble->provisional_serial==ble->forced_incarnation &&
         ble->provisional_handle==ble->forced_handle):
        (!ble->provisional_active && ble->next_link_incarnation==ble->forced_incarnation));
    portEXIT_CRITICAL(&ble->disconnect_mux);
    /* Only the worker retries; duplicate callbacks/requests cannot accelerate
     * retries or reset the finite budget. Tick subtraction tolerates rollover. */
    if(owned && ble->termination_retry &&
       !ble->forced_disconnected && !ble->unpairing && !ble->shutting_down &&
       (ble->termination_attempts==0 || (worker &&
        (TickType_t)(xTaskGetTickCount()-ble->termination_last_attempt)>=
            pdMS_TO_TICKS(AMBIENT_BLE_TERMINATION_RETRY_MS)))){
        ble->termination_last_attempt=xTaskGetTickCount();
        ble->termination_attempts++;
        int rc=ble_gap_terminate(ble->forced_handle,BLE_ERR_REM_USER_CONN_TERM);
        if(rc==0){ble->termination_retry=false;}
        else if(rc==BLE_HS_ENOTCONN){
            ble->termination_retry=false;ble->forced_disconnected=true;
        }else if(ble->termination_attempts>=AMBIENT_BLE_TERMINATION_ATTEMPTS){
            ble->termination_retry=false;ble->termination_fault=true;
            ESP_LOGE(TAG,"terminal lifecycle fault=TERMINATE attempts=%u",ble->termination_attempts);
        }else{ESP_LOGW(TAG,"termination retry pending attempt=%u rc=%d",ble->termination_attempts,rc);}
    }
    if(ble->forced_incarnation && !owned){
        ble->termination_retry=false;ble->termination_fault=false;
        ble->forced_incarnation=0;ble->forced_disconnected=false;
        ble->forced_handle=BLE_HS_CONN_HANDLE_NONE;ble->forced_provisional=false;
    }else if(owned && ble->forced_disconnected && !ble->unpairing && !ble->link.connected){
        portENTER_CRITICAL(&ble->disconnect_mux);
        if(ble->forced_provisional && ble->provisional_serial==ble->forced_incarnation)
            ble->provisional_active=false;
        portEXIT_CRITICAL(&ble->disconnect_mux);
        ble->termination_retry=false;ble->termination_fault=false;
        resume=true;ble->forced_incarnation=0;ble->forced_disconnected=false;
        ble->forced_handle=BLE_HS_CONN_HANDLE_NONE;ble->forced_provisional=false;
    }
    if(changed || resume){
        ambient_ble_ring_reset(&ble->rx_ring);
        ambient_ble_tx_item_t discarded;
        /* The existing TX queue has exactly one slot. */
        (void)xQueueReceive(ble->tx_queue,&discarded,0);
    }
    after=ble->link;
    /* Serialize the restart with connection ownership. No second fallible state
     * lock may strand a callback after its slot has been consumed. */
    if(resume && !ble->shutting_down && !ble->unpairing && !ble->link.connected)
        ambient_ble_start_advertising_impl(ble);
    portENTER_CRITICAL(&ble->disconnect_mux);
    atomic_store(&ble->disconnect_pending,ble->pending_disconnect_event ||
                 ble->provisional_active || (ble->forced_incarnation && owned));
    portEXIT_CRITICAL(&ble->disconnect_mux);
    xSemaphoreGive(ble->state_mutex);
    if(changed)ambient_ble_notify_link_change(ble,before,after);
}

void ambient_ble_process_disconnect(ambient_ble_t *ble)
{ ambient_ble_process_disconnect_impl(ble,false); }
void ambient_ble_worker_disconnect(ambient_ble_t *ble)
{ ambient_ble_process_disconnect_impl(ble,true); }
