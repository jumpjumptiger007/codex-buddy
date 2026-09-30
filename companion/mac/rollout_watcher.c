#define _POSIX_C_SOURCE 200809L

#include "rollout_watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ROLLOUT_READ_CHUNK_BYTES 4096U
#define ROLLOUT_JSON_TOKEN_CAPACITY 512U
#define ROLLOUT_JSON_MAX_DEPTH 24U

typedef enum {
    JSON_TOKEN_INVALID = 0,
    JSON_TOKEN_OBJECT,
    JSON_TOKEN_ARRAY,
    JSON_TOKEN_STRING,
    JSON_TOKEN_PRIMITIVE,
} json_token_kind_t;

typedef struct {
    json_token_kind_t kind;
    size_t start;
    size_t length;
    size_t parent;
    size_t key_start;
    size_t key_length;
    bool has_key;
    bool key_escaped;
    bool string_escaped;
} json_token_t;

typedef struct {
    const char *source;
    size_t length;
    size_t position;
    json_token_t tokens[ROLLOUT_JSON_TOKEN_CAPACITY];
    size_t token_count;
} json_parser_t;

typedef struct {
    size_t start;
    size_t length;
    bool escaped;
} json_string_span_t;

typedef enum {
    LINE_IGNORED = 0,
    LINE_MALFORMED,
    LINE_INVALID_EVENT,
    LINE_UNVERIFIED_EVENT,
    LINE_INVALID_QUOTA,
    LINE_SESSION_META,
    LINE_EMITTED_EVENT,
    LINE_EMITTED_QUOTA,
} line_result_t;

static void rollout_watcher_clear_stream_state(rollout_watcher_t *watcher)
{
    watcher->line_length = 0;
    watcher->discarding_oversized_line = false;
    watcher->has_session_id = false;
    watcher->session_id_length = 0;
    watcher->session_id[0] = '\0';
}

bool rollout_watcher_init(rollout_watcher_t *watcher,
                          char *line_buffer,
                          size_t line_capacity,
                          size_t read_budget_bytes)
{
    if (!watcher || !line_buffer || line_capacity == 0
        || line_capacity > ROLLOUT_WATCHER_MAX_LINE_BYTES
        || read_budget_bytes == 0
        || read_budget_bytes > ROLLOUT_WATCHER_MAX_READ_BUDGET_BYTES) {
        return false;
    }

    memset(watcher, 0, sizeof(*watcher));
    watcher->line_buffer = line_buffer;
    watcher->line_capacity = line_capacity;
    watcher->read_budget_bytes = read_budget_bytes;
    return true;
}

void rollout_watcher_reset(rollout_watcher_t *watcher)
{
    if (!watcher) {
        return;
    }

    watcher->has_file_identity = false;
    watcher->file_device = 0;
    watcher->file_inode = 0;
    watcher->read_offset = 0;
    rollout_watcher_clear_stream_state(watcher);
}

static void json_skip_whitespace(json_parser_t *parser)
{
    while (parser->position < parser->length) {
        char ch = parser->source[parser->position];
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        parser->position++;
    }
}

static size_t json_utf8_sequence_length(const unsigned char *bytes,
                                       size_t available)
{
    unsigned char first;
    unsigned char second;

    if (!bytes || available == 0) {
        return 0;
    }
    first = bytes[0];
    if (first <= 0x7f) {
        return 1;
    }
    if (first >= 0xc2 && first <= 0xdf) {
        return available >= 2 && bytes[1] >= 0x80 && bytes[1] <= 0xbf
            ? 2 : 0;
    }
    if (first >= 0xe0 && first <= 0xef) {
        if (available < 3) {
            return 0;
        }
        second = bytes[1];
        if ((first == 0xe0 && (second < 0xa0 || second > 0xbf))
            || (first == 0xed && (second < 0x80 || second > 0x9f))
            || ((first != 0xe0 && first != 0xed)
                && (second < 0x80 || second > 0xbf))
            || bytes[2] < 0x80 || bytes[2] > 0xbf) {
            return 0;
        }
        return 3;
    }
    if (first >= 0xf0 && first <= 0xf4) {
        if (available < 4) {
            return 0;
        }
        second = bytes[1];
        if ((first == 0xf0 && (second < 0x90 || second > 0xbf))
            || (first == 0xf4 && (second < 0x80 || second > 0x8f))
            || ((first != 0xf0 && first != 0xf4)
                && (second < 0x80 || second > 0xbf))
            || bytes[2] < 0x80 || bytes[2] > 0xbf
            || bytes[3] < 0x80 || bytes[3] > 0xbf) {
            return 0;
        }
        return 4;
    }
    return 0;
}

