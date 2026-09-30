#define _POSIX_C_SOURCE 200809L

#include "rollout_watcher.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_EVENT_CAPACITY 8U

typedef struct {
    ambient_event_kind_t kind;
    char session_id[ROLLOUT_WATCHER_SESSION_ID_CAPACITY];
    char turn_id[64];
    uint64_t ordinal;
} captured_event_t;

typedef struct {
    captured_event_t events[TEST_EVENT_CAPACITY];
    size_t count;
    rollout_rate_limits_input_t quota_updates[TEST_EVENT_CAPACITY];
    size_t quota_count;
} event_capture_t;

static const char session_a[] =
    "{\"type\":\"session_meta\",\"payload\":{\"session_id\":\"session-a\"}}\n";
static const char session_b[] =
    "{\"type\":\"session_meta\",\"payload\":{\"session_id\":\"session-b\"}}\n";
static const char event_a[] =
    "{\"type\":\"event_msg\",\"ordinal\":7,\"timestamp\":\"2026-09-29T00:00:00Z\",\"payload\":{\"type\":\"turn_aborted\",\"turn_id\":\"turn-a\",\"reason\":\"user\",\"started_at\":1,\"completed_at\":2,\"duration_ms\":1}}\n";
static const char event_b[] =
    "{\"type\":\"event_msg\",\"ordinal\":9,\"timestamp\":\"2026-09-29T00:00:02Z\",\"payload\":{\"type\":\"turn_aborted\",\"turn_id\":\"turn-b\",\"reason\":\"user\",\"started_at\":2,\"completed_at\":3,\"duration_ms\":1}}\n";

static void capture_event(void *context,
                          const rollout_lifecycle_input_t *event)
{
    event_capture_t *capture = context;
    captured_event_t *saved;
    assert(capture != NULL);
    assert(event != NULL);
    assert(capture->count < TEST_EVENT_CAPACITY);
    assert(event->session_id_length < sizeof(capture->events[0].session_id));
    assert(event->turn_id_length < sizeof(capture->events[0].turn_id));
    saved = &capture->events[capture->count++];
    saved->kind = event->kind;
    memcpy(saved->session_id, event->session_id, event->session_id_length);
    saved->session_id[event->session_id_length] = '\0';
    memcpy(saved->turn_id, event->turn_id, event->turn_id_length);
    saved->turn_id[event->turn_id_length] = '\0';
    saved->ordinal = event->source_ordinal;
}

static void capture_quota(void *context,
                          const rollout_rate_limits_input_t *rate_limits)
{
    event_capture_t *capture = context;
    assert(capture != NULL);
    assert(rate_limits != NULL);
    assert(capture->quota_count < TEST_EVENT_CAPACITY);
    capture->quota_updates[capture->quota_count++] = *rate_limits;
}

static void write_bytes(const char *path, const char *bytes, size_t length,
                        bool append)
{
    int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
    int fd = open(path, flags, 0600);
    size_t offset = 0;
    assert(fd >= 0);
    while (offset < length) {
        ssize_t amount = write(fd, bytes + offset, length - offset);
        assert(amount > 0);
        offset += (size_t)amount;
    }
    assert(close(fd) == 0);
}

static void write_pair(const char *path, const char *session,
                       const char *event, bool append)
{
    write_bytes(path, session, strlen(session), append);
    write_bytes(path, event, strlen(event), true);
}

static void make_test_paths(char *path,
                           size_t path_capacity,
                           char *backup,
                           size_t backup_capacity)
{
    char pattern[] = "/tmp/rollout-watcher-tests.XXXXXX";
    int fd = mkstemp(pattern);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(strlen(pattern) + 1 <= path_capacity);
    memcpy(path, pattern, strlen(pattern) + 1);
    assert(snprintf(backup, backup_capacity, "%s.old", path) > 0);
}

static void assert_one_event(const event_capture_t *capture,
                             size_t index,
                             const char *session_id,
                             const char *turn_id,
                             uint64_t ordinal)
{
    assert(index < capture->count);
    assert(capture->events[index].kind == AMBIENT_EVENT_TURN_ABORTED);
    assert(strcmp(capture->events[index].session_id, session_id) == 0);
    assert(strcmp(capture->events[index].turn_id, turn_id) == 0);
    assert(capture->events[index].ordinal == ordinal);
}

