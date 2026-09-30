#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ambient_quota.h"

#define ROLLOUT_QUOTA_LIMIT_ID_CAPACITY 64U

typedef struct {
    bool used_percent_present;
    double used_percent;
    bool window_minutes_present;
    int64_t window_minutes;
    bool resets_at_present;
    int64_t resets_at;
} rollout_quota_window_input_t;

/* Decoded fields from the persisted Codex `rate_limits` object. */
typedef struct {
    char limit_id[ROLLOUT_QUOTA_LIMIT_ID_CAPACITY];
    bool has_primary;
    rollout_quota_window_input_t primary;
    bool has_secondary;
    rollout_quota_window_input_t secondary;
} rollout_rate_limits_input_t;

typedef enum {
    ROLLOUT_QUOTA_INVALID = 0,
    ROLLOUT_QUOTA_UNAVAILABLE,
    ROLLOUT_QUOTA_AVAILABLE,
    ROLLOUT_QUOTA_IGNORED,
} rollout_quota_result_t;

typedef struct {
    ambient_quota_reset_detector_t reset_detector;
    ambient_quota_snapshot_t latest_snapshot;
    bool has_snapshot;
} rollout_quota_source_t;

bool rollout_quota_source_init(rollout_quota_source_t *source,
                               double minimum_reset_drop_percent);

/* `now_unix_seconds` is supplied by the caller for deterministic freshness. */
rollout_quota_result_t rollout_quota_source_update(
    rollout_quota_source_t *source,
    const rollout_rate_limits_input_t *rate_limits,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot,
    uint8_t *confirmed_reset_windows_mask);

/* Re-check cached reset boundaries and expire matching detector state. */
rollout_quota_result_t rollout_quota_source_current(
    rollout_quota_source_t *source,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot);
