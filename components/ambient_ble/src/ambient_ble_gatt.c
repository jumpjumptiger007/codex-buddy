/*
 * SPDX-FileCopyrightText: 2026 FoloToy
 * SPDX-License-Identifier: Apache-2.0
 *
 * GATT registration and bounded byte callbacks adapted from Espressif
 * esp-desktop-buddy S5 at b6bac05db208717676e70180e5269d79f32b2d68.
 */
#include "ambient_ble_internal.h"

#include <string.h>

/* UUIDv5 namespace: https://yliu.tech/ai-passport/ambient-ble/v1/ */
static const ble_uuid128_t AMBIENT_SERVICE_UUID =
    BLE_UUID128_INIT(0x81, 0x38, 0xdb, 0xb3, 0x0d, 0xc9, 0xc7, 0xb7,
                     0xce, 0x53, 0x32, 0x07, 0x62, 0xbf, 0x13, 0x09);
static const ble_uuid128_t AMBIENT_RX_UUID =
    BLE_UUID128_INIT(0x11, 0x1c, 0x6e, 0x4a, 0xad, 0xd4, 0x19, 0xa2,
                     0xbb, 0x53, 0x29, 0x4a, 0xeb, 0xa8, 0x9d, 0x3b);
static const ble_uuid128_t AMBIENT_TX_UUID =
    BLE_UUID128_INIT(0xaf, 0x27, 0x8c, 0x8a, 0xf5, 0x41, 0x8a, 0x8e,
                     0x74, 0x5b, 0x65, 0xfe, 0xbc, 0x7e, 0x12, 0x90);

static bool ambient_ble_secure_link(const ambient_ble_link_facts_t *facts)
{
    return facts->connected && facts->tx_notify_subscribed &&
           facts->encrypted && facts->authenticated &&
           facts->bonded && facts->secure_connections &&
           facts->peer_identity_valid && facts->peer_identity_accepted;
}

static int ambient_ble_gatt_rx_access(uint16_t conn_handle,
                                      uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt,
                                      void *arg)
{
    ambient_ble_t *ble = arg;
    ambient_ble_link_facts_t facts;
    uint8_t flat[AMBIENT_BLE_ATT_WRITE_PAYLOAD_MAX];
    uint16_t length;
    int rc;

    (void)attr_handle;
    if (ble == NULL || ctxt == NULL || ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR ||
        (ble->shutting_down || ble->unpairing || ble->disconnect_pending)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    length = OS_MBUF_PKTLEN(ctxt->om);
    if (length == 0 || length > sizeof(flat)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (ambient_ble_get_link_facts(ble, &facts) != ESP_OK ||
        !ambient_ble_secure_link(&facts) ||
        facts.connection_handle != conn_handle ||
        facts.link_incarnation == 0) {
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    rc = ble_hs_mbuf_to_flat(ctxt->om, flat, sizeof(flat), &length);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (xSemaphoreTake(ble->state_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    /* Recheck incarnation after flattening to reject a raced disconnect. */
    if (ble->unpairing || ble->shutting_down || ble->disconnect_pending || !ambient_ble_secure_link(&ble->link) ||
        ble->link.connection_handle != conn_handle ||
        ble->link.link_incarnation != facts.link_incarnation) {
        xSemaphoreGive(ble->state_mutex);
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    if (!ambient_ble_ring_write(&ble->rx_ring, flat, length)) {
        xSemaphoreGive(ble->state_mutex);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ble->rx_ring.length > ble->rx_high_water) ble->rx_high_water = ble->rx_ring.length;
    xSemaphoreGive(ble->state_mutex);
    if (ble->config.rx_available != NULL) {
        ble->config.rx_available(ble->config.context);
    }
    return 0;
}

static int ambient_ble_gatt_tx_access(uint16_t conn_handle,
                                      uint16_t attr_handle,
                                      struct ble_gatt_access_ctxt *ctxt,
                                      void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)ctxt;
    (void)arg;
    return BLE_ATT_ERR_UNLIKELY;
}

void ambient_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                  void *arg)
{
    ambient_ble_t *ble = arg;

    if (ctxt == NULL || ble == NULL) {
        return;
    }
    if (ctxt->op == BLE_GATT_REGISTER_OP_CHR &&
        ctxt->chr.chr_def->uuid == &AMBIENT_TX_UUID.u) {
        ble->tx_value_handle = ctxt->chr.val_handle;
    }
}

void ambient_ble_prepare_gatt(ambient_ble_t *ble)
{
    memset(ble->gatt_chars, 0, sizeof(ble->gatt_chars));
    memset(ble->gatt_svcs, 0, sizeof(ble->gatt_svcs));

    ble->gatt_chars[0].uuid = &AMBIENT_RX_UUID.u;
    ble->gatt_chars[0].access_cb = ambient_ble_gatt_rx_access;
    ble->gatt_chars[0].arg = ble;
    ble->gatt_chars[0].flags = BLE_GATT_CHR_F_WRITE |
                               BLE_GATT_CHR_F_WRITE_NO_RSP |
                               BLE_GATT_CHR_F_WRITE_ENC |
                               BLE_GATT_CHR_F_WRITE_AUTHEN;

    ble->gatt_chars[1].uuid = &AMBIENT_TX_UUID.u;
    ble->gatt_chars[1].access_cb = ambient_ble_gatt_tx_access;
    ble->gatt_chars[1].arg = ble;
    ble->gatt_chars[1].flags = BLE_GATT_CHR_F_NOTIFY;
    ble->gatt_chars[1].val_handle = &ble->tx_value_handle;

    ble->gatt_svcs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    ble->gatt_svcs[0].uuid = &AMBIENT_SERVICE_UUID.u;
    ble->gatt_svcs[0].characteristics = ble->gatt_chars;
}