static void test_incremental_partial_and_verified_event(void)
{
    char path[128];
    char backup[128];
    char line_buffer[1024];
    rollout_watcher_t watcher;
    rollout_watcher_stats_t stats;
    event_capture_t capture = {0};
    size_t event_length = strlen(event_a);
    size_t initial_bytes = strlen(session_a) + event_length - 1;

    make_test_paths(path, sizeof(path), backup, sizeof(backup));
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                32));
    write_bytes(path, session_a, strlen(session_a), false);
    write_bytes(path, event_a, event_length - 1, true);

    while ((size_t)watcher.read_offset < initial_bytes) {
        assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                    &stats) == ROLLOUT_WATCHER_OK);
        assert(stats.bytes_read <= 32);
        assert(stats.read_budget_exhausted
               || (size_t)watcher.read_offset == initial_bytes);
        assert(capture.count == 0);
    }
    assert(stats.partial_line_pending);
    assert(capture.count == 0);

    write_bytes(path, "\n", 1, true);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(!stats.partial_line_pending);
    assert(capture.count == 1);
    assert_one_event(&capture, 0, "session-a", "turn-a", 7);

    assert(unlink(path) == 0);
}

static void test_unknown_events_and_malformed_input_fail_closed(void)
{
    static const char unknown_events[] =
        "{\"type\":\"event_msg\",\"ordinal\":8,\"timestamp\":\"t\",\"payload\":{\"type\":\"task_started\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":9,\"timestamp\":\"t\",\"payload\":{\"type\":\"task_complete\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":10,\"timestamp\":\"t\",\"payload\":{\"type\":\"thread_settings_applied\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":11,\"timestamp\":\"t\",\"payload\":{\"type\":\"SessionConfigured\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":12,\"timestamp\":\"t\",\"payload\":{\"type\":\"TurnStarted\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":13,\"timestamp\":\"t\",\"payload\":{\"type\":\"TurnComplete\"}}\n";
    char path[128];
    char backup[128];
    char line_buffer[1024];
    rollout_watcher_t watcher;
    rollout_watcher_stats_t stats;
    event_capture_t capture = {0};

    make_test_paths(path, sizeof(path), backup, sizeof(backup));
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                4096));
    write_bytes(path, session_a, strlen(session_a), false);
    write_bytes(path, unknown_events, strlen(unknown_events), true);
    write_bytes(path, "{bad}\n", strlen("{bad}\n"), true);
    write_bytes(path, event_a, strlen(event_a), true);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 0);
    assert(stats.unverified_event_lines == 7);
    assert(stats.malformed_lines == 1);

    write_pair(path, session_a, event_a, true);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 1);
    assert_one_event(&capture, 0, "session-a", "turn-a", 7);

    assert(unlink(path) == 0);
}

static void test_oversized_line_discards_until_new_session_metadata(void)
{
    char path[128];
    char backup[128];
    char line_buffer[256];
    char oversized[320];
    rollout_watcher_t watcher;
    rollout_watcher_stats_t stats;
    event_capture_t capture = {0};

    memset(oversized, 'x', sizeof(oversized));
    oversized[sizeof(oversized) - 1] = '\n';
    make_test_paths(path, sizeof(path), backup, sizeof(backup));
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                4096));
    write_bytes(path, session_a, strlen(session_a), false);
    write_bytes(path, oversized, sizeof(oversized), true);
    write_bytes(path, event_a, strlen(event_a), true);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(stats.oversized_lines == 1);
    assert(stats.unverified_event_lines == 1);
    assert(capture.count == 0);

    write_pair(path, session_b, event_b, true);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 1);
    assert_one_event(&capture, 0, "session-b", "turn-b", 9);

    assert(unlink(path) == 0);
}