static bool json_parse_hex_quad(const char *source,
                                size_t length,
                                size_t position,
                                uint16_t *value)
{
    uint16_t result = 0;
    if (!source || !value || position > length || length - position < 4) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        char ch = source[position + i];
        uint16_t digit;
        if (ch >= '0' && ch <= '9') {
            digit = (uint16_t)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = (uint16_t)(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            digit = (uint16_t)(ch - 'A' + 10);
        } else {
            return false;
        }
        result = (uint16_t)((result << 4) | digit);
    }
    *value = result;
    return true;
}

static bool json_parse_string_span(json_parser_t *parser,
                                   json_string_span_t *span)
{
    size_t start;
    bool escaped = false;

    if (!parser || !span || parser->position >= parser->length
        || parser->source[parser->position] != '"') {
        return false;
    }

    parser->position++;
    start = parser->position;
    while (parser->position < parser->length) {
        unsigned char ch = (unsigned char)parser->source[parser->position++];
        if (ch == '"') {
            span->start = start;
            span->length = parser->position - start - 1;
            span->escaped = escaped;
            return true;
        }
        if (ch < 0x20) {
            return false;
        }
        if (ch != '\\') {
            if (ch >= 0x80) {
                size_t sequence_length = json_utf8_sequence_length(
                    (const unsigned char *)parser->source + parser->position - 1,
                    parser->length - parser->position + 1);
                if (sequence_length == 0) {
                    return false;
                }
                parser->position += sequence_length - 1;
            }
            continue;
        }

        escaped = true;
        if (parser->position >= parser->length) {
            return false;
        }
        ch = (unsigned char)parser->source[parser->position++];
        if (ch == '"' || ch == '\\' || ch == '/' || ch == 'b'
            || ch == 'f' || ch == 'n' || ch == 'r' || ch == 't') {
            continue;
        }
        if (ch != 'u') {
            return false;
        }
        uint16_t code_unit;
        if (!json_parse_hex_quad(parser->source, parser->length,
                                 parser->position, &code_unit)) {
            return false;
        }
        parser->position += 4;
        if (code_unit >= 0xd800 && code_unit <= 0xdbff) {
            uint16_t low_surrogate;
            if (parser->length - parser->position < 6
                || parser->source[parser->position] != '\\'
                || parser->source[parser->position + 1] != 'u'
                || !json_parse_hex_quad(parser->source, parser->length,
                                        parser->position + 2,
                                        &low_surrogate)
                || low_surrogate < 0xdc00 || low_surrogate > 0xdfff) {
                return false;
            }
            parser->position += 6;
        } else if (code_unit >= 0xdc00 && code_unit <= 0xdfff) {
            return false;
        }
    }
    return false;
}

static bool json_parse_number(json_parser_t *parser)
{
    size_t position = parser->position;

    if (position < parser->length && parser->source[position] == '-') {
        position++;
    }
    if (position >= parser->length) {
        return false;
    }
    if (parser->source[position] == '0') {
        position++;
        if (position < parser->length && parser->source[position] >= '0'
            && parser->source[position] <= '9') {
            return false;
        }
    } else {
        if (parser->source[position] < '1' || parser->source[position] > '9') {
            return false;
        }
        do {
            position++;
        } while (position < parser->length
                 && parser->source[position] >= '0'
                 && parser->source[position] <= '9');
    }
    if (position < parser->length && parser->source[position] == '.') {
        position++;
        if (position >= parser->length || parser->source[position] < '0'
            || parser->source[position] > '9') {
            return false;
        }
        do {
            position++;
        } while (position < parser->length
                 && parser->source[position] >= '0'
                 && parser->source[position] <= '9');
    }
    if (position < parser->length
        && (parser->source[position] == 'e'
            || parser->source[position] == 'E')) {
        position++;
        if (position < parser->length
            && (parser->source[position] == '+'
                || parser->source[position] == '-')) {
            position++;
        }
        if (position >= parser->length || parser->source[position] < '0'
            || parser->source[position] > '9') {
            return false;
        }
        do {
            position++;
        } while (position < parser->length
                 && parser->source[position] >= '0'
                 && parser->source[position] <= '9');
    }

    parser->position = position;
    return true;
}

static size_t json_add_token(json_parser_t *parser,
                             json_token_kind_t kind,
                             size_t parent,
                             bool has_key,
                             const json_string_span_t *key)
{
    size_t index;
    json_token_t *token;

    if (parser->token_count >= ROLLOUT_JSON_TOKEN_CAPACITY) {
        return SIZE_MAX;
    }

    index = parser->token_count++;
    token = &parser->tokens[index];
    memset(token, 0, sizeof(*token));
    token->kind = kind;
    token->parent = parent;
    token->has_key = has_key;
    if (has_key && key) {
        token->key_start = key->start;
        token->key_length = key->length;
        token->key_escaped = key->escaped;
    }
    return index;
}

static bool json_parse_value(json_parser_t *parser,
                             size_t parent,
                             bool has_key,
                             const json_string_span_t *key,
                             size_t depth,
                             size_t *token_index);

