#include "companion_r2_fixture.h"
#include "companion_secure_bridge.h"
#include "ambient_fake_transport.h"
#include "ambient_session.h"

#include <assert.h>
#include <string.h>

typedef struct { uint32_t epoch; bool fail_read, fail_write, fail_rng; } store_t;
static bool read_epoch(void *ctx, uint32_t *value)
{ store_t *s = ctx; if (s->fail_read) return false; *value = s->epoch; return true; }
static bool write_epoch(void *ctx, uint32_t value)
{ store_t *s = ctx; if (s->fail_write) return false; s->epoch = value; return true; }
static bool random_part(void *ctx, uint32_t *value)
{ store_t *s = ctx; if (s->fail_rng) return false; *value = 42; return true; }

static companion_secure_link_t authorized_link(uint64_t incarnation)
{
    return (companion_secure_link_t){
        .connected = true,
        .rx_notify_subscribed = true,
        .service_discovered=true, .characteristics_discovered=true,
        .pinned_identity=true, .application_authenticated=true, .auth_incarnation=incarnation,
        .link_incarnation = incarnation,
    };
}

static size_t encode(const ambient_wire_message_t *message, uint8_t *out)
{
    size_t length = 0;
    assert(ambient_wire_encode(message, out, AMBIENT_WIRE_MAX_FRAME_BYTES,
                               &length));
    return length;
}

static ambient_wire_message_t pop_message(const ambient_transport_t *transport)
{
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = 0;
    ambient_wire_message_t message;
    assert(ambient_transport_receive(transport, frame, sizeof(frame), &length) ==
           AMBIENT_TRANSPORT_OK);
    assert(length != 0 && frame[length - 1] == '\n');
    assert(ambient_wire_decode(frame, length - 1, &message));
    return message;
}

static companion_secure_bridge_result_t feed_message(
    companion_secure_bridge_t *bridge,
    const companion_secure_link_t *link,
    const ambient_wire_message_t *message,
    uint64_t now_ms)
{
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = encode(message, frame);
    return companion_secure_bridge_feed(bridge, link, frame, length,
                                        now_ms, 1);
}

static ambient_wire_message_t hello(uint64_t generation)
{
    return (ambient_wire_message_t){
        .kind = AMBIENT_WIRE_HELLO,
        .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.hello = {
            .offered = AMBIENT_WIRE_CAP_KNOWN,
            .required = AMBIENT_WIRE_CAP_REQUIRED,
        },
    };
}

static ambient_wire_message_t ack(uint64_t generation,
                                  ambient_wire_kind_t target,
                                  uint64_t id)
{
    return (ambient_wire_message_t){
        .kind = AMBIENT_WIRE_ACK,
        .version = AMBIENT_WIRE_VERSION,
        .generation = generation,
        .body.ack = {.target = target, .id = id},
    };
}

