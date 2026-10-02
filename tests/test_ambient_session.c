#include "ambient_session.h"
#include "ambient_fake_transport.h"

#include <assert.h>
#include <string.h>

static ambient_session_link_t authorized_link(uint64_t incarnation)
{
    return (ambient_session_link_t){
        .connected = true,
        .tx_notify_subscribed = true,
        .encrypted = true,
        .mitm_authenticated = true,
        .bonded = true,
        .secure_connections = true,
        .peer_identity_valid = true,
        .known_peer_accepted = true,
        .application_authenticated=true, .auth_incarnation=incarnation,
        .link_incarnation = incarnation,
    };
}

static ambient_wire_message_t snapshot(uint64_t generation,
                                      uint64_t revision,
                                      ambient_status_t status)
{
    ambient_wire_snapshot_t value = {.status = status, .fresh = 1};
    if (status == AMBIENT_STATUS_WORKING) value.working = 1;
    if (status == AMBIENT_STATUS_ATTENTION) value.attention = 1;
    return (ambient_wire_message_t){
        .kind = AMBIENT_WIRE_SNAPSHOT,
        .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.snapshot = {.revision = revision, .value = value},
    };
}

static ambient_wire_message_t notice(uint64_t generation,
                                     uint64_t id,
                                     uint64_t revision)
{
    return (ambient_wire_message_t){
        .kind = AMBIENT_WIRE_NOTICE,
        .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.notice = {
            .id = id,
            .snapshot_revision = revision,
            .code = AMBIENT_NOTICE_ATTENTION,
        },
    };
}

static size_t encode(const ambient_wire_message_t *message, uint8_t *frame)
{
    size_t length = 0;
    assert(ambient_wire_encode(message, frame, AMBIENT_WIRE_MAX_FRAME_BYTES,
                               &length));
    return length;
}

static ambient_wire_message_t pop_message(const ambient_transport_t *transport)
{
    uint8_t bytes[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = 0;
    ambient_wire_message_t message;
    assert(ambient_transport_receive(transport, bytes, sizeof(bytes), &length) ==
           AMBIENT_TRANSPORT_OK);
    assert(length != 0 && bytes[length - 1] == '\n');
    assert(ambient_wire_decode(bytes, length - 1, &message));
    return message;
}

static bool begin_session(ambient_session_t *session,
    const ambient_session_link_t *link, uint64_t generation,
    const ambient_transport_t *transport)
{
    assert(ambient_session_open(session, link));
    assert(session->generation == 0 && !session->wire.ready);
    ambient_wire_message_t early = snapshot(generation, 1, AMBIENT_STATUS_WORKING);
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = encode(&early, frame);
    assert(ambient_session_feed(session, link, frame, length, transport).rejected_frames == 1);
    ambient_wire_message_t hello = {
        .kind = AMBIENT_WIRE_HELLO, .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.hello = {.offered = AMBIENT_WIRE_CAP_KNOWN,
                       .required = AMBIENT_WIRE_CAP_REQUIRED},
    };
    ambient_wire_message_t incompatible = hello;
    incompatible.body.hello.offered = 0;
    incompatible.body.hello.required = 0;
    length = encode(&incompatible, frame);
    assert(ambient_session_feed(session, link, frame, length, transport).rejected_frames == 1);
    assert(session->generation == 0 && !session->wire.ready);
    length = encode(&hello, frame);
    assert(ambient_session_feed(session, link, frame, length, transport).accepted_hellos == 1);
    assert(ambient_session_feed(session, link, frame, length, transport).rejected_frames == 1);
    return session->wire.ready;
}

static void test_wait_hello_replay_and_reply_failure(void)
{
    uint8_t bytes[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t lengths[1];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, bytes, sizeof(bytes), lengths, 1,
        AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES, &transport));
    ambient_fake_transport_set_connected(&fake, true);
    ambient_session_t session = {0};
    ambient_session_link_t link = authorized_link(1);
    assert(ambient_session_open(&session, &link));
    assert(session.generation == 0 && fake.count == 0);
    ambient_wire_message_t hello = {
        .kind = AMBIENT_WIRE_HELLO, .version = AMBIENT_WIRE_VERSION,
        .generation = 10, .body.hello = {.offered = AMBIENT_WIRE_CAP_KNOWN,
                                       .required = AMBIENT_WIRE_CAP_REQUIRED},
    };
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES * 2];
    size_t length = encode(&hello, frame);
    assert(ambient_session_feed(&session, &link, frame, length, &transport).accepted_hellos == 1);
    (void)pop_message(&transport);
    ambient_session_close(&session);
    link.link_incarnation++;
    link.auth_incarnation=link.link_incarnation;
    assert(ambient_session_open(&session, &link));
    assert(ambient_session_feed(&session, &link, frame, length, &transport).rejected_frames == 1);
    assert(session.generation == 0 && !session.wire.ready && fake.count == 0);
    hello.generation++;
    length = encode(&hello, frame);
    const uint8_t filler = 'x';
    assert(ambient_transport_send(&transport, &filler, 1) == AMBIENT_TRANSPORT_OK);
    memcpy(frame + length, frame, length);
    ambient_session_feed_result_t result = ambient_session_feed(&session, &link,
        frame, length * 2, &transport);
    assert(result.rejected_frames == 1 && !session.active && !session.wire.ready);
    assert(session.last_generation == 11 && session.generation == 0);
}

