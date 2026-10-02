#include "ambient_session.h"

#include <string.h>

_Static_assert(AMBIENT_WIRE_MAX_FRAME_BYTES == 129U,
               "R3 may not increase the R1 frame maximum");
_Static_assert(AMBIENT_WIRE_MAX_LINE_BYTES == 128U,
               "R3 may not increase the R1 line maximum");
_Static_assert(sizeof(ambient_session_t) <= 1024U,
               "R3 receiver state must remain below one KiB");

bool ambient_session_link_authorized(const ambient_session_link_t *link)
{
    return link != NULL && link->connected && link->tx_notify_subscribed &&
           link->encrypted && link->mitm_authenticated && link->bonded &&
           link->secure_connections && link->peer_identity_valid &&
           link->known_peer_accepted && link->application_authenticated &&
           link->link_incarnation != 0 && link->auth_incarnation == link->link_incarnation;
}

void ambient_session_close(ambient_session_t *session)
{
    uint64_t last_generation;

    if (session == NULL) {
        return;
    }
    last_generation = session->last_generation;
    memset(session, 0, sizeof(*session));
    session->last_generation = last_generation;
}

bool ambient_session_open(ambient_session_t *session,
                          const ambient_session_link_t *link)
{
    if (session == NULL) return false;
    ambient_session_close(session);
    if (!ambient_session_link_authorized(link) ||
        !ambient_line_framer_init(&session->framer, session->line,
                                  sizeof(session->line), AMBIENT_WIRE_MAX_LINE_BYTES)) {
        return false;
    }
    session->link_incarnation = link->link_incarnation;
    session->active = true;
    return true; /* WAIT_HELLO: no generation allocation or outgoing traffic. */
}

ambient_transport_result_t ambient_session_flush_ack(
    ambient_session_t *session, const ambient_session_link_t *current_link,
    const ambient_transport_t *transport)
{
    ambient_wire_message_t ack;
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = 0;
    ambient_transport_result_t result;

    if (session == NULL || transport == NULL || !session->active ||
        session->generation == 0 || !ambient_session_link_authorized(current_link) ||
        current_link->link_incarnation != session->link_incarnation) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    if (!session->pending_ack) {
        return AMBIENT_TRANSPORT_OK;
    }
    ack = (ambient_wire_message_t){
        .kind = AMBIENT_WIRE_ACK,
        .version = AMBIENT_WIRE_VERSION,
        .generation = session->generation,
        .body.ack = {
            .target = (ambient_wire_kind_t)session->pending_ack_target,
            .id = session->pending_ack_id,
        },
    };
    if (!ambient_wire_encode(&ack, frame, sizeof(frame), &length)) {
        return AMBIENT_TRANSPORT_INVALID;
    }
    result = ambient_transport_send(transport, frame, length);
    if (result != AMBIENT_TRANSPORT_OK) {
        return result;
    }
    if (session->pending_ack_target == AMBIENT_WIRE_SNAPSHOT &&
        session->pending_ack_id == session->wire.received_revision) {
        session->snapshot_acknowledged = true;
    }
    session->pending_ack = false;
    session->pending_ack_target = 0;
    session->pending_ack_id = 0;
    return AMBIENT_TRANSPORT_OK;
}

static void ambient_session_clear_notices(ambient_session_t *session)
{
    memset(session->notices, 0, sizeof(session->notices));
    session->notice_head = 0;
    session->notice_count = 0;
}

