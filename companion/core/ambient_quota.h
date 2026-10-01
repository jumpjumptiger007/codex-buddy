#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ambient_model.h"

#define AMBIENT_QUOTA_SHORT_MINUTES 300U
#define AMBIENT_QUOTA_LONG_MINUTES 10080U
#define AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT 15.0

typedef enum {
    AMBIENT_QUOTA_INVALID = 0,
    AMBIENT_QUOTA_NORMALIZED,
} ambient_quota_result_t;

typedef enum {
    AMBIENT_QUOTA_UNAVAILABLE = 0,
    AMBIENT_QUOTA_NOT_COMPARABLE,
    AMBIENT_QUOTA_MATCH,
    AMBIENT_QUOTA_MARKER_CHANGED,
    AMBIENT_QUOTA_USAGE_CHANGED,
} ambient_quota_comparison_t;

typedef struct {
    bool has_baseline;
    double baseline_used_percent;
    uint64_t baseline_reset_marker;
    /* Prevent marker recovery/regression from looking like a fresh advance. */
    uint64_t highest_reset_marker;
    bool has_candidate;
    uint64_t candidate_reset_marker;
} ambient_quota_reset_window_state_t;

typedef struct {
    /* Percentage points; must be finite and in (0, 100]. */
    double minimum_drop_percent;
    ambient_quota_reset_window_state_t short_window;
    ambient_quota_reset_window_state_t long_window;
} ambient_quota_reset_detector_t;

enum {
    AMBIENT_QUOTA_RESET_CONFIRMED_SHORT = 1u << 0,
    AMBIENT_QUOTA_RESET_CONFIRMED_LONG = 1u << 1,
};

ambient_quota_result_t ambient_quota_normalize(
    const ambient_quota_window_t *windows,
    size_t window_count,
    size_t window_capacity,
    ambient_quota_snapshot_t *snapshot);

/* Field comparison only; marker inequality is not a reset determination. */
ambient_quota_comparison_t ambient_quota_compare(
    const ambient_quota_window_t *before,
    const ambient_quota_window_t *after);

/* Candidate transitions are internal; the mask reports confirmed resets only. */
bool ambient_quota_reset_detector_init(
    ambient_quota_reset_detector_t *detector,
    double minimum_drop_percent);

/* A null snapshot means unavailable data and discards both window baselines. */
bool ambient_quota_reset_detector_update(
    ambient_quota_reset_detector_t *detector,
    const ambient_quota_snapshot_t *snapshot,
    uint8_t *confirmed_windows_mask);

const ambient_quota_window_t *ambient_quota_find(
    const ambient_quota_snapshot_t *snapshot,
    uint32_t duration_minutes);
