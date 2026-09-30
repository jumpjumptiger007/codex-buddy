#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ambient_model.h"

typedef enum {
    AMBIENT_MESSAGE_INVALID = 0,
    AMBIENT_MESSAGE_CONTROL,
    AMBIENT_MESSAGE_EVENT,
} ambient_message_kind_t;

/*
 * A semantic envelope for host-side validation. CONTROL payload bytes remain
 * opaque until the product protocol is specified; EVENT carries normalized,
 * content-free state. The view is borrowed only for the validation call.
 */
typedef struct {
    ambient_message_kind_t kind;
    const uint8_t *control_payload;
    size_t control_payload_length;
    const ambient_event_t *event;
} ambient_protocol_message_t;

typedef enum {
    AMBIENT_PROTOCOL_VALID = 0,
    AMBIENT_PROTOCOL_INVALID,
    AMBIENT_PROTOCOL_TOO_LARGE,
} ambient_protocol_result_t;

ambient_protocol_result_t ambient_protocol_validate_message(
    const ambient_protocol_message_t *message,
    size_t max_control_payload_bytes);

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    size_t max_line_bytes;
    bool discarding_oversize_line;
} ambient_line_framer_t;

typedef enum {
    AMBIENT_FRAME_INVALID = 0,
    AMBIENT_FRAME_NEED_MORE,
    AMBIENT_FRAME_READY,
    AMBIENT_FRAME_TOO_LARGE,
} ambient_frame_result_t;

/* The caller owns buffer, sized max_line_bytes + 1, and supplies the limit. */
bool ambient_line_framer_init(ambient_line_framer_t *framer,
                              char *buffer,
                              size_t capacity,
                              size_t max_line_bytes);

/* Consumes bytes through the first LF. A ready line excludes its LF. */
ambient_frame_result_t ambient_line_framer_feed(
    ambient_line_framer_t *framer,
    const uint8_t *bytes,
    size_t byte_count,
    size_t max_line_bytes,
    size_t *consumed,
    const char **line,
    size_t *line_length);

void ambient_line_framer_reset(ambient_line_framer_t *framer);
