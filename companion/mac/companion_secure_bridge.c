#include "companion_secure_bridge.h"

#include <string.h>

bool companion_secure_link_authorized(const companion_secure_link_t *link)
{
    return link != NULL && link->connected && link->rx_notify_subscribed &&
           link->service_discovered && link->characteristics_discovered &&
           link->pinned_identity && link->application_authenticated &&
           link->link_incarnation != 0 && link->auth_incarnation == link->link_incarnation;
}

bool companion_secure_bridge_open(companion_secure_bridge_t *bridge,
                                  companion_runtime_t *runtime,
                                  const companion_secure_link_t *link,
                                  const ambient_transport_t *transport,
                                  const ambient_generation_backend_t *generation_backend)
{
    if (bridge == NULL || runtime == NULL || !runtime->running ||
        transport == NULL || !companion_secure_link_authorized(link)) {
        return false;
    }
    companion_secure_bridge_close(bridge);
    if (!ambient_line_framer_init(&bridge->framer, bridge->line,
                                  sizeof(bridge->line),
                                  AMBIENT_WIRE_MAX_LINE_BYTES)) {
        return false;
    }
    companion_runtime_link_lost(runtime);
    bridge->runtime = runtime;
    bridge->transport = *transport;
    bridge->link_incarnation = link->link_incarnation;
    uint64_t generation = 0;
    if (!ambient_generation_next(generation_backend, &generation) ||
        generation <= runtime->last_link_generation ||
        !ambient_wire_session_init(&bridge->handshake, generation,
            AMBIENT_WIRE_CAP_KNOWN, AMBIENT_WIRE_CAP_REQUIRED)) {
        companion_secure_bridge_close(bridge);
        return false;
    }
    bridge->generation = generation;
    ambient_wire_message_t hello = {
        .kind = AMBIENT_WIRE_HELLO, .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.hello = {.offered = AMBIENT_WIRE_CAP_KNOWN,
                       .required = AMBIENT_WIRE_CAP_REQUIRED},
    };
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = 0;
    if (!ambient_wire_encode(&hello, frame, sizeof(frame), &length) ||
        ambient_transport_send(transport, frame, length) != AMBIENT_TRANSPORT_OK) {
        companion_secure_bridge_close(bridge);
        return false;
    }
    bridge->active = true;
    return true;
}

void companion_secure_bridge_close(companion_secure_bridge_t *bridge)
{
    if (bridge == NULL) {
        return;
    }
    if (bridge->runtime != NULL) {
        companion_runtime_link_lost(bridge->runtime);
    }
    memset(bridge, 0, sizeof(*bridge));
}

static bool companion_secure_bridge_link_current(
    const companion_secure_bridge_t *bridge,
    const companion_secure_link_t *link)
{
    return bridge != NULL && bridge->active &&
           companion_secure_link_authorized(link) &&
           link->link_incarnation == bridge->link_incarnation;
}

ambient_transport_result_t companion_secure_bridge_pump(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link)
{
    if (!companion_secure_bridge_link_current(bridge, current_link) ||
        !bridge->negotiated || bridge->runtime == NULL) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    ambient_wire_session_t *wire = &bridge->runtime->wire;
    if (wire->notice_count == 0) {
        return AMBIENT_TRANSPORT_OK;
    }
    if (wire->emitted_revision == 0 ||
        wire->acknowledged_revision != wire->emitted_revision) {
        return AMBIENT_TRANSPORT_WOULD_BLOCK;
    }
    return ambient_wire_send_notice(wire, &bridge->transport);
}

ambient_transport_result_t companion_secure_bridge_publish(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link,
    uint64_t now_ms,
    int64_t now_unix)
{
    if (!companion_secure_bridge_link_current(bridge, current_link) ||
        !bridge->negotiated || bridge->runtime == NULL) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    return companion_runtime_publish(bridge->runtime, &bridge->transport,
                                     now_ms, now_unix);
}

static void companion_secure_bridge_accept_message(
    companion_secure_bridge_t *bridge,
    const ambient_wire_message_t *message,
    const companion_secure_link_t *current_link,
    uint64_t now_ms,
    int64_t now_unix,
    companion_secure_bridge_result_t *result)
{
    if (bridge->runtime == NULL || message->generation == 0) {
        result->rejected_frames++;
        return;
    }
    if (!bridge->negotiated) {
        if (message->kind != AMBIENT_WIRE_HELLO ||
            message->generation != bridge->generation ||
            !ambient_wire_negotiate(&bridge->handshake, message) ||
            !companion_runtime_link_ready(bridge->runtime,
                                          message->generation, message)) {
            result->rejected_frames++;
            return;
        }
        bridge->negotiated = true;
        result->accepted_hellos++;
        if (companion_runtime_publish(bridge->runtime, &bridge->transport,
                                      now_ms, now_unix) != AMBIENT_TRANSPORT_OK) {
            /* Keep the negotiated session so the caller can retry publish. */
            return;
        }
        return;
    }

    if (message->kind != AMBIENT_WIRE_ACK ||
        !ambient_wire_receive(&bridge->runtime->wire, message)) {
        result->rejected_frames++;
        return;
    }
    result->accepted_acks++;
    (void)companion_secure_bridge_pump(bridge, current_link);
}

companion_secure_bridge_result_t companion_secure_bridge_feed(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link,
    const uint8_t *bytes,
    size_t byte_count,
    uint64_t now_ms,
    int64_t now_unix)
{
    companion_secure_bridge_result_t result = {0};

    if (bridge == NULL || !bridge->active || bridge->runtime == NULL ||
        current_link == NULL ||
        (byte_count != 0 && bytes == NULL)) {
        result.rejected_frames = 1;
        return result;
    }
    if (current_link->link_incarnation != bridge->link_incarnation) {
        /* A delayed callback from an older connection must not close the new link. */
        result.rejected_frames = 1;
        return result;
    }
    if (!companion_secure_link_authorized(current_link)) {
        companion_secure_bridge_close(bridge);
        result.rejected_frames = 1;
        return result;
    }

    size_t offset = 0;
    while (offset < byte_count) {
        size_t consumed = 0;
        size_t line_length = 0;
        const char *line = NULL;
        ambient_frame_result_t frame_result = ambient_line_framer_feed(
            &bridge->framer, bytes + offset, byte_count - offset,
            AMBIENT_WIRE_MAX_LINE_BYTES, &consumed, &line, &line_length);
        if (consumed > byte_count - offset || consumed == 0 ||
            frame_result == AMBIENT_FRAME_INVALID) {
            result.rejected_frames++;
            ambient_line_framer_reset(&bridge->framer);
            break;
        }
        offset += consumed;
        if (frame_result == AMBIENT_FRAME_TOO_LARGE) {
            result.oversized_lines++;
            continue;
        }
        if (frame_result != AMBIENT_FRAME_READY) {
            continue;
        }

        ambient_wire_message_t message;
        if (!ambient_wire_decode((const uint8_t *)line, line_length, &message)) {
            result.rejected_frames++;
            continue;
        }
        companion_secure_bridge_accept_message(bridge, &message, current_link,
                                               now_ms,
                                               now_unix, &result);
    }
    return result;
}