static void test_security_predicate_and_hello_snapshot_order(void)
{
    companion_secure_link_t link = authorized_link(7);
    assert(companion_secure_link_authorized(&link));
    companion_secure_link_t denied = link;
    bool *required[] = {
        &denied.connected, &denied.rx_notify_subscribed, &denied.service_discovered,
        &denied.characteristics_discovered, &denied.pinned_identity,
        &denied.application_authenticated,
    };
    for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
        *required[i] = false;
        assert(!companion_secure_link_authorized(&denied));
        denied = link;
    }
    denied.auth_incarnation++;
    assert(!companion_secure_link_authorized(&denied));
    denied=link;
    denied.link_incarnation = 0;
    assert(!companion_secure_link_authorized(&denied));

    store_t store = {0};
    ambient_generation_backend_t backend = {read_epoch, write_epoch, random_part, &store};
    companion_runtime_t runtime = {0};
    companion_runtime_lease_t lease = {0};
    companion_runtime_options_t options = r2_options(&lease, 1, NULL);
    assert(companion_runtime_start(&runtime, &options));
    uint8_t storage[AMBIENT_WIRE_MAX_FRAME_BYTES * 2];
    size_t lengths[2];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage), lengths,
        2, AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES,
        &transport));
    ambient_fake_transport_set_connected(&fake, true);

    companion_secure_bridge_t bridge = COMPANION_SECURE_BRIDGE_INITIALIZER;
    denied = link;
    denied.application_authenticated = false;
    assert(!companion_secure_bridge_open(&bridge, &runtime, &denied,
                                         &transport, &backend));
    assert(!runtime.wire.ready);
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    assert(runtime.wire.ready == false && bridge.active);

    ambient_wire_message_t passport_hello = hello(bridge.generation);
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = encode(&passport_hello, frame);
    companion_secure_bridge_result_t result;
    companion_secure_link_t failed_security = link;
    failed_security.characteristics_discovered = false;
    result = feed_message(&bridge, &failed_security, &passport_hello, 0);
    assert(result.rejected_frames == 1 && !bridge.active && !runtime.wire.ready);
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    passport_hello = hello(bridge.generation);
    length = encode(&passport_hello, frame);
    result = companion_secure_bridge_feed(
        &bridge, &link, frame, length / 2, 0, 1);
    assert(result.accepted_hellos == 0 && !bridge.negotiated);
    result = companion_secure_bridge_feed(&bridge, &link,
        frame + length / 2, length - length / 2, 0, 1);
    assert(result.accepted_hellos == 1 && result.rejected_frames == 0);
    ambient_wire_message_t sent = pop_message(&transport);
    assert(sent.kind == AMBIENT_WIRE_SNAPSHOT && sent.generation == bridge.generation &&
           sent.body.snapshot.revision == 1);

    result = feed_message(&bridge, &link, &passport_hello, 1);
    assert(result.rejected_frames == 1 && result.accepted_hellos == 0);
    ambient_wire_message_t incoming_ack =
        ack(bridge.generation, AMBIENT_WIRE_SNAPSHOT, 1);
    companion_secure_link_t stale_link = link;
    stale_link.link_incarnation++;
    result = feed_message(&bridge, &stale_link, &incoming_ack, 1);
    assert(result.rejected_frames == 1 && runtime.wire.acknowledged_revision == 0);
    incoming_ack = ack(bridge.generation + 1, AMBIENT_WIRE_SNAPSHOT, 1);
    result = feed_message(&bridge, &link, &incoming_ack, 1);
    assert(result.rejected_frames == 1 && runtime.wire.acknowledged_revision == 0);
    companion_secure_bridge_close(&bridge);
    assert(!runtime.wire.ready && !bridge.active);
    companion_runtime_stop(&runtime);
}