static ambient_session_input_t ambient_session_accept_message(
    ambient_session_t *session,
    const ambient_session_link_t *current_link,
    const ambient_wire_message_t *message,
    const ambient_transport_t *transport)
{
    uint64_t received_revision;

    if (!session->wire.ready) {
        if (message->kind != AMBIENT_WIRE_HELLO || message->generation == 0 ||
            message->generation <= session->last_generation) {
            return AMBIENT_SESSION_INPUT_REJECTED;
        }
        ambient_wire_session_t next;
        if (!ambient_wire_session_init(&next, message->generation,
                AMBIENT_WIRE_CAP_KNOWN, AMBIENT_WIRE_CAP_REQUIRED) ||
            !ambient_wire_negotiate(&next, message)) {
            return AMBIENT_SESSION_INPUT_REJECTED;
        }
        ambient_wire_message_t reply = {
            .kind = AMBIENT_WIRE_HELLO, .version = AMBIENT_WIRE_VERSION,
            .generation = message->generation,
            .body.hello = {.offered = AMBIENT_WIRE_CAP_KNOWN,
                           .required = AMBIENT_WIRE_CAP_REQUIRED},
        };
        uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
        size_t length = 0;
        /* Consume the accepted ID even if reply transport fails. Reopen requires
         * a fresh Companion generation; never permit an ambiguous handshake. */
        session->last_generation = message->generation;
        if (!ambient_wire_encode(&reply, frame, sizeof(frame), &length) ||
            ambient_transport_send(transport, frame, length) != AMBIENT_TRANSPORT_OK) {
            ambient_session_close(session);
            return AMBIENT_SESSION_INPUT_REJECTED;
        }
        session->wire = next;
        session->generation = message->generation;
        return AMBIENT_SESSION_INPUT_HELLO;
    }
    if (message->generation != session->generation) {
        return AMBIENT_SESSION_INPUT_REJECTED;
    }
    received_revision = session->wire.received_revision;

    if (message->kind == AMBIENT_WIRE_SNAPSHOT) {
        uint64_t revision = message->body.snapshot.revision;

        if (revision <= received_revision) {
            if (revision == received_revision && !session->pending_ack) {
                session->pending_ack = true;
                session->pending_ack_target = AMBIENT_WIRE_SNAPSHOT;
                session->pending_ack_id = revision;
                (void)ambient_session_flush_ack(session, current_link, transport);
            }
            return AMBIENT_SESSION_INPUT_REJECTED;
        }
        if (!ambient_wire_receive(&session->wire, message)) {
            return AMBIENT_SESSION_INPUT_REJECTED;
        }
        session->snapshot_acknowledged = false;
        ambient_session_clear_notices(session);
        session->pending_ack = true;
        session->pending_ack_target = AMBIENT_WIRE_SNAPSHOT;
        session->pending_ack_id = revision;
        (void)ambient_session_flush_ack(session, current_link, transport);
        return AMBIENT_SESSION_INPUT_SNAPSHOT;
    }

    if (message->kind != AMBIENT_WIRE_NOTICE ||
        !session->snapshot_acknowledged || session->pending_ack ||
        session->notice_count == AMBIENT_WIRE_NOTICE_DEPTH) {
        return AMBIENT_SESSION_INPUT_REJECTED;
    }

    if (message->body.notice.id == session->wire.received_notice_id &&
        message->body.notice.snapshot_revision == session->wire.received_revision &&
        session->wire.received_notice_id != 0) {
        session->pending_ack = true;
        session->pending_ack_target = AMBIENT_WIRE_NOTICE;
        session->pending_ack_id = message->body.notice.id;
        (void)ambient_session_flush_ack(session, current_link, transport);
        return AMBIENT_SESSION_INPUT_REJECTED;
    }
    if (!ambient_wire_receive(&session->wire, message)) {
        return AMBIENT_SESSION_INPUT_REJECTED;
    }

    size_t tail = (session->notice_head + session->notice_count) %
                  AMBIENT_WIRE_NOTICE_DEPTH;
    session->notices[tail] = *message;
    session->notice_count++;
    session->pending_ack = true;
    session->pending_ack_target = AMBIENT_WIRE_NOTICE;
    session->pending_ack_id = message->body.notice.id;
    (void)ambient_session_flush_ack(session, current_link, transport);
    return AMBIENT_SESSION_INPUT_NOTICE;
}

ambient_session_feed_result_t ambient_session_feed(
    ambient_session_t *session,
    const ambient_session_link_t *current_link,
    const uint8_t *bytes,
    size_t byte_count,
    const ambient_transport_t *transport)
{
    ambient_session_feed_result_t result = {0};

    if (session == NULL || transport == NULL || !session->active ||
        !ambient_session_link_authorized(current_link) ||
        current_link->link_incarnation != session->link_incarnation ||
        (byte_count != 0 && bytes == NULL)) {
        if (session != NULL && current_link != NULL &&
            current_link->link_incarnation == session->link_incarnation &&
            !ambient_session_link_authorized(current_link)) {
            ambient_session_close(session);
        }
        result.rejected_frames = 1;
        return result;
    }
    if (session->pending_ack) {
        (void)ambient_session_flush_ack(session, current_link, transport);
    }

    size_t offset = 0;
    while (offset < byte_count) {
        size_t consumed = 0;
        size_t line_length = 0;
        const char *line = NULL;
        ambient_frame_result_t frame_result = ambient_line_framer_feed(
            &session->framer, bytes + offset, byte_count - offset,
            AMBIENT_WIRE_MAX_LINE_BYTES, &consumed, &line, &line_length);
        if (consumed > byte_count - offset) {
            result.rejected_frames++;
            break;
        }
        offset += consumed;
        if (frame_result == AMBIENT_FRAME_INVALID || consumed == 0) {
            result.rejected_frames++;
            ambient_line_framer_reset(&session->framer);
            break;
        }
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
        switch (ambient_session_accept_message(session, current_link, &message, transport)) {
        case AMBIENT_SESSION_INPUT_HELLO:
            result.accepted_hellos++;
            break;
        case AMBIENT_SESSION_INPUT_SNAPSHOT:
            result.accepted_snapshots++;
            break;
        case AMBIENT_SESSION_INPUT_NOTICE:
            result.accepted_notices++;
            break;
        default:
            result.rejected_frames++;
            break;
        }
        if (!session->active) break;
    }
    result.latest_snapshot_acknowledged = session->snapshot_acknowledged;
    return result;
}

bool ambient_session_snapshot(const ambient_session_t *session,
                              ambient_wire_snapshot_t *snapshot,
                              uint64_t *revision)
{
    if (session == NULL || snapshot == NULL || revision == NULL ||
        !session->active || !session->snapshot_acknowledged ||
        session->wire.received_revision == 0) {
        return false;
    }
    *snapshot = session->wire.received_snapshot;
    *revision = session->wire.received_revision;
    return true;
}

bool ambient_session_take_notice(ambient_session_t *session,
                                 ambient_wire_message_t *notice)
{
    if (session == NULL || notice == NULL || !session->active ||
        !session->snapshot_acknowledged || session->notice_count == 0) {
        return false;
    }
    *notice = session->notices[session->notice_head];
    memset(&session->notices[session->notice_head], 0,
           sizeof(session->notices[session->notice_head]));
    session->notice_head = (session->notice_head + 1U) %
                           AMBIENT_WIRE_NOTICE_DEPTH;
    session->notice_count--;
    return true;
}
