#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_protocol.h"
#include "ambient_transport.h"
#include "ambient_wire.h"

typedef struct {
    bool connected;
    bool tx_notify_subscribed;
    bool encrypted;
    bool mitm_authenticated;
    bool bonded;
    bool secure_connections;
    bool peer_identity_valid;
    bool known_peer_accepted;
    bool application_authenticated;
    uint64_t auth_incarnation;
    uint64_t link_incarnation;
} ambient_session_link_t;

typedef enum {
    AMBIENT_SESSION_INPUT_INVALID = 0,
    AMBIENT_SESSION_INPUT_REJECTED,
    AMBIENT_SESSION_INPUT_HELLO,
    AMBIENT_SESSION_INPUT_SNAPSHOT,
    AMBIENT_SESSION_INPUT_NOTICE,
    AMBIENT_SESSION_INPUT_OVERSIZED,
} ambient_session_input_t;

typedef struct {
    size_t accepted_hellos;
    size_t accepted_snapshots;
    size_t accepted_notices;
    size_t rejected_frames;
    size_t oversized_lines;
    bool latest_snapshot_acknowledged;
} ambient_session_feed_result_t;

typedef struct {
    bool active;
    bool snapshot_acknowledged;
    bool pending_ack;
    uint8_t pending_ack_target;
    uint64_t pending_ack_id;
    uint64_t generation;
    uint64_t last_generation;
    uint64_t link_incarnation;
    ambient_wire_session_t wire;
    ambient_line_framer_t framer;
    char line[AMBIENT_WIRE_MAX_LINE_BYTES + 1U];
    ambient_wire_message_t notices[AMBIENT_WIRE_NOTICE_DEPTH];
    size_t notice_head;
    size_t notice_count;
} ambient_session_t;

/* R3's fixed product predicate: all observations below are mandatory. */
bool ambient_session_link_authorized(const ambient_session_link_t *link);

/* Zero-initialize before first use. Open enters WAIT_HELLO on an authorized
 * link without sending traffic. Caller serializes open/feed/close and reports
 * current security facts on every operation. Replay history is process-local. */
bool ambient_session_open(ambient_session_t *session,
                          const ambient_session_link_t *link);
void ambient_session_close(ambient_session_t *session);

ambient_session_feed_result_t ambient_session_feed(
    ambient_session_t *session,
    const ambient_session_link_t *current_link,
    const uint8_t *bytes,
    size_t byte_count,
    const ambient_transport_t *transport);

/* Retry at most one pending ACK per call; no internal retry loop. */
ambient_transport_result_t ambient_session_flush_ack(
    ambient_session_t *session, const ambient_session_link_t *current_link,
    const ambient_transport_t *transport);

bool ambient_session_snapshot(const ambient_session_t *session,
                              ambient_wire_snapshot_t *snapshot,
                              uint64_t *revision);
bool ambient_session_take_notice(ambient_session_t *session,
                                 ambient_wire_message_t *notice);
