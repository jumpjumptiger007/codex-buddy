#include <assert.h>
#include <string.h>
#include "ambient_wire.h"
#include "ambient_protocol.h"
#include "ambient_fake_transport.h"

static ambient_wire_message_t hello(uint64_t generation)
{
    return (ambient_wire_message_t){.kind = AMBIENT_WIRE_HELLO, .version = 1,
        .generation = generation, .body.hello = {.offered = 7, .required = 7}};
}
static void init(ambient_wire_session_t *s, uint64_t generation)
{
    assert(ambient_wire_session_init(s, generation, 7, 7));
    ambient_wire_message_t h = hello(generation);
    assert(ambient_wire_negotiate(s, &h));
}
static void roundtrip(ambient_wire_message_t m)
{
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES]; size_t n = 42;
    ambient_wire_message_t decoded;
    assert(ambient_wire_encode(&m, frame, sizeof(frame), &n));
    const size_t expected[] = {0, 40, 95, 58, 41};
    assert(n == expected[m.kind] && frame[n - 1] == '\n');
    assert(ambient_wire_decode(frame, n - 1, &decoded));
    uint8_t again[sizeof(frame)]; size_t k;
    assert(ambient_wire_encode(&decoded, again, sizeof(again), &k));
    assert(k == n && !memcmp(frame, again, n));
    assert(!ambient_wire_encode(&m, frame, n - 1, &k) && !k);
    for (size_t i = 0; i < n - 1; i++) {
        assert(!ambient_wire_decode(frame, i, &decoded));
        uint8_t saved = frame[i]; frame[i] = 0xFF;
        assert(!ambient_wire_decode(frame, n - 1, &decoded)); frame[i] = saved;
    }
    frame[n - 1] = '|';
    assert(!ambient_wire_decode(frame, n, &decoded));
}
int main(void)
{
    ambient_wire_snapshot_t value = {.status = AMBIENT_STATUS_WORKING,
        .fresh = 8, .working = 8, .quota_present = 3,
        .short_used_basis_points = 10000, .long_used_basis_points = 1,
        .short_reset_marker = UINT64_MAX, .long_reset_marker = UINT64_MAX};
    ambient_wire_message_t m = hello(UINT64_MAX);
    roundtrip(m);
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_SNAPSHOT, .version = 1,
        .generation = UINT64_MAX, .body.snapshot = {.revision = UINT64_MAX, .value = value}};
    roundtrip(m);
    uint8_t encoded[AMBIENT_WIRE_MAX_FRAME_BYTES]; size_t n;
    assert(ambient_wire_encode(&m, encoded, sizeof(encoded), &n) && n == 95);
    m.body.snapshot.revision = 0; assert(!ambient_wire_valid(&m));
    m.body.snapshot.revision = 1; m.body.snapshot.value.fresh = 9;
    assert(!ambient_wire_valid(&m)); m.body.snapshot.value = value;
    m.body.snapshot.value.attention = 1; assert(!ambient_wire_valid(&m));
    m.body.snapshot.value = value; m.body.snapshot.value.short_used_basis_points = 10001;
    assert(!ambient_wire_valid(&m)); m.body.snapshot.value = value;
    m.body.snapshot.value.quota_present = 2; assert(!ambient_wire_valid(&m));
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_NOTICE, .version = 1,
        .generation = 1, .body.notice = {.id = UINT64_MAX, .snapshot_revision = 1,
                                        .code = AMBIENT_NOTICE_ERROR}};
    for (int code = 1; code <= 5; code++) { m.body.notice.code = code; roundtrip(m); }
    m.body.notice.code = 6; assert(!ambient_wire_valid(&m));
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_ACK, .version = 1,
        .generation = 1, .body.ack = {.target = AMBIENT_WIRE_SNAPSHOT, .id = UINT64_MAX}};
    roundtrip(m); m.body.ack.target = AMBIENT_WIRE_NOTICE; roundtrip(m);
    m.body.ack.target = AMBIENT_WIRE_HELLO; assert(!ambient_wire_valid(&m));
    static const char *forbidden[] = {"prompt", "transcript", "assistant", "tool_output",
        "shell_command", "diff", "authorization", "auth_token", "approval", "opaque"};
    ambient_wire_message_t unchanged = hello(1), output = unchanged;
    for (size_t i = 0; i < sizeof(forbidden)/sizeof(forbidden[0]); i++) {
        assert(!ambient_wire_decode((const uint8_t *)forbidden[i], strlen(forbidden[i]), &output));
        assert(!memcmp(&output, &unchanged, sizeof(output)));
        m = hello(1); assert(ambient_wire_encode(&m, encoded, sizeof(encoded), &n));
        memcpy(encoded + n - 1, forbidden[i], strlen(forbidden[i]));
        assert(!ambient_wire_decode(encoded, n - 1 + strlen(forbidden[i]), &output));
    }
    assert(!ambient_wire_decode(encoded, SIZE_MAX, &output));
    ambient_wire_session_t tx, rx, before;
    assert(!ambient_wire_session_init(&tx, 0, 7, 7));
    assert(!ambient_wire_session_init(&tx, 1, 7, 1));
    assert(ambient_wire_session_init(&tx, 1, 7, 7));
    m = hello(1); m.version = 2; assert(!ambient_wire_negotiate(&tx, &m));
    m = hello(2); assert(!ambient_wire_negotiate(&tx, &m));
    m = hello(1); m.body.hello.offered = 3; m.body.hello.required = 3;
    assert(!ambient_wire_negotiate(&tx, &m) && !tx.ready);
    m = hello(1); m.body.hello.offered |= 0x80000000U;
    m.body.hello.required |= 0x80000000U; assert(!ambient_wire_negotiate(&tx, &m));
    m.body.hello.required = 7; assert(ambient_wire_negotiate(&tx, &m) && tx.negotiated == 7);
    assert(!ambient_wire_negotiate(&tx, &m)); init(&rx, 1);
    uint8_t storage[AMBIENT_WIRE_QUEUE_BYTE_BUDGET], received[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t lengths[AMBIENT_WIRE_NOTICE_DEPTH], count;
    ambient_fake_transport_t fake; ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage), lengths,
        AMBIENT_WIRE_NOTICE_DEPTH, AMBIENT_WIRE_MAX_FRAME_BYTES,
        AMBIENT_WIRE_MAX_FRAME_BYTES, &transport));
    ambient_fake_transport_set_connected(&fake, false);
    assert(ambient_wire_send_snapshot(&tx, &value, &transport) == AMBIENT_TRANSPORT_DISCONNECTED);
    assert(tx.emitted_revision == 0);
    assert(!ambient_wire_queue_notice(&tx, AMBIENT_NOTICE_ATTENTION));
    ambient_fake_transport_set_connected(&fake, true);
    assert(ambient_wire_send_snapshot(&tx, &value, &transport) == AMBIENT_TRANSPORT_OK);
    assert(ambient_transport_receive(&transport, received, sizeof(received), &count) == AMBIENT_TRANSPORT_OK);
    assert(ambient_wire_decode(received, count - 1, &m));
    assert(ambient_wire_receive(&rx, &m)); assert(!ambient_wire_receive(&rx, &m));
    assert(ambient_wire_queue_notice(&tx, AMBIENT_NOTICE_ATTENTION));
    assert(ambient_wire_send_notice(&tx, &transport) == AMBIENT_TRANSPORT_INVALID);
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_ACK, .version = 1,
        .generation = 1, .body.ack = {.target = AMBIENT_WIRE_NOTICE, .id = 1}};
    assert(!ambient_wire_receive(&tx, &m));
    m.body.ack.target = AMBIENT_WIRE_SNAPSHOT; m.body.ack.id = 2;
    before = tx; assert(!ambient_wire_receive(&tx, &m)); assert(!memcmp(&tx, &before, sizeof(tx)));
    m.body.ack.id = 1; assert(ambient_wire_receive(&tx, &m)); assert(!ambient_wire_receive(&tx, &m));
    for (unsigned i = 1; i < AMBIENT_WIRE_NOTICE_DEPTH; i++) assert(ambient_wire_queue_notice(&tx, AMBIENT_NOTICE_ERROR));
    before = tx; assert(!ambient_wire_queue_notice(&tx, AMBIENT_NOTICE_ERROR));
    assert(!memcmp(&tx, &before, sizeof(tx)));
    assert(ambient_wire_send_notice(&tx, &transport) == AMBIENT_TRANSPORT_OK);
    assert(ambient_transport_receive(&transport, received, sizeof(received), &count) == AMBIENT_TRANSPORT_OK);
    assert(ambient_wire_decode(received, count - 1, &m));
    ambient_wire_snapshot_t truth = rx.received_snapshot;
    assert(ambient_wire_receive(&rx, &m)); assert(!ambient_wire_receive(&rx, &m));
    assert(!memcmp(&truth, &rx.received_snapshot, sizeof(truth)));
    m.body.notice.id = 3; /* missing notice2 is permitted, no persistent effect */
    assert(ambient_wire_receive(&rx, &m)); m.body.notice.id = 2;
    assert(!ambient_wire_receive(&rx, &m)); m.body.notice.id = 4;
    m.body.notice.snapshot_revision = 0; assert(!ambient_wire_receive(&rx, &m));
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_ACK, .version = 1,
        .generation = 1, .body.ack = {.target = AMBIENT_WIRE_NOTICE, .id = 2}};
    assert(!ambient_wire_receive(&tx, &m)); m.body.ack.id = 1;
    assert(ambient_wire_receive(&tx, &m) && tx.notice_count == 3);
    assert(!ambient_wire_receive(&tx, &m));
    /* Queuefull does not mark a snapshot emitted. */
    for (unsigned i = 0; i < AMBIENT_WIRE_NOTICE_DEPTH; i++)
        assert(ambient_transport_send(&transport, (const uint8_t *)"x", 1) == AMBIENT_TRANSPORT_OK);
    before = tx;
    assert(ambient_wire_send_snapshot(&tx, &value, &transport) == AMBIENT_TRANSPORT_WOULD_BLOCK);
    assert(!memcmp(&tx, &before, sizeof(tx)));
    for (unsigned i = 0; i < AMBIENT_WIRE_NOTICE_DEPTH; i++)
        assert(ambient_transport_receive(&transport, received, sizeof(received), &count) == AMBIENT_TRANSPORT_OK);
    assert(ambient_wire_send_snapshot(&tx, &value, &transport) == AMBIENT_TRANSPORT_OK);
    assert(tx.emitted_revision == 2 && !tx.notice_count);
    m.body.ack.target = AMBIENT_WIRE_SNAPSHOT; m.body.ack.id = 1;
    assert(!ambient_wire_receive(&tx, &m));
    /* Reconnect clears all truth/acks/notices until renegotiation and full snapshot. */
    assert(ambient_wire_session_init(&rx, 2, 7, 7));
    m = hello(2); assert(ambient_wire_negotiate(&rx, &m));
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_NOTICE, .version = 1,
        .generation = 2, .body.notice = {.id = 1, .snapshot_revision = 1, .code = 1}};
    assert(!ambient_wire_receive(&rx, &m));
    m = (ambient_wire_message_t){.kind = AMBIENT_WIRE_SNAPSHOT, .version = 1,
        .generation = 1, .body.snapshot = {.revision = 1, .value = value}};
    assert(!ambient_wire_receive(&rx, &m)); m.generation = 2;
    assert(ambient_wire_receive(&rx, &m));
    tx.emitted_revision = UINT64_MAX; assert(ambient_wire_send_snapshot(&tx, &value, &transport) == AMBIENT_TRANSPORT_INVALID);
    tx.next_notice_id = UINT64_MAX; assert(!ambient_wire_queue_notice(&tx, AMBIENT_NOTICE_ERROR));
    /* Split/coalesced framing, exact128 and oversized129 recovery; reset drops truncation. */
    char buffer[AMBIENT_WIRE_MAX_LINE_BYTES + 1]; ambient_line_framer_t framer;
    const char *line; size_t used, length;
    assert(ambient_line_framer_init(&framer, buffer, sizeof(buffer), AMBIENT_WIRE_MAX_LINE_BYTES));
    m = hello(1); assert(ambient_wire_encode(&m, encoded, sizeof(encoded), &n));
    for (size_t i = 0; i < n; i++) {
        ambient_frame_result_t r = ambient_line_framer_feed(&framer, encoded + i, 1,
            AMBIENT_WIRE_MAX_LINE_BYTES, &used, &line, &length);
        assert(r == (i + 1 == n ? AMBIENT_FRAME_READY : AMBIENT_FRAME_NEED_MORE));
    }
    assert(ambient_wire_decode((const uint8_t *)line, length, &output));
    assert(ambient_line_framer_feed(&framer, NULL, 0, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_NEED_MORE && used == 0);
    uint8_t oversized[AMBIENT_WIRE_MAX_LINE_BYTES + 2]; memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized)-1] = '\n';
    for (unsigned i = 0; i < 20; i++) {
        assert(ambient_line_framer_feed(&framer, oversized, sizeof(oversized), AMBIENT_WIRE_MAX_LINE_BYTES,
            &used, &line, &length) == AMBIENT_FRAME_TOO_LARGE);
        assert(ambient_line_framer_feed(&framer, encoded, n, AMBIENT_WIRE_MAX_LINE_BYTES,
            &used, &line, &length) == AMBIENT_FRAME_READY);
        assert(ambient_wire_decode((const uint8_t *)line, length, &output));
    }
    oversized[AMBIENT_WIRE_MAX_LINE_BYTES] = '\n';
    assert(ambient_line_framer_feed(&framer, oversized, AMBIENT_WIRE_MAX_LINE_BYTES + 1,
        AMBIENT_WIRE_MAX_LINE_BYTES, &used, &line, &length) == AMBIENT_FRAME_READY);
    assert(length == AMBIENT_WIRE_MAX_LINE_BYTES && !ambient_wire_decode((const uint8_t *)line, length, &output));
    assert(ambient_line_framer_feed(&framer, encoded, n - 1, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_NEED_MORE);
    ambient_line_framer_reset(&framer);
    assert(ambient_line_framer_feed(&framer, encoded, n, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_READY);
    uint8_t coalesced[2 * AMBIENT_WIRE_MAX_FRAME_BYTES];
    memcpy(coalesced, encoded, n); memcpy(coalesced + n, encoded, n);
    assert(ambient_line_framer_feed(&framer, coalesced, 2*n, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_READY && used == n);
    assert(ambient_wire_decode((const uint8_t *)line, length, &output));
    assert(ambient_line_framer_feed(&framer, coalesced + n, n, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_READY && used == n);
    assert(ambient_wire_decode((const uint8_t *)line, length, &output));
    framer.length = AMBIENT_WIRE_MAX_LINE_BYTES + 1;
    assert(ambient_line_framer_feed(&framer, encoded, n, AMBIENT_WIRE_MAX_LINE_BYTES,
        &used, &line, &length) == AMBIENT_FRAME_INVALID);
    ambient_line_framer_reset(&framer);
    assert(!ambient_line_framer_init(&framer, buffer, sizeof(buffer), SIZE_MAX));
    return 0;
}