static void test_snapshot_ack_gates_notices_and_reconnect_resync(void)
{
    store_t store = {0};
    ambient_generation_backend_t backend = {read_epoch, write_epoch, random_part, &store};
    companion_runtime_t runtime = {0};
    companion_runtime_lease_t lease = {0};
    companion_runtime_options_t options = r2_options(&lease, 1, NULL);
    assert(companion_runtime_start(&runtime, &options));
    uint8_t storage[AMBIENT_WIRE_MAX_FRAME_BYTES * 2];
    size_t lengths[2];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage), lengths,
        2, AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES,
        &transport));
    ambient_fake_transport_set_connected(&fake, true);
    companion_secure_bridge_t bridge = COMPANION_SECURE_BRIDGE_INITIALIZER;
    companion_secure_link_t link = authorized_link(1);

    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    ambient_wire_message_t passport_hello = hello(bridge.generation);
    companion_secure_bridge_result_t result = feed_message(
        &bridge, &link, &passport_hello, 0);
    assert(result.accepted_hellos == 1);
    ambient_wire_message_t snapshot = pop_message(&transport);
    assert(snapshot.kind == AMBIENT_WIRE_SNAPSHOT &&
           snapshot.body.snapshot.revision == 1);

    assert(r2_apply(&runtime, "session", "turn", 1,
                    CODEX_HOOK_FACT_TURN_STARTED, 1) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&runtime, "session", NULL, 2,
                    CODEX_HOOK_FACT_ATTENTION_REQUIRED, 2) == AMBIENT_REDUCER_APPLIED);
    assert(runtime.pending_count == 1);
    assert(companion_secure_bridge_publish(&bridge, &link, 2, 1) ==
           AMBIENT_TRANSPORT_OK);
    snapshot = pop_message(&transport);
    assert(snapshot.kind == AMBIENT_WIRE_SNAPSHOT &&
           snapshot.body.snapshot.revision == 2 &&
           snapshot.body.snapshot.value.status == AMBIENT_STATUS_ATTENTION);
    assert(runtime.wire.notice_count == 1);
    assert(companion_secure_bridge_pump(&bridge, &link) == AMBIENT_TRANSPORT_WOULD_BLOCK);

    ambient_wire_message_t incoming_ack =
        ack(bridge.generation, AMBIENT_WIRE_SNAPSHOT, 2);
    result = feed_message(&bridge, &link, &incoming_ack, 3);
    assert(result.accepted_acks == 1 && result.rejected_frames == 0);
    ambient_wire_message_t notice = pop_message(&transport);
    assert(notice.kind == AMBIENT_WIRE_NOTICE &&
           notice.body.notice.snapshot_revision == 2);
    incoming_ack = ack(bridge.generation, AMBIENT_WIRE_NOTICE, notice.body.notice.id);
    result = feed_message(&bridge, &link, &incoming_ack, 4);
    assert(result.accepted_acks == 1 && runtime.wire.notice_count == 0);
    assert(fake.count == 0);

    /* Disconnect while source truth changes. New generation starts with a full snapshot. */
    companion_secure_bridge_close(&bridge);
    assert(r2_apply(&runtime, "session", NULL, 3,
                    CODEX_HOOK_FACT_ATTENTION_CLEARED, 5) == AMBIENT_REDUCER_APPLIED);
    assert(r2_apply(&runtime, "session", "turn", 4,
                    CODEX_HOOK_FACT_TURN_SUCCEEDED, 6) == AMBIENT_REDUCER_APPLIED);
    link = authorized_link(2);
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    passport_hello = hello(runtime.last_link_generation); /* Previous generation is stale. */
    result = feed_message(&bridge, &link, &passport_hello, 7);
    assert(result.rejected_frames == 1 && !runtime.wire.ready);
    companion_secure_bridge_close(&bridge);
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    passport_hello = hello(bridge.generation);
    result = feed_message(&bridge, &link, &passport_hello, 7);
    assert(result.accepted_hellos == 1);
    snapshot = pop_message(&transport);
    assert(snapshot.kind == AMBIENT_WIRE_SNAPSHOT && snapshot.body.snapshot.revision == 1 &&
           snapshot.body.snapshot.value.status == AMBIENT_STATUS_DONE);
    assert(runtime.wire.notice_count == 0); /* Link loss drops transient notices. */
    assert(companion_secure_bridge_pump(&bridge, &link) == AMBIENT_TRANSPORT_OK);
    assert(fake.count == 0);
    companion_secure_bridge_close(&bridge);
    companion_runtime_stop(&runtime);
}

