/*
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ambient_ble_internal.h"

#include <string.h>


static bool ambient_ble_nonzero_identity(const uint8_t identity[6])
{
    uint8_t any = 0;

    for (size_t i = 0; i < 6; ++i) {
        any |= identity[i];
    }
    return any != 0;
}

bool ambient_ble_load_accepted_peer(ambient_ble_t *ble)
{
    uint8_t record[7];
    size_t length = sizeof(record);
    esp_err_t err;

    if (ble == NULL || !ble->peer_nvs_open) {
        return false;
    }
    err = nvs_get_blob(ble->peer_nvs, "accepted_peer", record, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }
    if (err != ESP_OK || length != sizeof(record) ||
        !ambient_ble_nonzero_identity(&record[1])) {
        return false;
    }
    ble->accepted_peer_identity_type = record[0];
    memcpy(ble->accepted_peer_identity, &record[1], 6);
    ble->accepted_peer_valid = true;
    return true;
}

bool ambient_ble_persist_accepted_peer(ambient_ble_t *ble,
                                       uint8_t identity_type,
                                       const uint8_t identity[6])
{
    uint8_t record[7];
    bool persisted = false;

    if (ble == NULL || !ble->peer_nvs_open || identity == NULL ||
        !ambient_ble_nonzero_identity(identity)) {
        return false;
    }
    if (xSemaphoreTake(ble->peer_store_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }
    if (ble->unpairing || ble->shutting_down || ble->disconnect_pending) {
        xSemaphoreGive(ble->peer_store_mutex);
        return false;
    }
    record[0] = identity_type;
    memcpy(&record[1], identity, 6);
    persisted = nvs_set_blob(ble->peer_nvs, "accepted_peer", record,
                             sizeof(record)) == ESP_OK &&
                nvs_commit(ble->peer_nvs) == ESP_OK;
    xSemaphoreGive(ble->peer_store_mutex);
    return persisted;
}

bool ambient_ble_clear_accepted_peer(ambient_ble_t *ble)
{
    esp_err_t err;

    if (ble == NULL || !ble->peer_nvs_open) {
        return false;
    }
    if (xSemaphoreTake(ble->peer_store_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }
    err = nvs_erase_key(ble->peer_nvs, "accepted_peer");
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        xSemaphoreGive(ble->peer_store_mutex);
        return false;
    }
    bool cleared = nvs_commit(ble->peer_nvs) == ESP_OK;
    xSemaphoreGive(ble->peer_store_mutex);
    return cleared;
}