static bool json_parse_object(json_parser_t *parser,
                             size_t object_index,
                             size_t depth)
{
    parser->position++;
    json_skip_whitespace(parser);
    if (parser->position < parser->length
        && parser->source[parser->position] == '}') {
        parser->position++;
        return true;
    }

    while (parser->position < parser->length) {
        json_string_span_t key;
        size_t child_index;

        if (!json_parse_string_span(parser, &key)) {
            return false;
        }
        json_skip_whitespace(parser);
        if (parser->position >= parser->length
            || parser->source[parser->position++] != ':') {
            return false;
        }
        json_skip_whitespace(parser);
        if (!json_parse_value(parser, object_index, true, &key, depth + 1,
                              &child_index)) {
            return false;
        }
        (void)child_index;
        json_skip_whitespace(parser);
        if (parser->position >= parser->length) {
            return false;
        }
        if (parser->source[parser->position] == '}') {
            parser->position++;
            return true;
        }
        if (parser->source[parser->position++] != ',') {
            return false;
        }
        json_skip_whitespace(parser);
    }
    return false;
}

static bool json_parse_array(json_parser_t *parser,
                             size_t array_index,
                             size_t depth)
{
    parser->position++;
    json_skip_whitespace(parser);
    if (parser->position < parser->length
        && parser->source[parser->position] == ']') {
        parser->position++;
        return true;
    }

    while (parser->position < parser->length) {
        size_t child_index;
        if (!json_parse_value(parser, array_index, false, NULL, depth + 1,
                              &child_index)) {
            return false;
        }
        (void)child_index;
        json_skip_whitespace(parser);
        if (parser->position >= parser->length) {
            return false;
        }
        if (parser->source[parser->position] == ']') {
            parser->position++;
            return true;
        }
        if (parser->source[parser->position++] != ',') {
            return false;
        }
        json_skip_whitespace(parser);
    }
    return false;
}

static bool json_parse_literal(json_parser_t *parser, const char *literal)
{
    size_t literal_length = strlen(literal);
    if (parser->length - parser->position < literal_length
        || memcmp(parser->source + parser->position, literal,
                  literal_length) != 0) {
        return false;
    }
    parser->position += literal_length;
    return true;
}

static bool json_parse_value(json_parser_t *parser,
                             size_t parent,
                             bool has_key,
                             const json_string_span_t *key,
                             size_t depth,
                             size_t *token_index)
{
    json_token_kind_t kind;
    size_t index;
    size_t start;

    if (!parser || !token_index || depth > ROLLOUT_JSON_MAX_DEPTH) {
        return false;
    }
    json_skip_whitespace(parser);
    if (parser->position >= parser->length) {
        return false;
    }

    start = parser->position;
    switch (parser->source[parser->position]) {
    case '{':
        kind = JSON_TOKEN_OBJECT;
        break;
    case '[':
        kind = JSON_TOKEN_ARRAY;
        break;
    case '"':
        kind = JSON_TOKEN_STRING;
        break;
    default:
        kind = JSON_TOKEN_PRIMITIVE;
        break;
    }

    index = json_add_token(parser, kind, parent, has_key, key);
    if (index == SIZE_MAX) {
        return false;
    }
    *token_index = index;
    parser->tokens[index].start = start;

    if (kind == JSON_TOKEN_OBJECT) {
        return json_parse_object(parser, index, depth);
    }
    if (kind == JSON_TOKEN_ARRAY) {
        return json_parse_array(parser, index, depth);
    }
    if (kind == JSON_TOKEN_STRING) {
        json_string_span_t span;
        if (!json_parse_string_span(parser, &span)) {
            return false;
        }
        parser->tokens[index].start = span.start;
        parser->tokens[index].length = span.length;
        parser->tokens[index].string_escaped = span.escaped;
        return true;
    }
    if (parser->source[parser->position] == 't') {
        if (!json_parse_literal(parser, "true")) {
            return false;
        }
    } else if (parser->source[parser->position] == 'f') {
        if (!json_parse_literal(parser, "false")) {
            return false;
        }
    } else if (parser->source[parser->position] == 'n') {
        if (!json_parse_literal(parser, "null")) {
            return false;
        }
    } else if (!json_parse_number(parser)) {
        return false;
    }
    parser->tokens[index].length = parser->position - start;
    return true;
}

static bool json_parse_document(const char *source,
                                size_t length,
                                json_parser_t *parser,
                                size_t *root_index)
{
    if (!source || !parser || !root_index || length == 0) {
        return false;
    }
    memset(parser, 0, sizeof(*parser));
    parser->source = source;
    parser->length = length;
    if (!json_parse_value(parser, SIZE_MAX, false, NULL, 0, root_index)) {
        return false;
    }
    json_skip_whitespace(parser);
    return parser->position == parser->length;
}