static void test_backpressure_oversize_and_reconnect_generation(void)
{
    store_t store = {0};
    ambient_generation_backend_t backend = {read_epoch, write_epoch, random_part, &store};
    companion_runtime_t runtime = {0};
    companion_runtime_lease_t lease = {0};
    companion_runtime_options_t options = r2_options(&lease, 1, NULL);
    assert(companion_runtime_start(&runtime, &options));
    uint8_t storage[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t lengths[1];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage), lengths,
        1, AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES,
        &transport));
    ambient_fake_transport_set_connected(&fake, true);
    companion_secure_bridge_t bridge = COMPANION_SECURE_BRIDGE_INITIALIZER;
    companion_secure_link_t link = authorized_link(1);
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
    ambient_wire_message_t passport_hello = hello(bridge.generation);
    companion_secure_bridge_result_t result = feed_message(
        &bridge, &link, &passport_hello, 0);
    assert(result.accepted_hellos == 1 && fake.count == 1);
    uint64_t revision = runtime.wire.emitted_revision;
    assert(companion_secure_bridge_publish(&bridge, &link, 1, 1) ==
           AMBIENT_TRANSPORT_WOULD_BLOCK);
    assert(runtime.wire.emitted_revision == revision);
    (void)pop_message(&transport);
    assert(companion_secure_bridge_publish(&bridge, &link, 2, 1) ==
           AMBIENT_TRANSPORT_OK);
    (void)pop_message(&transport);

    uint8_t oversized[AMBIENT_WIRE_MAX_FRAME_BYTES + 1];
    memset(oversized, 'A', sizeof(oversized));
    oversized[sizeof(oversized) - 1] = '\n';
    result = companion_secure_bridge_feed(&bridge, &link, oversized,
        sizeof(oversized), 3, 1);
    assert(result.oversized_lines == 1 && result.rejected_frames == 0);
    ambient_wire_message_t incoming_ack =
        ack(bridge.generation, AMBIENT_WIRE_SNAPSHOT, 2);
    result = feed_message(&bridge, &link, &incoming_ack, 4);
    assert(result.accepted_acks == 1);
    result = feed_message(&bridge, &link, &incoming_ack, 5);
    assert(result.rejected_frames == 1);
    ambient_wire_message_t passport_notice = {
        .kind = AMBIENT_WIRE_NOTICE,
        .version = AMBIENT_WIRE_VERSION,
        .generation = bridge.generation,
        .body.notice = {.id = 1, .snapshot_revision = 1,
                        .code = AMBIENT_NOTICE_ATTENTION},
    };
    result = feed_message(&bridge, &link, &passport_notice, 6);
    assert(result.rejected_frames == 1);

    companion_secure_bridge_close(&bridge);
    for (uint64_t generation = 21; generation < 71; ++generation) {
        link = authorized_link(generation);
        assert(companion_secure_bridge_open(&bridge, &runtime, &link,
                                            &transport, &backend));
    assert(pop_message(&transport).kind == AMBIENT_WIRE_HELLO);
        ambient_wire_message_t passport_hello = hello(bridge.generation);
        result = feed_message(&bridge, &link, &passport_hello, generation);
        assert(result.accepted_hellos == 1);
        ambient_wire_message_t fresh = pop_message(&transport);
        assert(fresh.kind == AMBIENT_WIRE_SNAPSHOT &&
               fresh.generation == bridge.generation &&
               fresh.body.snapshot.revision == 1);
        companion_secure_bridge_close(&bridge);
        assert(!runtime.wire.ready);
    }
    companion_runtime_stop(&runtime);
}

