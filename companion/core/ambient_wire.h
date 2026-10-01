#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ambient_model.h"
#include "ambient_transport.h"

/* Project-owned v1: fixed-width uppercase ASCII hex, | separators, LF frame. */
#define AMBIENT_WIRE_VERSION 1U
#define AMBIENT_WIRE_CAP_SNAPSHOT 1U
#define AMBIENT_WIRE_CAP_NOTICE 2U
#define AMBIENT_WIRE_CAP_ACK 4U
#define AMBIENT_WIRE_CAP_KNOWN 7U
#define AMBIENT_WIRE_CAP_REQUIRED 7U
#define AMBIENT_WIRE_MAX_SESSIONS 8U
#define AMBIENT_WIRE_NOTICE_DEPTH 4U
#define AMBIENT_WIRE_MAX_LINE_BYTES 128U
#define AMBIENT_WIRE_MAX_FRAME_BYTES (AMBIENT_WIRE_MAX_LINE_BYTES + 1U)
#define AMBIENT_WIRE_QUEUE_BYTE_BUDGET (AMBIENT_WIRE_NOTICE_DEPTH * AMBIENT_WIRE_MAX_FRAME_BYTES)

typedef enum { AMBIENT_WIRE_HELLO = 1, AMBIENT_WIRE_SNAPSHOT,
    AMBIENT_WIRE_NOTICE, AMBIENT_WIRE_ACK } ambient_wire_kind_t;
typedef enum { AMBIENT_NOTICE_ATTENTION = 1, AMBIENT_NOTICE_COMPLETED,
    AMBIENT_NOTICE_ERROR, AMBIENT_NOTICE_SHORT_RESET,
    AMBIENT_NOTICE_LONG_RESET } ambient_wire_notice_code_t;
typedef struct {
    ambient_status_t status;
    uint8_t fresh, working, attention, done;
    /* bit0 short, bit1 long; absent windows have zero usage and marker. */
    uint8_t quota_present;
    uint16_t short_used_basis_points, long_used_basis_points;
    uint64_t short_reset_marker, long_reset_marker;
} ambient_wire_snapshot_t;
typedef struct {
    ambient_wire_kind_t kind;
    uint8_t version;
    uint64_t generation;
    union {
        struct { uint32_t offered, required; } hello;
        struct { uint64_t revision; ambient_wire_snapshot_t value; } snapshot;
        struct { uint64_t id, snapshot_revision; ambient_wire_notice_code_t code; } notice;
        struct { ambient_wire_kind_t target; uint64_t id; } ack;
    } body;
} ambient_wire_message_t;

/* No string, blob, extension, or generic payload exists in this model. */
bool ambient_wire_valid(const ambient_wire_message_t *message);
bool ambient_wire_encode(const ambient_wire_message_t *message,
                         uint8_t *frame, size_t capacity, size_t *length);
bool ambient_wire_decode(const uint8_t *line, size_t length,
                         ambient_wire_message_t *message);

/* Each secure transport session supplies a fresh nonzero generation. R3 owns
 * authenticated generation uniqueness. This host model does not authenticate. */
typedef struct {
    uint64_t generation, emitted_revision, acknowledged_revision;
    uint64_t received_revision, next_notice_id, received_notice_id;
    uint32_t offered, required, negotiated;
    bool ready;
    ambient_wire_snapshot_t received_snapshot;
    ambient_wire_message_t notices[AMBIENT_WIRE_NOTICE_DEPTH];
    size_t notice_count;
} ambient_wire_session_t;
bool ambient_wire_session_init(ambient_wire_session_t *session,
                               uint64_t generation, uint32_t offered,
                               uint32_t required);
bool ambient_wire_negotiate(ambient_wire_session_t *session,
                            const ambient_wire_message_t *hello);
/* Rejected input is atomic. Accepted notices never alter retained truth. */
bool ambient_wire_receive(ambient_wire_session_t *session,
                          const ambient_wire_message_t *message);
/* Successful send commits the revision; WOULD_BLOCK/errors leave it unchanged. */
ambient_transport_result_t ambient_wire_send_snapshot(
    ambient_wire_session_t *session, const ambient_wire_snapshot_t *snapshot,
    const ambient_transport_t *transport);
bool ambient_wire_queue_notice(ambient_wire_session_t *session,
                               ambient_wire_notice_code_t code);
ambient_transport_result_t ambient_wire_send_notice(
    ambient_wire_session_t *session, const ambient_transport_t *transport);