int main(void)
{
    test_wait_hello_replay_and_reply_failure();
    uint8_t transport_bytes[AMBIENT_WIRE_QUEUE_BYTE_BUDGET];
    size_t transport_lengths[AMBIENT_WIRE_NOTICE_DEPTH];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, transport_bytes,
        sizeof(transport_bytes), transport_lengths, AMBIENT_WIRE_NOTICE_DEPTH,
        AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES,
        &transport));
    ambient_fake_transport_set_connected(&fake, true);

    ambient_session_link_t link = authorized_link(1);
    assert(ambient_session_link_authorized(&link));
    ambient_session_t session = {0};
    uint64_t companion_generation = 0;

    ambient_session_link_t denied = link;
    denied.known_peer_accepted = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.connected = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.tx_notify_subscribed = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.application_authenticated=false;
    assert(!ambient_session_open(&session,&denied));
    denied = link; denied.auth_incarnation++;
    assert(!ambient_session_open(&session,&denied));
    denied = link; denied.encrypted = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.mitm_authenticated = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.bonded = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.secure_connections = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.peer_identity_valid = false;
    assert(!ambient_session_link_authorized(&denied));
    denied = link; denied.link_incarnation = 0;
    assert(!ambient_session_link_authorized(&denied));
    assert(!ambient_session_open(&session, &denied));
    denied = link; denied.known_peer_accepted = false;
    assert(!ambient_session_open(&session, &denied));

    assert(begin_session(&session, &link, ++companion_generation, &transport));
    uint64_t first_generation = session.generation;
    ambient_wire_message_t sent = pop_message(&transport);
    assert(sent.kind == AMBIENT_WIRE_HELLO &&
           sent.generation == first_generation &&
           sent.body.hello.required == AMBIENT_WIRE_CAP_REQUIRED);

    ambient_wire_message_t incoming = snapshot(first_generation, 1,
                                               AMBIENT_STATUS_WORKING);
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = encode(&incoming, frame);
    ambient_session_feed_result_t feed = ambient_session_feed(
        &session, &(ambient_session_link_t){.link_incarnation = link.link_incarnation + 1}, frame, length, &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_revision == 0);

    incoming = notice(first_generation, 1, 1);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_notice_id == 0);

    incoming = snapshot(first_generation + UINT64_C(0x100000000), 1,
                        AMBIENT_STATUS_WORKING);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_revision == 0);

    incoming = snapshot(first_generation, 1, AMBIENT_STATUS_WORKING);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_snapshots == 1 && feed.latest_snapshot_acknowledged);
    ambient_wire_message_t ack = pop_message(&transport);
    assert(ack.kind == AMBIENT_WIRE_ACK &&
           ack.body.ack.target == AMBIENT_WIRE_SNAPSHOT && ack.body.ack.id == 1);
    ambient_wire_snapshot_t current;
    uint64_t revision = 0;
    assert(ambient_session_snapshot(&session, &current, &revision));
    assert(revision == 1 && current.status == AMBIENT_STATUS_WORKING);

    /* Duplicate snapshot is rejected without truth mutation and ACKed again. */
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_revision == 1);
    ack = pop_message(&transport);
    assert(ack.kind == AMBIENT_WIRE_ACK && ack.body.ack.id == 1);

    incoming = notice(first_generation, 1, 1);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_notices == 1 && session.wire.received_notice_id == 1);
    ack = pop_message(&transport);
    assert(ack.kind == AMBIENT_WIRE_ACK &&
           ack.body.ack.target == AMBIENT_WIRE_NOTICE && ack.body.ack.id == 1);
    ambient_wire_message_t delivered;
    assert(ambient_session_take_notice(&session, &delivered));
    assert(delivered.body.notice.id == 1);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 && session.notice_count == 0);
    ack = pop_message(&transport);
    assert(ack.kind == AMBIENT_WIRE_ACK && ack.body.ack.id == 1);

    /* A newer full snapshot replaces truth and invalidates queued old notices. */
    incoming = notice(first_generation, 2, 1);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_notices == 1);
    (void)pop_message(&transport);
    incoming = snapshot(first_generation, 2, AMBIENT_STATUS_ATTENTION);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_snapshots == 1 && feed.latest_snapshot_acknowledged);
    (void)pop_message(&transport);
    assert(session.notice_count == 0);
    assert(ambient_session_snapshot(&session, &current, &revision));
    assert(revision == 2 && current.status == AMBIENT_STATUS_ATTENTION);

    /* Four notices are bounded; a fifth does not mutate received_notice_id. */
    for (uint64_t id = 3; id < 3U + AMBIENT_WIRE_NOTICE_DEPTH; id++) {
        incoming = notice(first_generation, id, 2);
        length = encode(&incoming, frame);
        feed = ambient_session_feed(&session, &link, frame,
                                    length, &transport);
        assert(feed.accepted_notices == 1);
        (void)pop_message(&transport);
    }
    incoming = notice(first_generation, 3U + AMBIENT_WIRE_NOTICE_DEPTH, 2);
    length = encode(&incoming, frame);
    uint64_t last_notice_id = session.wire.received_notice_id;
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 &&
           session.wire.received_notice_id == last_notice_id);

    /* No mutation while the required snapshot ACK is blocked by a full TX queue. */
    ambient_session_close(&session);
    link.link_incarnation++;
    link.auth_incarnation=link.link_incarnation;
    assert(begin_session(&session, &link, ++companion_generation, &transport));
    uint64_t blocked_generation = session.generation;
    (void)pop_message(&transport);
    for (size_t i = 0; i < AMBIENT_WIRE_NOTICE_DEPTH; i++) {
        const uint8_t filler[] = {'x'};
        assert(ambient_transport_send(&transport, filler, sizeof(filler)) ==
               AMBIENT_TRANSPORT_OK);
    }
    incoming = snapshot(blocked_generation, 1, AMBIENT_STATUS_WORKING);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_snapshots == 1 &&
           !feed.latest_snapshot_acknowledged && session.pending_ack);
    assert(!ambient_session_snapshot(&session, &current, &revision));
    incoming = notice(blocked_generation, 1, 1);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_notice_id == 0);
    for (size_t i = 0; i < AMBIENT_WIRE_NOTICE_DEPTH; i++) {
        uint8_t filler[AMBIENT_WIRE_MAX_FRAME_BYTES];
        size_t filler_length = 0;
        assert(ambient_transport_receive(&transport, filler, sizeof(filler),
                                         &filler_length) == AMBIENT_TRANSPORT_OK);
        assert(filler_length == 1 && filler[0] == 'x');
    }
    assert(ambient_session_flush_ack(&session, &link, &transport) ==
           AMBIENT_TRANSPORT_OK);
    assert(ambient_session_snapshot(&session, &current, &revision));
    assert(pop_message(&transport).body.ack.target == AMBIENT_WIRE_SNAPSHOT);
    incoming = notice(blocked_generation, 1, 1);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_notices == 1);
    (void)pop_message(&transport);

    /* Malformed and oversized lines recover; incomplete data dies on disconnect. */
    uint8_t malformed[] = "X|not-an-r1-line\n";
    feed = ambient_session_feed(&session, &link, malformed,
                                sizeof(malformed) - 1U, &transport);
    assert(feed.rejected_frames == 1);
    uint8_t oversized[AMBIENT_WIRE_MAX_LINE_BYTES + 2U];
    memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1U] = '\n';
    feed = ambient_session_feed(&session, &link, oversized,
                                sizeof(oversized), &transport);
    assert(feed.oversized_lines == 1);
    incoming = snapshot(blocked_generation, 2, AMBIENT_STATUS_IDLE);
    length = encode(&incoming, frame);
    feed = ambient_session_feed(&session, &link, frame, length,
                                &transport);
    assert(feed.accepted_snapshots == 1);
    (void)pop_message(&transport);
    feed = ambient_session_feed(&session, &link, frame, 10,
                                &transport);
    assert(feed.accepted_snapshots == 0 && feed.rejected_frames == 0);
    ambient_session_close(&session);
    assert(!ambient_session_snapshot(&session, &current, &revision));
    assert(session.generation == 0 && session.last_generation != 0);
    link.link_incarnation++;
    link.auth_incarnation=link.link_incarnation;
    assert(begin_session(&session, &link, ++companion_generation, &transport));
    uint64_t final_generation = session.generation;
    assert(final_generation != blocked_generation &&
           companion_generation >= 3);
    (void)pop_message(&transport);
    feed = ambient_session_feed(&session, &link, frame + 10,
                                length - 10, &transport);
    assert(feed.rejected_frames == 1 && session.wire.received_revision == 0);

    /* Repeated reconnects consume distinct durable epochs without heap growth. */
    for (unsigned i = 0; i < 50; i++) {
        uint64_t prior = session.generation;
        ambient_session_close(&session);
        link.link_incarnation++;
    link.auth_incarnation=link.link_incarnation;
        assert(begin_session(&session, &link, ++companion_generation, &transport));
        assert(session.generation != prior && session.link_incarnation ==
               link.link_incarnation);
        (void)pop_message(&transport);
    }
    denied = link;
    denied.tx_notify_subscribed = false;
    feed = ambient_session_feed(&session, &denied, NULL, 0, &transport);
    assert(feed.rejected_frames == 1 && !session.active && session.generation == 0);
    ambient_session_close(&session);
    assert(!ambient_session_take_notice(&session, &delivered));
    return 0;
}