static bool json_object_has_exact_keys(const json_parser_t *parser,
                                       size_t object_index,
                                       const char *const *keys,
                                       size_t key_count)
{
    size_t found_count = 0;
    bool found[6] = {false};

    if (!parser || object_index >= parser->token_count
        || parser->tokens[object_index].kind != JSON_TOKEN_OBJECT
        || key_count > sizeof(found) / sizeof(found[0])) {
        return false;
    }

    for (size_t i = 0; i < parser->token_count; ++i) {
        const json_token_t *token = &parser->tokens[i];
        if (token->parent != object_index) {
            continue;
        }
        if (!token->has_key || token->key_escaped) {
            return false;
        }
        bool key_matched = false;
        for (size_t j = 0; j < key_count; ++j) {
            size_t length = strlen(keys[j]);
            if (token->key_length == length
                && memcmp(parser->source + token->key_start, keys[j], length) == 0) {
                if (found[j]) {
                    return false;
                }
                found[j] = true;
                found_count++;
                key_matched = true;
                break;
            }
        }
        if (!key_matched) {
            return false;
        }
    }

    return found_count == key_count;
}

static bool json_object_get(const json_parser_t *parser,
                            size_t object_index,
                            const char *key,
                            size_t *value_index)
{
    size_t match_count = 0;
    size_t match_index = SIZE_MAX;
    size_t key_length;

    if (!parser || !key || !value_index
        || object_index >= parser->token_count
        || parser->tokens[object_index].kind != JSON_TOKEN_OBJECT) {
        return false;
    }
    key_length = strlen(key);
    for (size_t i = 0; i < parser->token_count; ++i) {
        const json_token_t *token = &parser->tokens[i];
        if (token->parent != object_index || !token->has_key) {
            continue;
        }
        if (token->key_escaped) {
            return false;
        }
        if (token->key_length == key_length
            && memcmp(parser->source + token->key_start, key, key_length) == 0) {
            match_index = i;
            match_count++;
        }
    }
    if (match_count != 1) {
        return false;
    }
    *value_index = match_index;
    return true;
}

static bool json_object_get_optional(const json_parser_t *parser,
                                     size_t object_index,
                                     const char *key,
                                     size_t *value_index,
                                     bool *present)
{
    size_t match_count = 0;
    size_t match_index = SIZE_MAX;
    size_t key_length;

    if (!parser || !key || !value_index || !present
        || object_index >= parser->token_count
        || parser->tokens[object_index].kind != JSON_TOKEN_OBJECT) {
        return false;
    }
    key_length = strlen(key);
    for (size_t i = 0; i < parser->token_count; ++i) {
        const json_token_t *token = &parser->tokens[i];
        if (token->parent != object_index || !token->has_key) {
            continue;
        }
        if (token->key_escaped) {
            return false;
        }
        if (token->key_length == key_length
            && memcmp(parser->source + token->key_start, key, key_length) == 0) {
            match_index = i;
            match_count++;
        }
    }
    if (match_count > 1) {
        return false;
    }
    *present = match_count == 1;
    if (*present) {
        *value_index = match_index;
    }
    return true;
}