static void test_authority_failures_restart_and_roundtrip(void)
{
    store_t store = {0};
    ambient_generation_backend_t backend = {read_epoch, write_epoch, random_part, &store};
    companion_runtime_t runtime = {0};
    companion_runtime_lease_t lease = {0};
    companion_runtime_options_t options = r2_options(&lease, 1, NULL);
    assert(companion_runtime_start(&runtime, &options));
    uint8_t storage[AMBIENT_WIRE_QUEUE_BYTE_BUDGET];
    size_t lengths[4];
    ambient_fake_transport_t fake;
    ambient_transport_t transport;
    assert(ambient_fake_transport_init(&fake, storage, sizeof(storage), lengths,
        4, AMBIENT_WIRE_MAX_FRAME_BYTES, AMBIENT_WIRE_MAX_FRAME_BYTES, &transport));
    ambient_fake_transport_set_connected(&fake, true);
    companion_secure_bridge_t bridge = COMPANION_SECURE_BRIDGE_INITIALIZER;
    companion_secure_link_t link = authorized_link(1);
    bool *failures[] = {&store.fail_read, &store.fail_write, &store.fail_rng};
    for (size_t i = 0; i < 3; ++i) {
        *failures[i] = true;
        assert(!companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
        assert(!bridge.active && !runtime.wire.ready && fake.count == 0);
        *failures[i] = false;
    }
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    ambient_wire_message_t request = pop_message(&transport);
    assert(request.kind == AMBIENT_WIRE_HELLO && !bridge.handshake.ready);
    assert(!runtime.wire.ready && !bridge.negotiated && fake.count == 0);
    assert(companion_secure_bridge_publish(&bridge, &link, 0, 1) == AMBIENT_TRANSPORT_INVALID);
    ambient_wire_message_t premature_ack = ack(request.generation, AMBIENT_WIRE_SNAPSHOT, 1);
    assert(feed_message(&bridge, &link, &premature_ack, 0).rejected_frames == 1);
    ambient_wire_message_t wrong_reply = hello(request.generation + 1);
    assert(feed_message(&bridge, &link, &wrong_reply, 0).rejected_frames == 1);
    assert(!bridge.negotiated && !runtime.wire.ready && fake.count == 0);
    wrong_reply = hello(request.generation);
    wrong_reply.body.hello.offered = 0;
    wrong_reply.body.hello.required = 0;
    assert(feed_message(&bridge, &link, &wrong_reply, 0).rejected_frames == 1);
    assert(!bridge.negotiated && !runtime.wire.ready && fake.count == 0);

    ambient_session_t passport = {0};
    ambient_session_link_t device_link = {
        .connected = true, .tx_notify_subscribed = true, .encrypted = true,
        .mitm_authenticated = true, .bonded = true, .secure_connections = true,
        .peer_identity_valid = true, .known_peer_accepted = true, .application_authenticated=true, .auth_incarnation=1, .link_incarnation = 1,
    };
    assert(ambient_session_open(&passport, &device_link));
    assert(passport.generation == 0 && fake.count == 0);
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES];
    size_t length = encode(&request, frame);
    assert(ambient_session_feed(&passport, &device_link, frame, length, &transport).accepted_hellos == 1);
    ambient_wire_message_t reply = pop_message(&transport);
    assert(reply.kind == AMBIENT_WIRE_HELLO && reply.generation == request.generation);
    assert(feed_message(&bridge, &link, &reply, 0).accepted_hellos == 1);
    ambient_wire_message_t full = pop_message(&transport);
    assert(full.kind == AMBIENT_WIRE_SNAPSHOT && full.generation == request.generation);
    length = encode(&full, frame);
    assert(ambient_session_feed(&passport, &device_link, frame, length, &transport).accepted_snapshots == 1);
    ambient_wire_message_t snapshot_ack = pop_message(&transport);
    assert(feed_message(&bridge, &link, &snapshot_ack, 0).accepted_acks == 1);
    assert(runtime.wire.acknowledged_revision == 1);
    uint64_t previous = bridge.generation;
    companion_secure_bridge_close(&bridge);
    ambient_session_close(&passport);
    companion_runtime_stop(&runtime);
    /* Simulated process restart keeps only the backend's durable state. */
    memset(&runtime, 0, sizeof(runtime));
    memset(&bridge, 0, sizeof(bridge));
    options = r2_options(&lease, 2, NULL);
    assert(companion_runtime_start(&runtime, &options));
    link.link_incarnation++;
    link.auth_incarnation=link.link_incarnation;
    assert(companion_secure_bridge_open(&bridge, &runtime, &link, &transport, &backend));
    assert(bridge.generation > previous);
    request = pop_message(&transport);
    companion_secure_link_t stale = link; stale.link_incarnation--;
    assert(feed_message(&bridge, &stale, &reply, 0).rejected_frames == 1);
    assert(bridge.active && !bridge.negotiated);
    assert(feed_message(&bridge, &link, &reply, 0).rejected_frames == 1);
    companion_secure_bridge_close(&bridge);
    companion_runtime_stop(&runtime);
}

int main(void)
{
    test_authority_failures_restart_and_roundtrip();
    test_security_predicate_and_hello_snapshot_order();
    test_snapshot_ack_gates_notices_and_reconnect_resync();
    test_backpressure_oversize_and_reconnect_generation();
    return 0;
}
