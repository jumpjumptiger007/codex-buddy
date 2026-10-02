/*
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 *
 * Notify queue mechanics adapted from Espressif esp-desktop-buddy S5 at
 * b6bac05db208717676e70180e5269d79f32b2d68; frames remain opaque bytes.
 */
#include "ambient_ble_internal.h"

#include <string.h>

static const char *TAG = "ambient_ble_tx";

static esp_err_t ambient_ble_tx_snapshot(ambient_ble_t *ble,
                                    uint64_t expected_incarnation,
                                    uint16_t expected_handle,
                                    uint16_t *mtu)
{
    ambient_ble_link_facts_t facts;

    esp_err_t err=ambient_ble_get_link_facts(ble, &facts);
    if(err!=ESP_OK)return err;
    if (!facts.connected || !facts.tx_notify_subscribed ||
        !facts.encrypted || !facts.authenticated || !facts.bonded ||
        !facts.secure_connections || !facts.peer_identity_valid ||
        !facts.peer_identity_accepted ||
        facts.link_incarnation != expected_incarnation ||
        facts.connection_handle != expected_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    *mtu = facts.att_mtu;
    return ESP_OK;
}

static esp_err_t ambient_ble_notify_frame(ambient_ble_t *ble,
                                          const ambient_ble_tx_item_t *item)
{
    size_t offset = 0;
    uint16_t mtu;

    while (offset < item->length) {
        size_t part_length;
        struct os_mbuf *om;
        int rc;

        esp_err_t snapshot=ambient_ble_tx_snapshot(ble,item->link_incarnation,
                                                  item->connection_handle,&mtu);
        if(snapshot!=ESP_OK)return snapshot;
        if(mtu<=3)return ESP_FAIL;
        part_length = mtu - 3U;
        if (part_length > item->length - offset) {
            part_length = item->length - offset;
        }
        om = ble_hs_mbuf_from_flat(item->bytes + offset,
                                   (uint16_t)part_length);
        if (om == NULL) {
            return ESP_ERR_NO_MEM;
        }
        rc = ble_gatts_notify_custom(item->connection_handle,
                                     ble->tx_value_handle, om);
        if (rc != 0) {
            /* NimBLE consumes om on both success and failure. */
            ESP_LOGW(TAG, "notify failed at byte %u rc=%d",
                     (unsigned)offset, rc);
            return ESP_FAIL;
        }
        offset += part_length;
    }
    return ESP_OK;
}

static void ambient_ble_recover_after_send_failure(
    ambient_ble_t *ble, const ambient_ble_tx_item_t *item)
{
    /* Publication checks the actual current incarnation even when the state
     * mutex is unavailable. Generic TX recovery never changes local unpair. */
    ambient_ble_request_disconnect(ble,item->connection_handle,item->link_incarnation);
}

static bool ambient_ble_approved_peer_matches(
    const ambient_ble_link_facts_t *facts,
    const ambient_ble_tx_item_t *item)
{
    return facts->connected && facts->encrypted && facts->authenticated &&
           facts->bonded && facts->secure_connections &&
           facts->peer_identity_valid &&
           facts->connection_handle == item->connection_handle &&
           facts->link_incarnation == item->link_incarnation &&
           facts->peer_identity_type == item->peer_identity_type &&
           memcmp(facts->peer_identity, item->peer_identity,
                  sizeof(item->peer_identity)) == 0;
}

static void ambient_ble_commit_peer_approval(ambient_ble_t *ble,
                                             const ambient_ble_tx_item_t *item)
{
    ambient_ble_link_facts_t facts;
    ambient_ble_link_facts_t before;
    ambient_ble_link_facts_t after;
    bool committed = false;

    if (ambient_ble_get_link_facts(ble, &facts) != ESP_OK ||
        !ambient_ble_approved_peer_matches(&facts, item) || ble->unpairing || ble->shutting_down || ble->disconnect_pending ||
        !ambient_ble_persist_accepted_peer(ble, item->peer_identity_type,
                                           item->peer_identity)) {
        ESP_LOGW(TAG, "locally approved peer trust could not be committed");
        ambient_ble_recover_after_send_failure(ble, item);
        return;
    }

    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        before = ble->link;
        if (!ble->unpairing && !ble->shutting_down && !ble->disconnect_pending &&
            ambient_ble_approved_peer_matches(&ble->link, item)) {
            ble->accepted_peer_identity_type = item->peer_identity_type;
            memcpy(ble->accepted_peer_identity, item->peer_identity,
                   sizeof(ble->accepted_peer_identity));
            ble->accepted_peer_valid = true;
            ble->link.peer_identity_accepted = true;
            after = ble->link;
            committed = true;
        }
        xSemaphoreGive(ble->state_mutex);
        if (committed) {
            ambient_ble_notify_link_change(ble, before, after);
            return;
        }
    }
    /* The durable approval remains bound to this identity. End this link so
     * it cannot proceed with stale in-memory authorization facts. */
    ambient_ble_recover_after_send_failure(ble, item);
}

void ambient_ble_tx_task(void *arg)
{
    ambient_ble_t *ble = arg;
    ambient_ble_tx_item_t item;

    while (ble != NULL && !ble->shutting_down) {
        ambient_ble_worker_disconnect(ble);
        if (xQueueReceive(ble->tx_queue, &item,
                          pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (ble->shutting_down) {
            break;
        }
        if (item.kind == AMBIENT_BLE_TX_ITEM_ACCEPT_PEER) {
            ambient_ble_commit_peer_approval(ble, &item);
        } else if (item.kind == AMBIENT_BLE_TX_ITEM_FRAME) {
            esp_err_t err = ambient_ble_notify_frame(ble, &item);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "queued frame discarded after link/send failure");
                if (err != ESP_ERR_INVALID_STATE) {
                    ambient_ble_recover_after_send_failure(ble, &item);
                }
            }
        } else {
            ESP_LOGW(TAG, "unknown TX work item discarded");
        }
    }
    if (ble != NULL && ble->tx_stopped != NULL) {
        xSemaphoreGive(ble->tx_stopped);
    }
    vTaskDelete(NULL);
}