static bool json_object_has_only_keys(const json_parser_t *parser,
                                      size_t object_index,
                                      const char *const *keys,
                                      size_t key_count)
{
    bool found[4] = {false};

    if (!parser || object_index >= parser->token_count
        || parser->tokens[object_index].kind != JSON_TOKEN_OBJECT
        || !keys || key_count > sizeof(found) / sizeof(found[0])) {
        return false;
    }
    for (size_t i = 0; i < parser->token_count; ++i) {
        const json_token_t *token = &parser->tokens[i];
        bool matched = false;
        if (token->parent != object_index) {
            continue;
        }
        if (!token->has_key || token->key_escaped) {
            return false;
        }
        for (size_t j = 0; j < key_count; ++j) {
            size_t key_length = strlen(keys[j]);
            if (token->key_length == key_length
                && memcmp(parser->source + token->key_start,
                          keys[j], key_length) == 0) {
                if (found[j]) {
                    return false;
                }
                found[j] = true;
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}

static bool json_string_equals(const json_parser_t *parser,
                               size_t token_index,
                               const char *value)
{
    const json_token_t *token;
    size_t value_length;
    if (!parser || !value || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    if (token->kind != JSON_TOKEN_STRING || token->string_escaped) {
        return false;
    }
    value_length = strlen(value);
    return token->length == value_length
        && memcmp(parser->source + token->start, value, value_length) == 0;
}

static bool json_token_is_null(const json_parser_t *parser,
                               size_t token_index)
{
    const json_token_t *token;
    if (!parser || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    return token->kind == JSON_TOKEN_PRIMITIVE && token->length == 4
        && memcmp(parser->source + token->start, "null", 4) == 0;
}

static bool json_string_copy(const json_parser_t *parser,
                             size_t token_index,
                             char *destination,
                             size_t destination_capacity,
                             size_t *length_out)
{
    const json_token_t *token;
    if (!parser || !destination || destination_capacity == 0 || !length_out
        || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    if (token->kind != JSON_TOKEN_STRING || token->string_escaped
        || token->length == 0 || token->length >= destination_capacity) {
        return false;
    }
    memcpy(destination, parser->source + token->start, token->length);
    destination[token->length] = '\0';
    *length_out = token->length;
    return true;
}

static bool json_token_is_integer(const json_parser_t *parser,
                                  size_t token_index,
                                  bool require_positive,
                                  uint64_t *unsigned_value)
{
    const json_token_t *token;
    size_t position;
    uint64_t value = 0;

    if (!parser || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    if (token->kind != JSON_TOKEN_PRIMITIVE || token->length == 0) {
        return false;
    }
    position = token->start;
    if (parser->source[position] == '-') {
        if (require_positive || ++position >= token->start + token->length) {
            return false;
        }
    }
    for (; position < token->start + token->length; ++position) {
        unsigned char ch = (unsigned char)parser->source[position];
        if (ch < '0' || ch > '9') {
            return false;
        }
        if (unsigned_value) {
            unsigned digit = (unsigned)(ch - '0');
            if (value > (UINT64_MAX - digit) / 10) {
                return false;
            }
            value = value * 10 + digit;
        }
    }
    if (require_positive && value == 0) {
        return false;
    }
    if (unsigned_value) {
        *unsigned_value = value;
    }
    return true;
}

static bool json_token_to_int64(const json_parser_t *parser,
                                size_t token_index,
                                int64_t *value_out)
{
    const json_token_t *token;
    char buffer[32];
    char *end = NULL;
    long long value;

    if (!parser || !value_out || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    if (token->kind != JSON_TOKEN_PRIMITIVE || token->length == 0
        || token->length >= sizeof(buffer)) {
        return false;
    }
    for (size_t i = 0; i < token->length; ++i) {
        char ch = parser->source[token->start + i];
        if ((i == 0 && ch == '-') || (ch >= '0' && ch <= '9')) {
            continue;
        }
        return false;
    }
    memcpy(buffer, parser->source + token->start, token->length);
    buffer[token->length] = '\0';
    errno = 0;
    value = strtoll(buffer, &end, 10);
    if (errno == ERANGE || end != buffer + token->length) {
        return false;
    }
    *value_out = (int64_t)value;
    return true;
}

static bool json_token_to_double(const json_parser_t *parser,
                                 size_t token_index,
                                 double *value_out)
{
    const json_token_t *token;
    char buffer[64];
    char *end = NULL;
    double value;

    if (!parser || !value_out || token_index >= parser->token_count) {
        return false;
    }
    token = &parser->tokens[token_index];
    if (token->kind != JSON_TOKEN_PRIMITIVE || token->length == 0
        || token->length >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, parser->source + token->start, token->length);
    buffer[token->length] = '\0';
    errno = 0;
    value = strtod(buffer, &end);
    if (errno == ERANGE || end != buffer + token->length || !isfinite(value)) {
        return false;
    }
    *value_out = value;
    return true;
}

static bool rollout_watcher_parse_quota_window(
    const json_parser_t *parser,
    size_t window_index,
    rollout_quota_window_input_t *window,
    bool *present)
{
    static const char *const window_keys[] = {
        "used_percent", "window_minutes", "reset_at"
    };
    size_t value_index;
    bool has_value;

    if (!parser || !window || !present
        || window_index >= parser->token_count) {
        return false;
    }
    memset(window, 0, sizeof(*window));
    *present = false;
    if (json_token_is_null(parser, window_index)) {
        return true;
    }
    if (!json_object_has_only_keys(
            parser, window_index, window_keys,
            sizeof(window_keys) / sizeof(window_keys[0]))) {
        return false;
    }
    if (!json_object_get_optional(parser, window_index, "used_percent",
                                  &value_index, &has_value)
        || !has_value || !json_token_to_double(parser, value_index,
                                               &window->used_percent)) {
        return false;
    }
    window->used_percent_present = true;
    if (!json_object_get_optional(parser, window_index, "window_minutes",
                                  &value_index, &has_value)) {
        return false;
    }
    if (has_value && !json_token_is_null(parser, value_index)) {
        if (!json_token_to_int64(parser, value_index, &window->window_minutes)) {
            return false;
        }
        window->window_minutes_present = true;
    }
    if (!json_object_get_optional(parser, window_index, "reset_at",
                                  &value_index, &has_value)) {
        return false;
    }
    if (has_value && !json_token_is_null(parser, value_index)) {
        if (!json_token_to_int64(parser, value_index, &window->resets_at)) {
            return false;
        }
        window->resets_at_present = true;
    }
    *present = true;
    return true;
}

static bool rollout_watcher_copy_limit_id(
    const json_parser_t *parser,
    size_t payload_index,
    rollout_rate_limits_input_t *rate_limits)
{
    size_t value_index;
    bool has_metered_name;
    bool has_limit_name;
    const char *field_name;
    size_t field_length;

    if (!json_object_get_optional(parser, payload_index,
                                  "metered_limit_name", &value_index,
                                  &has_metered_name)) {
        return false;
    }
    if (has_metered_name
        && !json_token_is_null(parser, value_index)) {
        field_name = "metered_limit_name";
    } else {
        if (!json_object_get_optional(parser, payload_index, "limit_name",
                                      &value_index, &has_limit_name)) {
            return false;
        }
        if (!has_limit_name || json_token_is_null(parser, value_index)) {
            memcpy(rate_limits->limit_id, "codex", sizeof("codex"));
            return true;
        }
        field_name = "limit_name";
    }
    if (!json_object_get(parser, payload_index, field_name, &value_index)) {
        return false;
    }
    if (!json_string_copy(parser, value_index, rate_limits->limit_id,
                          sizeof(rate_limits->limit_id), &field_length)) {
        return false;
    }
    for (size_t i = 0; i < field_length; ++i) {
        char ch = rate_limits->limit_id[i];
        if (ch >= 'A' && ch <= 'Z') {
            rate_limits->limit_id[i] = (char)(ch - 'A' + 'a');
        } else if (ch == '-') {
            rate_limits->limit_id[i] = '_';
        }
    }
    return true;
}

static line_result_t rollout_watcher_parse_rate_limits(
    const json_parser_t *parser,
    size_t payload_index,
    rollout_watcher_quota_fn on_quota,
    void *context)
{
    static const char *const rate_limits_keys[] = {"primary", "secondary"};
    rollout_rate_limits_input_t rate_limits = {0};
    size_t rate_limits_index;
    size_t primary_index;
    size_t secondary_index;
    bool has_rate_limits;
    bool has_primary;
    bool has_secondary;

    if (!rollout_watcher_copy_limit_id(parser, payload_index, &rate_limits)) {
        return LINE_INVALID_QUOTA;
    }
    if (!json_object_get_optional(parser, payload_index, "rate_limits",
                                  &rate_limits_index, &has_rate_limits)) {
        return LINE_INVALID_QUOTA;
    }
    if (has_rate_limits) {
        if (json_token_is_null(parser, rate_limits_index)) {
            has_rate_limits = false;
        } else if (!json_object_has_only_keys(
                       parser, rate_limits_index, rate_limits_keys,
                       sizeof(rate_limits_keys) / sizeof(rate_limits_keys[0]))) {
            return LINE_INVALID_QUOTA;
        }
    }
    if (has_rate_limits) {
        if (!json_object_get_optional(parser, rate_limits_index, "primary",
                                      &primary_index, &has_primary)
            || !json_object_get_optional(parser, rate_limits_index, "secondary",
                                         &secondary_index, &has_secondary)) {
            return LINE_INVALID_QUOTA;
        }
        if (has_primary) {
            if (!rollout_watcher_parse_quota_window(
                    parser, primary_index, &rate_limits.primary,
                    &rate_limits.has_primary)) {
                return LINE_INVALID_QUOTA;
            }
        }
        if (has_secondary) {
            if (!rollout_watcher_parse_quota_window(
                    parser, secondary_index, &rate_limits.secondary,
                    &rate_limits.has_secondary)) {
                return LINE_INVALID_QUOTA;
            }
        }
    }
    if (!on_quota) {
        return LINE_IGNORED;
    }
    on_quota(context, &rate_limits);
    return LINE_EMITTED_QUOTA;
}

static line_result_t rollout_watcher_parse_line(
    rollout_watcher_t *watcher,
    const char *line,
    size_t line_length,
    rollout_watcher_event_fn on_event,
    rollout_watcher_quota_fn on_quota,
    void *context)
{
    static const char *const event_root_keys[] = {
        "type", "ordinal", "timestamp", "payload"
    };
    static const char *const aborted_payload_keys[] = {
        "type", "turn_id", "reason", "started_at", "completed_at",
        "duration_ms"
    };
    json_parser_t parser;
    size_t root_index;
    size_t type_index;
    size_t payload_index;

    if (!json_parse_document(line, line_length, &parser, &root_index)) {
        watcher->has_session_id = false;
        watcher->session_id_length = 0;
        watcher->session_id[0] = '\0';
        return LINE_MALFORMED;
    }
    if (parser.tokens[root_index].kind != JSON_TOKEN_OBJECT
        || !json_object_get(&parser, root_index, "type", &type_index)) {
        return LINE_IGNORED;
    }

    if (json_string_equals(&parser, type_index, "session_meta")) {
        size_t session_id_index;
        if (!json_object_get(&parser, root_index, "payload", &payload_index)
            || parser.tokens[payload_index].kind != JSON_TOKEN_OBJECT
            || !json_object_get(&parser, payload_index, "session_id",
                             &session_id_index)
            || !json_string_copy(&parser, session_id_index,
                                 watcher->session_id,
                                 sizeof(watcher->session_id),
                                 &watcher->session_id_length)) {
            watcher->has_session_id = false;
            watcher->session_id_length = 0;
            watcher->session_id[0] = '\0';
            return LINE_IGNORED;
        }
        watcher->has_session_id = true;
        return LINE_SESSION_META;
    }

    if (!json_string_equals(&parser, type_index, "event_msg")) {
        return LINE_IGNORED;
    }
    if (!json_object_get(&parser, root_index, "payload", &payload_index)
        || parser.tokens[payload_index].kind != JSON_TOKEN_OBJECT) {
        return LINE_UNVERIFIED_EVENT;
    }
    if (!watcher->has_session_id) {
        return LINE_UNVERIFIED_EVENT;
    }

    size_t payload_type_index;
    if (!json_object_get(&parser, payload_index, "type", &payload_type_index)) {
        return LINE_INVALID_EVENT;
    }
    if (json_string_equals(&parser, payload_type_index, "codex.rate_limits")) {
        return rollout_watcher_parse_rate_limits(
            &parser, payload_index, on_quota, context);
    }
    if (!json_string_equals(&parser, payload_type_index, "turn_aborted")) {
        return LINE_UNVERIFIED_EVENT;
    }

    size_t ordinal_index;
    size_t turn_id_index;
    size_t reason_index;
    size_t started_at_index;
    size_t completed_at_index;
    size_t duration_ms_index;
    size_t timestamp_index;
    uint64_t ordinal;
    if (!json_object_has_exact_keys(&parser, root_index,
                                    event_root_keys,
                                    sizeof(event_root_keys)
                                        / sizeof(event_root_keys[0]))
        || !json_object_has_exact_keys(&parser, payload_index,
                                       aborted_payload_keys,
                                       sizeof(aborted_payload_keys)
                                           / sizeof(aborted_payload_keys[0]))
        || !json_object_get(&parser, root_index, "ordinal", &ordinal_index)
        || !json_token_is_integer(&parser, ordinal_index, true, &ordinal)
        || !json_object_get(&parser, root_index, "timestamp", &timestamp_index)
        || parser.tokens[timestamp_index].kind != JSON_TOKEN_STRING
        || !json_object_get(&parser, payload_index, "turn_id", &turn_id_index)
        || parser.tokens[turn_id_index].kind != JSON_TOKEN_STRING
        || parser.tokens[turn_id_index].string_escaped
        || parser.tokens[turn_id_index].length == 0
        || !json_object_get(&parser, payload_index, "reason", &reason_index)
        || parser.tokens[reason_index].kind != JSON_TOKEN_STRING
        || !json_object_get(&parser, payload_index, "started_at",
                            &started_at_index)
        || !json_token_is_integer(&parser, started_at_index, false, NULL)
        || !json_object_get(&parser, payload_index, "completed_at",
                            &completed_at_index)
        || !json_token_is_integer(&parser, completed_at_index, false, NULL)
        || !json_object_get(&parser, payload_index, "duration_ms",
                            &duration_ms_index)
        || !json_token_is_integer(&parser, duration_ms_index, false, NULL)) {
        return LINE_INVALID_EVENT;
    }

    if (on_event) {
        rollout_lifecycle_input_t event = {
            .kind = AMBIENT_EVENT_TURN_ABORTED,
            .session_id = watcher->session_id,
            .session_id_length = watcher->session_id_length,
            .turn_id = line + parser.tokens[turn_id_index].start,
            .turn_id_length = parser.tokens[turn_id_index].length,
            .source_ordinal = ordinal,
        };
        on_event(context, &event);
        return LINE_EMITTED_EVENT;
    }
    return LINE_IGNORED;
}

static void rollout_watcher_finish_line(rollout_watcher_t *watcher,
                                        rollout_watcher_event_fn on_event,
                                        rollout_watcher_quota_fn on_quota,
                                        void *context,
                                        rollout_watcher_stats_t *stats)
{
    line_result_t result;
    stats->complete_lines++;
    if (watcher->line_length == 0) {
        stats->ignored_lines++;
        return;
    }
    result = rollout_watcher_parse_line(watcher, watcher->line_buffer,
                                        watcher->line_length, on_event,
                                        on_quota, context);
    switch (result) {
    case LINE_MALFORMED:
        stats->malformed_lines++;
        break;
    case LINE_INVALID_EVENT:
        stats->invalid_event_lines++;
        break;
    case LINE_UNVERIFIED_EVENT:
        stats->unverified_event_lines++;
        break;
    case LINE_INVALID_QUOTA:
        stats->invalid_quota_lines++;
        break;
    case LINE_EMITTED_EVENT:
        stats->emitted_events++;
        break;
    case LINE_EMITTED_QUOTA:
        stats->emitted_quota_updates++;
        break;
    case LINE_SESSION_META:
    case LINE_IGNORED:
    default:
        stats->ignored_lines++;
        break;
    }
}

static void rollout_watcher_consume_byte(rollout_watcher_t *watcher,
                                        char byte,
                                        rollout_watcher_event_fn on_event,
                                        rollout_watcher_quota_fn on_quota,
                                        void *context,
                                        rollout_watcher_stats_t *stats)
{
    if (byte == '\n') {
        if (watcher->discarding_oversized_line) {
            stats->complete_lines++;
            watcher->discarding_oversized_line = false;
            watcher->line_length = 0;
            return;
        }
        rollout_watcher_finish_line(watcher, on_event, on_quota,
                                    context, stats);
        watcher->line_length = 0;
        return;
    }

    if (watcher->discarding_oversized_line) {
        return;
    }
    if (watcher->line_length >= watcher->line_capacity) {
        watcher->line_length = 0;
        watcher->discarding_oversized_line = true;
        watcher->has_session_id = false;
        watcher->session_id_length = 0;
        watcher->session_id[0] = '\0';
        stats->oversized_lines++;
        return;
    }
    watcher->line_buffer[watcher->line_length++] = byte;
}

rollout_watcher_result_t rollout_watcher_poll_with_quota(
    rollout_watcher_t *watcher,
    const char *path,
    rollout_watcher_event_fn on_event,
    rollout_watcher_quota_fn on_quota,
    void *context,
    rollout_watcher_stats_t *stats)
{
    struct stat file_stat;
    char chunk[ROLLOUT_READ_CHUNK_BYTES];
    size_t read_budget;
    int fd;

    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
    if (!watcher || !watcher->line_buffer || watcher->line_capacity == 0
        || watcher->read_budget_bytes == 0 || !path || path[0] == '\0'
        || !stats) {
        return ROLLOUT_WATCHER_INVALID_ARGUMENT;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return ROLLOUT_WATCHER_IO_ERROR;
    }
    if (fstat(fd, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)
        || file_stat.st_size < 0) {
        (void)close(fd);
        return ROLLOUT_WATCHER_IO_ERROR;
    }

    uint64_t device = (uint64_t)file_stat.st_dev;
    uint64_t inode = (uint64_t)file_stat.st_ino;
    bool changed = watcher->has_file_identity
        && (watcher->file_device != device || watcher->file_inode != inode);
    bool truncated = watcher->has_file_identity && !changed
        && (uint64_t)file_stat.st_size < (uint64_t)watcher->read_offset;
    if (!watcher->has_file_identity || changed || truncated) {
        stats->file_changed = changed;
        stats->file_truncated = truncated;
        watcher->file_device = device;
        watcher->file_inode = inode;
        watcher->has_file_identity = true;
        watcher->read_offset = 0;
        rollout_watcher_clear_stream_state(watcher);
    }

    if (watcher->read_offset < 0 || watcher->read_offset > INT64_MAX) {
        (void)close(fd);
        return ROLLOUT_WATCHER_IO_ERROR;
    }
    read_budget = watcher->read_budget_bytes;
    while (stats->bytes_read < read_budget) {
        size_t remaining = read_budget - stats->bytes_read;
        size_t request = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        ssize_t amount = pread(fd, chunk, request, (off_t)watcher->read_offset);
        if (amount < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(fd);
            return ROLLOUT_WATCHER_IO_ERROR;
        }
        if (amount == 0) {
            break;
        }
        if ((uint64_t)amount > (uint64_t)(INT64_MAX - watcher->read_offset)) {
            (void)close(fd);
            return ROLLOUT_WATCHER_IO_ERROR;
        }
        watcher->read_offset += (int64_t)amount;
        stats->bytes_read += (size_t)amount;
        for (ssize_t i = 0; i < amount; ++i) {
            rollout_watcher_consume_byte(watcher, chunk[i], on_event,
                                         on_quota, context, stats);
        }
    }

    if (stats->bytes_read == read_budget
        && (uint64_t)watcher->read_offset < (uint64_t)file_stat.st_size) {
        stats->read_budget_exhausted = true;
    }
    stats->partial_line_pending = watcher->line_length > 0
        || watcher->discarding_oversized_line;
    (void)close(fd);
    return ROLLOUT_WATCHER_OK;
}

rollout_watcher_result_t rollout_watcher_poll(
    rollout_watcher_t *watcher,
    const char *path,
    rollout_watcher_event_fn on_event,
    void *context,
    rollout_watcher_stats_t *stats)
{
    return rollout_watcher_poll_with_quota(
        watcher, path, on_event, NULL, context, stats);
}
