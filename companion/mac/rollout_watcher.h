#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ambient_model.h"
#include "rollout_quota_source.h"

#define ROLLOUT_WATCHER_SESSION_ID_CAPACITY 128U
#define ROLLOUT_WATCHER_MAX_LINE_BYTES (1024U * 1024U)
#define ROLLOUT_WATCHER_MAX_READ_BUDGET_BYTES (1024U * 1024U)

typedef struct {
    ambient_event_kind_t kind;
    const char *session_id;
    size_t session_id_length;
    const char *turn_id;
    size_t turn_id_length;
    uint64_t source_ordinal;
} rollout_lifecycle_input_t;

/* Identifier pointers are valid only for the duration of the callback. */
typedef void (*rollout_watcher_event_fn)(
    void *context,
    const rollout_lifecycle_input_t *event);

/* Quota input pointers are valid only for the duration of the callback. */
typedef void (*rollout_watcher_quota_fn)(
    void *context,
    const rollout_rate_limits_input_t *rate_limits);

typedef struct {
    size_t bytes_read;
    size_t complete_lines;
    size_t malformed_lines;
    size_t oversized_lines;
    size_t invalid_event_lines;
    size_t unverified_event_lines;
    size_t invalid_quota_lines;
    size_t ignored_lines;
    size_t emitted_events;
    size_t emitted_quota_updates;
    bool file_changed;
    bool file_truncated;
    bool partial_line_pending;
    bool read_budget_exhausted;
} rollout_watcher_stats_t;

typedef enum {
    ROLLOUT_WATCHER_OK = 0,
    ROLLOUT_WATCHER_INVALID_ARGUMENT,
    ROLLOUT_WATCHER_IO_ERROR,
} rollout_watcher_result_t;

typedef struct {
    char *line_buffer;
    size_t line_capacity;
    size_t line_length;
    size_t read_budget_bytes;
    uint64_t file_device;
    uint64_t file_inode;
    int64_t read_offset;
    bool has_file_identity;
    bool discarding_oversized_line;
    bool has_session_id;
    char session_id[ROLLOUT_WATCHER_SESSION_ID_CAPACITY];
    size_t session_id_length;
} rollout_watcher_t;

bool rollout_watcher_init(rollout_watcher_t *watcher,
                          char *line_buffer,
                          size_t line_capacity,
                          size_t read_budget_bytes);

void rollout_watcher_reset(rollout_watcher_t *watcher);

rollout_watcher_result_t rollout_watcher_poll(
    rollout_watcher_t *watcher,
    const char *path,
    rollout_watcher_event_fn on_event,
    void *context,
    rollout_watcher_stats_t *stats);

rollout_watcher_result_t rollout_watcher_poll_with_quota(
    rollout_watcher_t *watcher,
    const char *path,
    rollout_watcher_event_fn on_event,
    rollout_watcher_quota_fn on_quota,
    void *context,
    rollout_watcher_stats_t *stats);