static void test_file_rotation_and_truncation_reset_identity(void)
{
    char path[128];
    char backup[128];
    char line_buffer[2048];
    rollout_watcher_t watcher;
    rollout_watcher_stats_t stats;
    event_capture_t capture = {0};

    make_test_paths(path, sizeof(path), backup, sizeof(backup));
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                4096));
    write_pair(path, session_a, event_a, false);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 1);

    assert(rename(path, backup) == 0);
    write_pair(path, session_b, event_b, false);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(stats.file_changed);
    assert(capture.count == 2);
    assert_one_event(&capture, 1, "session-b", "turn-b", 9);

    int fd = open(path, O_WRONLY | O_TRUNC);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(stats.file_truncated);
    assert(capture.count == 2);

    write_pair(path, session_a, event_a, false);
    assert(rollout_watcher_poll(&watcher, path, capture_event, &capture,
                                &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 3);
    assert_one_event(&capture, 2, "session-a", "turn-a", 7);

    assert(unlink(path) == 0);
    assert(unlink(backup) == 0);
}

static void test_persisted_rate_limits_extracts_by_window_minutes(void)
{
    static const char quota_lines[] =
        "{\"type\":\"session_meta\",\"payload\":{\"session_id\":\"session-quota\"}}\n"
        "{\"type\":\"event_msg\",\"ordinal\":1,\"timestamp\":\"t\",\"payload\":{\"type\":\"codex.rate_limits\",\"metered_limit_name\":null,\"limit_name\":null,\"rate_limits\":{\"primary\":{\"used_percent\":25.5,\"window_minutes\":300,\"reset_at\":1900000000},\"secondary\":{\"used_percent\":70,\"window_minutes\":10080,\"reset_at\":1900600000}}}}\n";
    static const char malformed_quota[] =
        "{\"type\":\"event_msg\",\"ordinal\":2,\"timestamp\":\"t\",\"payload\":{\"type\":\"codex.rate_limits\",\"rate_limits\":{\"primary\":{\"used_percent\":\"25\",\"window_minutes\":300,\"reset_at\":1900000000}}}}\n";
    static const char token_only_quota[] =
        "{\"type\":\"event_msg\",\"ordinal\":3,\"timestamp\":\"t\",\"payload\":{\"type\":\"codex.rate_limits\",\"rate_limits\":{\"primary\":{\"used_tokens\":25,\"limit_tokens\":100,\"window_minutes\":300,\"reset_at\":1900000000}}}}\n";
    char path[128];
    char backup[128];
    char line_buffer[1024];
    rollout_watcher_t watcher;
    rollout_watcher_stats_t stats;
    event_capture_t capture = {0};

    make_test_paths(path, sizeof(path), backup, sizeof(backup));
    assert(rollout_watcher_init(&watcher, line_buffer, sizeof(line_buffer),
                                4096));
    write_bytes(path, quota_lines, strlen(quota_lines), false);
    write_bytes(path, malformed_quota, strlen(malformed_quota), true);
    write_bytes(path, token_only_quota, strlen(token_only_quota), true);
    assert(rollout_watcher_poll_with_quota(
               &watcher, path, capture_event, capture_quota,
               &capture, &stats) == ROLLOUT_WATCHER_OK);
    assert(capture.count == 0);
    assert(capture.quota_count == 1);
    assert(strcmp(capture.quota_updates[0].limit_id, "codex") == 0);
    assert(capture.quota_updates[0].has_primary);
    assert(capture.quota_updates[0].primary.used_percent == 25.5);
    assert(capture.quota_updates[0].primary.window_minutes == 300);
    assert(capture.quota_updates[0].primary.resets_at == 1900000000);
    assert(capture.quota_updates[0].has_secondary);
    assert(capture.quota_updates[0].secondary.used_percent == 70.0);
    assert(capture.quota_updates[0].secondary.window_minutes == 10080);
    assert(capture.quota_updates[0].secondary.resets_at == 1900600000);
    assert(stats.emitted_quota_updates == 1);
    /* Token counts without source-provided used_percent never infer quota. */
    assert(stats.invalid_quota_lines == 2);
    assert(unlink(path) == 0);
}

int main(void)
{
    test_incremental_partial_and_verified_event();
    test_unknown_events_and_malformed_input_fail_closed();
    test_oversized_line_discards_until_new_session_metadata();
    test_file_rotation_and_truncation_reset_identity();
    test_persisted_rate_limits_extracts_by_window_minutes();
    return 0;
}
