#include "ambient_session_ble.h"

#include "esp_err.h"

static bool ble_connected(void *context)
{
    ambient_ble_link_facts_t facts;
    ambient_ble_t *ble = context;

    return ambient_ble_get_link_facts(ble, &facts) == ESP_OK && facts.connected &&
           facts.tx_notify_subscribed && facts.encrypted && facts.authenticated &&
           facts.bonded && facts.secure_connections && facts.peer_identity_valid &&
           facts.peer_identity_accepted && facts.link_incarnation != 0;
}

static ambient_transport_result_t ble_send(void *context,
                                           const uint8_t *bytes,
                                           size_t length)
{
    switch (ambient_ble_send(context, bytes, length)) {
    case ESP_OK: return AMBIENT_TRANSPORT_OK;
    case ESP_ERR_TIMEOUT: return AMBIENT_TRANSPORT_WOULD_BLOCK;
    case ESP_ERR_INVALID_STATE: return AMBIENT_TRANSPORT_DISCONNECTED;
    case ESP_ERR_INVALID_SIZE: return AMBIENT_TRANSPORT_TOO_LARGE;
    default: return AMBIENT_TRANSPORT_INVALID;
    }
}

static ambient_transport_result_t ble_receive(void *context,
                                              uint8_t *buffer,
                                              size_t capacity,
                                              size_t *received)
{
    esp_err_t err = ambient_ble_read(context, buffer, capacity, received);

    if (err == ESP_OK) return AMBIENT_TRANSPORT_OK;
    if (err == ESP_ERR_INVALID_STATE) return AMBIENT_TRANSPORT_DISCONNECTED;
    if (err == ESP_ERR_TIMEOUT) return AMBIENT_TRANSPORT_WOULD_BLOCK;
    return AMBIENT_TRANSPORT_INVALID;
}

static const ambient_transport_ops_t BLE_TRANSPORT_OPS = {
    .is_connected = ble_connected,
    .send = ble_send,
    .receive = ble_receive,
};

bool ambient_session_ble_transport_init(ambient_ble_t *ble,
                                        ambient_transport_t *transport)
{
    if (ble == NULL || transport == NULL) {
        return false;
    }
    transport->ops = &BLE_TRANSPORT_OPS;
    transport->context = ble;
    transport->max_message_bytes = AMBIENT_WIRE_MAX_FRAME_BYTES;
    return true;
}
