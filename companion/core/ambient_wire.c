#include "ambient_wire.h"
#include <string.h>

_Static_assert(sizeof(ambient_wire_snapshot_t) <= 64, "retained snapshot host budget");
_Static_assert(sizeof(ambient_wire_message_t) <= 64, "semantic message host budget");
_Static_assert(sizeof(ambient_wire_session_t) <= 512, "protocol session host budget");

static bool snapshot_valid(const ambient_wire_snapshot_t *s)
{
    if (s->status < AMBIENT_STATUS_OFFLINE || s->status > AMBIENT_STATUS_DONE
        || s->fresh > AMBIENT_WIRE_MAX_SESSIONS || s->working > s->fresh
        || s->attention > s->fresh || s->done > s->fresh
        || (unsigned)s->working + s->attention + s->done > s->fresh
        || s->quota_present > 3 || s->short_used_basis_points > 10000
        || s->long_used_basis_points > 10000) return false;
    if (!(s->quota_present & 1) && (s->short_used_basis_points || s->short_reset_marker)) return false;
    if (!(s->quota_present & 2) && (s->long_used_basis_points || s->long_reset_marker)) return false;
    ambient_status_t expected = s->attention ? AMBIENT_STATUS_ATTENTION
        : s->working ? AMBIENT_STATUS_WORKING : s->done ? AMBIENT_STATUS_DONE
        : s->fresh ? AMBIENT_STATUS_IDLE : AMBIENT_STATUS_OFFLINE;
    return s->status == expected;
}
bool ambient_wire_valid(const ambient_wire_message_t *m)
{
    if (!m || m->version != AMBIENT_WIRE_VERSION || !m->generation) return false;
    switch (m->kind) {
    case AMBIENT_WIRE_HELLO:
        return !(m->body.hello.required & ~m->body.hello.offered);
    case AMBIENT_WIRE_SNAPSHOT:
        return m->body.snapshot.revision && snapshot_valid(&m->body.snapshot.value);
    case AMBIENT_WIRE_NOTICE:
        return m->body.notice.id && m->body.notice.snapshot_revision
            && m->body.notice.code >= AMBIENT_NOTICE_ATTENTION
            && m->body.notice.code <= AMBIENT_NOTICE_LONG_RESET;
    case AMBIENT_WIRE_ACK:
        return m->body.ack.id && (m->body.ack.target == AMBIENT_WIRE_SNAPSHOT
                                || m->body.ack.target == AMBIENT_WIRE_NOTICE);
    default: return false;
    }
}
/* A finite field list is shared by encoder and decoder. 15 fields maximum. */
static size_t fields(ambient_wire_message_t *m, uint64_t v[15], unsigned w[15], bool read)
{
    size_t n = 0;
#define FIELD(member, width) do { w[n] = width; if (read) m->member = v[n]; else v[n] = m->member; n++; } while (0)
    FIELD(kind, 1); FIELD(version, 2); FIELD(generation, 16);
    switch (m->kind) {
    case AMBIENT_WIRE_HELLO:
        FIELD(body.hello.offered, 8); FIELD(body.hello.required, 8); break;
    case AMBIENT_WIRE_SNAPSHOT:
        FIELD(body.snapshot.revision, 16);
        FIELD(body.snapshot.value.status, 1); FIELD(body.snapshot.value.fresh, 1);
        FIELD(body.snapshot.value.working, 1); FIELD(body.snapshot.value.attention, 1);
        FIELD(body.snapshot.value.done, 1); FIELD(body.snapshot.value.quota_present, 1);
        FIELD(body.snapshot.value.short_used_basis_points, 4);
        FIELD(body.snapshot.value.long_used_basis_points, 4);
        FIELD(body.snapshot.value.short_reset_marker, 16);
        FIELD(body.snapshot.value.long_reset_marker, 16); break;
    case AMBIENT_WIRE_NOTICE:
        FIELD(body.notice.id, 16); FIELD(body.notice.snapshot_revision, 16);
        FIELD(body.notice.code, 1); break;
    case AMBIENT_WIRE_ACK:
        FIELD(body.ack.target, 1); FIELD(body.ack.id, 16); break;
    default: return 0;
    }
#undef FIELD
    return n;
}
bool ambient_wire_encode(const ambient_wire_message_t *m, uint8_t *out,
                         size_t capacity, size_t *length)
{
    static const char hex[] = "0123456789ABCDEF";
    uint64_t v[15] = {0}; unsigned w[15] = {0};
    if (length) *length = 0;
    if (!out || !length || !ambient_wire_valid(m)) return false;
    ambient_wire_message_t copy = *m;
    size_t n = fields(&copy, v, w, false), needed = n;
    for (size_t i = 0; i < n; i++) needed += w[i];
    if (needed > AMBIENT_WIRE_MAX_FRAME_BYTES || capacity < needed) return false;
    size_t p = 0;
    for (size_t i = 0; i < n; i++) {
        if (i) out[p++] = '|';
        for (unsigned j = w[i]; j; j--) out[p++] = (uint8_t)hex[(v[i] >> (4 * (j - 1))) & 15];
    }
    out[p++] = '\n'; *length = p; return true;
}
bool ambient_wire_decode(const uint8_t *line, size_t length, ambient_wire_message_t *m)
{
    uint64_t v[15] = {0}; unsigned w[15] = {0};
    if (!line || !m || !length || length > AMBIENT_WIRE_MAX_LINE_BYTES
        || line[0] < '1' || line[0] > '4') return false;
    ambient_wire_message_t decoded = {.kind = (ambient_wire_kind_t)(line[0] - '0')};
    size_t n = fields(&decoded, v, w, false), p = 0;
    memset(v, 0, sizeof(v));
    for (size_t i = 0; i < n; i++) {
        if (i && (p >= length || line[p++] != '|')) return false;
        for (unsigned j = 0; j < w[i]; j++) {
            if (p >= length) return false;
            unsigned c = line[p++], digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return false;
            v[i] = (v[i] << 4) | digit;
        }
    }
    if (p != length) return false;
    (void)fields(&decoded, v, w, true);
    if (!ambient_wire_valid(&decoded)) return false;
    *m = decoded; return true;
}
bool ambient_wire_session_init(ambient_wire_session_t *s, uint64_t generation,
                               uint32_t offered, uint32_t required)
{
    if (!s || !generation || (required & ~offered)
        || (required & AMBIENT_WIRE_CAP_REQUIRED) != AMBIENT_WIRE_CAP_REQUIRED
        || (required & ~AMBIENT_WIRE_CAP_KNOWN)) return false;
    *s = (ambient_wire_session_t){.generation = generation, .offered = offered,
                                  .required = required}; return true;
}
bool ambient_wire_negotiate(ambient_wire_session_t *s, const ambient_wire_message_t *m)
{
    if (!s || s->ready || !ambient_wire_valid(m) || m->kind != AMBIENT_WIRE_HELLO
        || m->generation != s->generation || !s->generation) return false;
    uint32_t common = s->offered & m->body.hello.offered & AMBIENT_WIRE_CAP_KNOWN;
    if ((s->required & common) != s->required
        || (m->body.hello.required & common) != m->body.hello.required) return false;
    s->negotiated = common; s->ready = true; return true;
}
bool ambient_wire_receive(ambient_wire_session_t *s, const ambient_wire_message_t *m)
{
    if (!s || !s->ready || !ambient_wire_valid(m) || m->generation != s->generation) return false;
    switch (m->kind) {
    case AMBIENT_WIRE_SNAPSHOT:
        if (m->body.snapshot.revision <= s->received_revision) return false;
        s->received_snapshot = m->body.snapshot.value;
        s->received_revision = m->body.snapshot.revision; return true;
    case AMBIENT_WIRE_NOTICE:
        if (!s->received_revision || m->body.notice.snapshot_revision != s->received_revision
            || m->body.notice.id <= s->received_notice_id) return false;
        s->received_notice_id = m->body.notice.id; return true;
    case AMBIENT_WIRE_ACK:
        if (m->body.ack.target == AMBIENT_WIRE_SNAPSHOT) {
            if (!s->emitted_revision || m->body.ack.id != s->emitted_revision
                || m->body.ack.id <= s->acknowledged_revision) return false;
            s->acknowledged_revision = m->body.ack.id; return true;
        }
        /* Only the head notice actually sent (snapshot_revision != 0) is ackable. */
        if (!s->notice_count || !s->notices[0].body.notice.snapshot_revision
            || m->body.ack.id != s->notices[0].body.notice.id) return false;
        memmove(s->notices, s->notices + 1, (--s->notice_count) * sizeof(s->notices[0]));
        memset(&s->notices[s->notice_count], 0, sizeof(s->notices[0])); return true;
    default: return false;
    }
}
static ambient_transport_result_t send_message(const ambient_transport_t *t,
                                               const ambient_wire_message_t *m)
{
    uint8_t frame[AMBIENT_WIRE_MAX_FRAME_BYTES]; size_t length;
    if (!ambient_wire_encode(m, frame, sizeof(frame), &length)) return AMBIENT_TRANSPORT_INVALID;
    return ambient_transport_send(t, frame, length);
}
ambient_transport_result_t ambient_wire_send_snapshot(ambient_wire_session_t *s,
    const ambient_wire_snapshot_t *value, const ambient_transport_t *t)
{
    if (!s || !s->ready || !value || s->emitted_revision == UINT64_MAX) return AMBIENT_TRANSPORT_INVALID;
    ambient_wire_message_t m = {.kind = AMBIENT_WIRE_SNAPSHOT,
        .version = AMBIENT_WIRE_VERSION, .generation = s->generation,
        .body.snapshot = {.revision = s->emitted_revision + 1, .value = *value}};
    ambient_transport_result_t r = send_message(t, &m);
    if (r == AMBIENT_TRANSPORT_OK) {
        s->emitted_revision++;
        /* Superseding snapshot drops outstanding one-shot notices; truth survives. */
        s->notice_count = 0; memset(s->notices, 0, sizeof(s->notices));
    }
    return r;
}
bool ambient_wire_queue_notice(ambient_wire_session_t *s, ambient_wire_notice_code_t code)
{
    if (!s || !s->ready || !s->emitted_revision || s->notice_count >= AMBIENT_WIRE_NOTICE_DEPTH
        || s->next_notice_id == UINT64_MAX || code < AMBIENT_NOTICE_ATTENTION
        || code > AMBIENT_NOTICE_LONG_RESET) return false;
    s->notices[s->notice_count++] = (ambient_wire_message_t){.kind = AMBIENT_WIRE_NOTICE,
        .version = AMBIENT_WIRE_VERSION, .generation = s->generation,
        .body.notice = {.id = ++s->next_notice_id, .code = code}};
    return true;
}
ambient_transport_result_t ambient_wire_send_notice(ambient_wire_session_t *s,
                                                   const ambient_transport_t *t)
{
    if (!s || !s->ready || !s->notice_count || !s->emitted_revision
        || s->acknowledged_revision != s->emitted_revision) return AMBIENT_TRANSPORT_INVALID;
    ambient_wire_message_t m = s->notices[0];
    m.body.notice.snapshot_revision = s->emitted_revision;
    ambient_transport_result_t r = send_message(t, &m);
    if (r == AMBIENT_TRANSPORT_OK) s->notices[0] = m;
    return r;
}
