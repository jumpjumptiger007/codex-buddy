#include "ambient_r3_build_seam.h"

#include "ambient_ble.h"
#include "ambient_session.h"
#include "ambient_identity.h"
#include "ambient_authenticated_session.h"
#include "ambient_session_ble.h"

static esp_err_t (*volatile s_ble_start_reference)(
    const ambient_ble_config_t *config, ambient_ble_t **out_ble);
static bool (*volatile s_session_open_reference)(
    ambient_session_t *session,
    const ambient_session_link_t *link);
static ambient_session_feed_result_t (*volatile s_session_feed_reference)(
    ambient_session_t *session, const ambient_session_link_t *link,
    const uint8_t *bytes, size_t byte_count, const ambient_transport_t *transport);
static bool (*volatile s_session_transport_reference)(
    ambient_ble_t *ble, ambient_transport_t *transport);

static bool (*volatile s_identity_open)(ambient_identity_esp_t **);
static bool (*volatile s_identity_public)(ambient_identity_esp_t *,uint8_t[65]);
static ambient_auth_crypto_t (*volatile s_identity_crypto)(ambient_identity_esp_t *);
static bool (*volatile s_authenticated_open)(ambient_authenticated_session_t *,const ambient_session_link_t *,const uint8_t[65],bool,const ambient_auth_crypto_t *);
static ambient_transport_result_t (*volatile s_authenticated_flush)(ambient_authenticated_session_t *,const ambient_session_link_t *,const ambient_transport_t *);
static bool (*volatile s_authenticated_feed)(ambient_authenticated_session_t *, const ambient_session_link_t *,const uint8_t *,size_t,const ambient_transport_t *);
void ambient_r3_build_seam(void)
{
    /* Keep the transport/session implementation in the final app link so the
     * C3 firmware gate checks real symbol resolution. This never starts BLE. */
    s_identity_open=ambient_identity_esp_open;
    s_identity_public=ambient_identity_esp_public;
    s_identity_crypto=ambient_identity_esp_crypto;
    s_authenticated_open=ambient_authenticated_session_open;
    s_authenticated_flush=ambient_authenticated_session_flush;
    s_authenticated_feed=ambient_authenticated_session_feed;
    s_ble_start_reference = ambient_ble_start;
    s_session_open_reference = ambient_session_open;
    s_session_feed_reference = ambient_session_feed;
    s_session_transport_reference = ambient_session_ble_transport_init;
}
