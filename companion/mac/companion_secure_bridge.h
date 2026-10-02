#pragma once

#include "companion_runtime.h"
#include "ambient_generation.h"

typedef struct {
    bool connected;
    bool rx_notify_subscribed;
    bool service_discovered;
    bool characteristics_discovered;
    bool pinned_identity;
    bool application_authenticated;
    uint64_t auth_incarnation;
    uint64_t link_incarnation;
} companion_secure_link_t;

typedef struct {
    bool active;
    bool negotiated;
    uint64_t link_incarnation;
    uint64_t generation;
    ambient_wire_session_t handshake;
    companion_runtime_t *runtime;
    ambient_transport_t transport;
    ambient_line_framer_t framer;
    char line[AMBIENT_WIRE_MAX_LINE_BYTES + 1U];
} companion_secure_bridge_t;

#define COMPANION_SECURE_BRIDGE_INITIALIZER {0}

typedef struct {
    size_t accepted_hellos;
    size_t accepted_acks;
    size_t rejected_frames;
    size_t oversized_lines;
} companion_secure_bridge_result_t;

/* Host-observable transport ownership plus pinned fresh Passport authentication. */
bool companion_secure_link_authorized(const companion_secure_link_t *link);
/* Initialize bridge with COMPANION_SECURE_BRIDGE_INITIALIZER before first use. */
bool companion_secure_bridge_open(companion_secure_bridge_t *bridge,
                                  companion_runtime_t *runtime,
                                  const companion_secure_link_t *link,
                                  const ambient_transport_t *transport,
                                  const ambient_generation_backend_t *generation_backend);
void companion_secure_bridge_close(companion_secure_bridge_t *bridge);

/* Open allocates a durable generation and emits Companion HELLO. First inbound
 * record must be a matching Passport HELLO reply; that handshake immediately
 * publishes a full snapshot. Later input is limited to R1 ACKs. */
companion_secure_bridge_result_t companion_secure_bridge_feed(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link,
    const uint8_t *bytes,
    size_t byte_count,
    uint64_t now_ms,
    int64_t now_unix);

ambient_transport_result_t companion_secure_bridge_publish(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link,
    uint64_t now_ms,
    int64_t now_unix);
/* Sends at most one notice, and only after the latest snapshot ACK. */
ambient_transport_result_t companion_secure_bridge_pump(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *current_link);
