#include "ambient_quota.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

enum {
    AMBIENT_SHORT_WINDOW_MINUTES = AMBIENT_QUOTA_SHORT_MINUTES,
    AMBIENT_LONG_WINDOW_MINUTES = AMBIENT_QUOTA_LONG_MINUTES,
};

static bool ambient_quota_window_is_valid(const ambient_quota_window_t *window)
{
    return !window->used_percent_present
        || (window->used_percent >= 0.0 && window->used_percent <= 100.0);
}

static bool ambient_quota_reset_observation_is_valid(
    const ambient_quota_window_t *window,
    uint32_t expected_duration)
{
    return window && window->duration_minutes == expected_duration
        && window->used_percent_present
        && ambient_quota_window_is_valid(window)
        && window->reset_marker_present;
}

static bool ambient_quota_drop_is_material(double before,
                                           double after,
                                           double minimum_drop_percent)
{
    return before > after && before - after >= minimum_drop_percent;
}

static void ambient_quota_reset_window_clear(
    ambient_quota_reset_window_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

static void ambient_quota_reset_window_set_baseline(
    ambient_quota_reset_window_state_t *state,
    const ambient_quota_window_t *window)
{
    state->has_baseline = true;
    state->baseline_used_percent = window->used_percent;
    if (window->reset_marker > state->highest_reset_marker) {
        state->highest_reset_marker = window->reset_marker;
    }
    state->baseline_reset_marker = state->highest_reset_marker;
    state->has_candidate = false;
    state->candidate_reset_marker = 0;
}

static bool ambient_quota_reset_window_update(
    ambient_quota_reset_window_state_t *state,
    const ambient_quota_window_t *window,
    uint32_t expected_duration,
    double minimum_drop_percent)
{
    bool confirmed = false;

    if (!ambient_quota_reset_observation_is_valid(window, expected_duration)) {
        ambient_quota_reset_window_clear(state);
        return false;
    }
    if (!state->has_baseline) {
        ambient_quota_reset_window_set_baseline(state, window);
        return false;
    }

    if (state->has_candidate) {
        confirmed = window->reset_marker == state->candidate_reset_marker
            && window->reset_marker > state->baseline_reset_marker
            && ambient_quota_drop_is_material(
                state->baseline_used_percent, window->used_percent,
                minimum_drop_percent);
        if (window->reset_marker > state->highest_reset_marker) {
            state->highest_reset_marker = window->reset_marker;
        }
        ambient_quota_reset_window_set_baseline(state, window);
        return confirmed;
    }

    if (window->reset_marker > state->highest_reset_marker
        && ambient_quota_drop_is_material(
            state->baseline_used_percent, window->used_percent,
            minimum_drop_percent)) {
        state->highest_reset_marker = window->reset_marker;
        state->has_candidate = true;
        state->candidate_reset_marker = window->reset_marker;
        return false;
    }

    /* Rebaseline on same-boundary changes, increases, or marker regression. */
    ambient_quota_reset_window_set_baseline(state, window);
    return false;
}

ambient_quota_result_t ambient_quota_normalize(
    const ambient_quota_window_t *windows,
    size_t window_count,
    size_t window_capacity,
    ambient_quota_snapshot_t *snapshot)
{
    ambient_quota_snapshot_t normalized = {0};

    if (!snapshot || window_count > window_capacity
        || (window_count > 0 && !windows)) {
        return AMBIENT_QUOTA_INVALID;
    }

    for (size_t i = 0; i < window_count; ++i) {
        const ambient_quota_window_t *window = &windows[i];
        if (window->duration_minutes != AMBIENT_SHORT_WINDOW_MINUTES
            && window->duration_minutes != AMBIENT_LONG_WINDOW_MINUTES) {
            continue;
        }
        if (!ambient_quota_window_is_valid(window)) {
            return AMBIENT_QUOTA_INVALID;
        }

        if (window->duration_minutes == AMBIENT_SHORT_WINDOW_MINUTES) {
            if (normalized.has_short_window) {
                return AMBIENT_QUOTA_INVALID;
            }
            normalized.short_window = *window;
            normalized.has_short_window = true;
        } else {
            if (normalized.has_long_window) {
                return AMBIENT_QUOTA_INVALID;
            }
            normalized.long_window = *window;
            normalized.has_long_window = true;
        }
    }
    *snapshot = normalized;
    return AMBIENT_QUOTA_NORMALIZED;
}

ambient_quota_comparison_t ambient_quota_compare(
    const ambient_quota_window_t *before,
    const ambient_quota_window_t *after)
{
    if (!before || !after) {
        return AMBIENT_QUOTA_UNAVAILABLE;
    }
    if ((before->duration_minutes != AMBIENT_SHORT_WINDOW_MINUTES
         && before->duration_minutes != AMBIENT_LONG_WINDOW_MINUTES)
        || before->duration_minutes != after->duration_minutes) {
        return AMBIENT_QUOTA_NOT_COMPARABLE;
    }
    if (!ambient_quota_window_is_valid(before)
        || !ambient_quota_window_is_valid(after)
        || !before->used_percent_present || !after->used_percent_present
        || !before->reset_marker_present || !after->reset_marker_present) {
        return AMBIENT_QUOTA_NOT_COMPARABLE;
    }
    if (before->reset_marker != after->reset_marker) {
        return AMBIENT_QUOTA_MARKER_CHANGED;
    }
    if (before->used_percent != after->used_percent) {
        return AMBIENT_QUOTA_USAGE_CHANGED;
    }
    return AMBIENT_QUOTA_MATCH;
}

bool ambient_quota_reset_detector_init(
    ambient_quota_reset_detector_t *detector,
    double minimum_drop_percent)
{
    if (!detector) {
        return false;
    }
    memset(detector, 0, sizeof(*detector));
    if (!isfinite(minimum_drop_percent)
        || minimum_drop_percent <= 0.0 || minimum_drop_percent > 100.0) {
        return false;
    }

    detector->minimum_drop_percent = minimum_drop_percent;
    return true;
}

bool ambient_quota_reset_detector_update(
    ambient_quota_reset_detector_t *detector,
    const ambient_quota_snapshot_t *snapshot,
    uint8_t *confirmed_windows_mask)
{
    const ambient_quota_window_t *short_window = NULL;
    const ambient_quota_window_t *long_window = NULL;
    uint8_t mask = 0;

    if (!confirmed_windows_mask) {
        return false;
    }
    *confirmed_windows_mask = 0;
    if (!detector || !isfinite(detector->minimum_drop_percent)
        || detector->minimum_drop_percent <= 0.0
        || detector->minimum_drop_percent > 100.0) {
        return false;
    }

    if (snapshot) {
        if (snapshot->has_short_window) {
            short_window = &snapshot->short_window;
        }
        if (snapshot->has_long_window) {
            long_window = &snapshot->long_window;
        }
    }

    if (ambient_quota_reset_window_update(
            &detector->short_window, short_window,
            AMBIENT_SHORT_WINDOW_MINUTES, detector->minimum_drop_percent)) {
        mask |= AMBIENT_QUOTA_RESET_CONFIRMED_SHORT;
    }
    if (ambient_quota_reset_window_update(
            &detector->long_window, long_window,
            AMBIENT_LONG_WINDOW_MINUTES, detector->minimum_drop_percent)) {
        mask |= AMBIENT_QUOTA_RESET_CONFIRMED_LONG;
    }
    *confirmed_windows_mask = mask;
    return true;
}

const ambient_quota_window_t *ambient_quota_find(
    const ambient_quota_snapshot_t *snapshot,
    uint32_t duration_minutes)
{
    if (!snapshot) {
        return NULL;
    }
    if (duration_minutes == AMBIENT_SHORT_WINDOW_MINUTES
        && snapshot->has_short_window) {
        return &snapshot->short_window;
    }
    if (duration_minutes == AMBIENT_LONG_WINDOW_MINUTES
        && snapshot->has_long_window) {
        return &snapshot->long_window;
    }
    return NULL;
}
