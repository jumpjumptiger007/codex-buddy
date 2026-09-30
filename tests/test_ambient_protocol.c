#include <assert.h>
#include <string.h>

#include "ambient_protocol.h"

int main(void)
{
    static const uint8_t control_data[] = {0x01, 0x02, 0x03};
    static const uint8_t first_chunk[] = {'a', 'b'};
    static const uint8_t second_chunk[] = {'c', '\n', 'd', '\n'};
    static const uint8_t exact_limit[] = {'w', 'x', 'y', 'z', '\n'};
    static const uint8_t oversized[] = {
        '1', '2', '3', '4', '5', '\n', 'x', '\n'
    };
    ambient_event_t event = {
        .kind = AMBIENT_EVENT_TURN_STARTED,
        .session_key = 1,
        .turn_key = 2,
        .event_key = 3,
    };
    ambient_protocol_message_t message = {
        .kind = AMBIENT_MESSAGE_CONTROL,
        .control_payload = control_data,
        .control_payload_length = sizeof(control_data),
    };
    char storage[5];
    ambient_line_framer_t framer;
    const char *line;
    size_t line_length;
    size_t consumed;

    assert(ambient_protocol_validate_message(&message, 3)
           == AMBIENT_PROTOCOL_VALID);
    assert(ambient_protocol_validate_message(&message, 2)
           == AMBIENT_PROTOCOL_TOO_LARGE);
    message.control_payload = NULL;
    assert(ambient_protocol_validate_message(&message, 3)
           == AMBIENT_PROTOCOL_INVALID);
    message = (ambient_protocol_message_t){
        .kind = AMBIENT_MESSAGE_EVENT,
        .event = &event,
    };
    assert(ambient_protocol_validate_message(&message, 0)
           == AMBIENT_PROTOCOL_VALID);
    event.turn_key = 0;
    assert(ambient_protocol_validate_message(&message, 0)
           == AMBIENT_PROTOCOL_INVALID);
    message.kind = AMBIENT_MESSAGE_INVALID;
    assert(ambient_protocol_validate_message(&message, 0)
           == AMBIENT_PROTOCOL_INVALID);

    assert(!ambient_line_framer_init(&framer, storage, sizeof(storage), 0));
    assert(ambient_line_framer_init(&framer, storage, sizeof(storage), 4));
    assert(ambient_line_framer_feed(&framer, first_chunk,
                                    sizeof(first_chunk), 4, &consumed,
                                    &line, &line_length)
           == AMBIENT_FRAME_NEED_MORE);
    assert(consumed == sizeof(first_chunk));
    assert(ambient_line_framer_feed(&framer, second_chunk,
                                    sizeof(second_chunk), 1, &consumed,
                                    &line, &line_length)
           == AMBIENT_FRAME_INVALID);
    assert(ambient_line_framer_feed(&framer, second_chunk,
                                    sizeof(second_chunk), 4, &consumed,
                                    &line, &line_length)
           == AMBIENT_FRAME_READY);
    assert(consumed == 2);
    assert(line_length == 3 && memcmp(line, "abc", 3) == 0);
    assert(ambient_line_framer_feed(&framer, second_chunk + consumed,
                                    sizeof(second_chunk) - consumed, 4,
                                    &consumed, &line, &line_length)
           == AMBIENT_FRAME_READY);
    assert(consumed == 2);
    assert(line_length == 1 && line[0] == 'd');
    assert(ambient_line_framer_feed(&framer, exact_limit,
                                    sizeof(exact_limit), 4, &consumed,
                                    &line, &line_length)
           == AMBIENT_FRAME_READY);
    assert(consumed == sizeof(exact_limit));
    assert(line_length == 4 && memcmp(line, "wxyz", 4) == 0);

    assert(ambient_line_framer_feed(&framer, oversized, sizeof(oversized),
                                    4, &consumed, &line, &line_length)
           == AMBIENT_FRAME_TOO_LARGE);
    assert(consumed == 6);
    assert(ambient_line_framer_feed(&framer, oversized + consumed,
                                    sizeof(oversized) - consumed, 4,
                                    &consumed, &line, &line_length)
           == AMBIENT_FRAME_READY);
    assert(line_length == 1 && line[0] == 'x');
    ambient_line_framer_reset(&framer);
    assert(!ambient_line_framer_feed(&framer, NULL, 1, 4, &consumed,
                                     &line, &line_length));
    return 0;
}
