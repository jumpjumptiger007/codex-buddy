#include "ambient_protocol.h"

#include <string.h>

static bool ambient_event_is_valid(const ambient_event_t *event)
{
    bool turn_event;

    if (!event || event->kind <= AMBIENT_EVENT_INVALID
        || event->kind > AMBIENT_EVENT_ATTENTION_CLEARED
        || event->session_key == 0 || event->event_key == 0) {
        return false;
    }

    turn_event = event->kind == AMBIENT_EVENT_TURN_STARTED
        || event->kind == AMBIENT_EVENT_TURN_SUCCEEDED
        || event->kind == AMBIENT_EVENT_TURN_FAILED
        || event->kind == AMBIENT_EVENT_TURN_ABORTED;
    return turn_event ? event->turn_key != 0 : event->turn_key == 0;
}

ambient_protocol_result_t ambient_protocol_validate_message(
    const ambient_protocol_message_t *message,
    size_t max_control_payload_bytes)
{
    if (!message) {
        return AMBIENT_PROTOCOL_INVALID;
    }

    switch (message->kind) {
    case AMBIENT_MESSAGE_CONTROL:
        if (message->event) {
            return AMBIENT_PROTOCOL_INVALID;
        }
        if (message->control_payload_length > max_control_payload_bytes) {
            return AMBIENT_PROTOCOL_TOO_LARGE;
        }
        if (message->control_payload_length > 0 && !message->control_payload) {
            return AMBIENT_PROTOCOL_INVALID;
        }
        return AMBIENT_PROTOCOL_VALID;
    case AMBIENT_MESSAGE_EVENT:
        if (message->control_payload || message->control_payload_length != 0
            || !ambient_event_is_valid(message->event)) {
            return AMBIENT_PROTOCOL_INVALID;
        }
        return AMBIENT_PROTOCOL_VALID;
    case AMBIENT_MESSAGE_INVALID:
    default:
        return AMBIENT_PROTOCOL_INVALID;
    }
}

bool ambient_line_framer_init(ambient_line_framer_t *framer,
                              char *buffer,
                              size_t capacity,
                              size_t max_line_bytes)
{
    if (!framer || !buffer || max_line_bytes == 0
        || max_line_bytes == SIZE_MAX || capacity < max_line_bytes + 1) {
        return false;
    }
    framer->buffer = buffer;
    framer->capacity = capacity;
    framer->length = 0;
    framer->max_line_bytes = max_line_bytes;
    framer->discarding_oversize_line = false;
    buffer[0] = '\0';
    return true;
}

ambient_frame_result_t ambient_line_framer_feed(
    ambient_line_framer_t *framer,
    const uint8_t *bytes,
    size_t byte_count,
    size_t max_line_bytes,
    size_t *consumed,
    const char **line,
    size_t *line_length)
{
    if (!framer || !framer->buffer || !consumed || !line || !line_length
        || (byte_count > 0 && !bytes) || max_line_bytes == 0
        || max_line_bytes != framer->max_line_bytes
        || max_line_bytes == SIZE_MAX
        || framer->capacity < framer->max_line_bytes + 1) {
        return AMBIENT_FRAME_INVALID;
    }

    *consumed = 0;
    *line = NULL;
    *line_length = 0;

    while (*consumed < byte_count) {
        uint8_t byte = bytes[*consumed];
        (*consumed)++;

        if (byte == '\n') {
            if (framer->discarding_oversize_line) {
                framer->discarding_oversize_line = false;
                framer->length = 0;
                framer->buffer[0] = '\0';
                return AMBIENT_FRAME_TOO_LARGE;
            }
            framer->buffer[framer->length] = '\0';
            *line = framer->buffer;
            *line_length = framer->length;
            framer->length = 0;
            return AMBIENT_FRAME_READY;
        }

        if (framer->discarding_oversize_line) {
            continue;
        }
        if (framer->length >= framer->max_line_bytes) {
            framer->discarding_oversize_line = true;
            framer->length = 0;
            continue;
        }
        framer->buffer[framer->length++] = (char)byte;
    }

    return AMBIENT_FRAME_NEED_MORE;
}

void ambient_line_framer_reset(ambient_line_framer_t *framer)
{
    if (!framer || !framer->buffer || framer->capacity == 0) {
        return;
    }
    framer->length = 0;
    framer->discarding_oversize_line = false;
    framer->buffer[0] = '\0';
}
