#include "rollout_quota_source.h"

#include <math.h>
#include <string.h>

static bool rollout_quota_add_window(
    const rollout_quota_window_input_t *input,
    int64_t now_unix_seconds,
    ambient_quota_window_t *windows,
    size_t *window_count)
{
    ambient_quota_window_t *window;

    if (!input || !windows || !window_count) {
        return false;
    }
    if (!input->used_percent_present || !input->window_minutes_present
        || !input->resets_at_present || input->window_minutes <= 0
        || input->resets_at <= now_unix_seconds) {
        return true;
    }
    if (input->window_minutes != 300 && input->window_minutes != 10080) {
        return true;
    }
    if (*window_count >= 2) {
        return false;
    }

    window = &windows[(*window_count)++];
    window->duration_minutes = (uint32_t)input->window_minutes;
    window->used_percent_present = true;
    window->used_percent = input->used_percent;
    window->reset_marker_present = true;
    window->reset_marker = (uint64_t)input->resets_at;
    return true;
}

static void rollout_quota_clear_detector(rollout_quota_source_t *source,
                                         uint8_t *confirmed_mask)
{
    if (!source || !confirmed_mask) {
        return;
    }
    (void)ambient_quota_reset_detector_update(
        &source->reset_detector, NULL, confirmed_mask);
    memset(&source->latest_snapshot, 0, sizeof(source->latest_snapshot));
    source->has_snapshot = false;
}

bool rollout_quota_source_init(rollout_quota_source_t *source,
                               double minimum_reset_drop_percent)
{
    if (!source) {
        return false;
    }
    memset(source, 0, sizeof(*source));
    return ambient_quota_reset_detector_init(
        &source->reset_detector, minimum_reset_drop_percent);
}

rollout_quota_result_t rollout_quota_source_update(
    rollout_quota_source_t *source,
    const rollout_rate_limits_input_t *rate_limits,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot,
    uint8_t *confirmed_reset_windows_mask)
{
    ambient_quota_window_t windows[2] = {0};
    ambient_quota_snapshot_t normalized = {0};
    size_t window_count = 0;
    uint8_t confirmed_mask = 0;

    if (snapshot) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
    if (confirmed_reset_windows_mask) {
        *confirmed_reset_windows_mask = 0;
    }
    if (!source || !snapshot || !confirmed_reset_windows_mask
        || now_unix_seconds < 0
        || !isfinite(source->reset_detector.minimum_drop_percent)
        || source->reset_detector.minimum_drop_percent <= 0.0
        || source->reset_detector.minimum_drop_percent > 100.0) {
        return ROLLOUT_QUOTA_INVALID;
    }

    if (!rate_limits) {
        rollout_quota_clear_detector(source, confirmed_reset_windows_mask);
        return ROLLOUT_QUOTA_UNAVAILABLE;
    }

    size_t limit_id_length = 0;
    while (limit_id_length < sizeof(rate_limits->limit_id)
           && rate_limits->limit_id[limit_id_length] != '\0') {
        limit_id_length++;
    }
    if (limit_id_length == sizeof(rate_limits->limit_id)) {
        rollout_quota_clear_detector(source, confirmed_reset_windows_mask);
        return ROLLOUT_QUOTA_INVALID;
    }
    if (strcmp(rate_limits->limit_id, "codex") != 0) {
        return ROLLOUT_QUOTA_IGNORED;
    }

    if ((rate_limits->has_primary
         && !rollout_quota_add_window(&rate_limits->primary,
                                      now_unix_seconds,
                                      windows, &window_count))
        || (rate_limits->has_secondary
            && !rollout_quota_add_window(&rate_limits->secondary,
                                         now_unix_seconds,
                                         windows, &window_count))) {
        rollout_quota_clear_detector(source, confirmed_reset_windows_mask);
        return ROLLOUT_QUOTA_INVALID;
    }

    if (ambient_quota_normalize(windows, window_count,
                                sizeof(windows) / sizeof(windows[0]),
                                &normalized) != AMBIENT_QUOTA_NORMALIZED) {
        rollout_quota_clear_detector(source, confirmed_reset_windows_mask);
        return ROLLOUT_QUOTA_INVALID;
    }
    if (!ambient_quota_reset_detector_update(
            &source->reset_detector, &normalized, &confirmed_mask)) {
        memset(&source->latest_snapshot, 0, sizeof(source->latest_snapshot));
        source->has_snapshot = false;
        return ROLLOUT_QUOTA_INVALID;
    }
    source->latest_snapshot = normalized;
    source->has_snapshot = normalized.has_short_window
        || normalized.has_long_window;
    *snapshot = normalized;
    *confirmed_reset_windows_mask = confirmed_mask;
    if (!normalized.has_short_window && !normalized.has_long_window) {
        return ROLLOUT_QUOTA_UNAVAILABLE;
    }
    return ROLLOUT_QUOTA_AVAILABLE;
}

rollout_quota_result_t rollout_quota_source_current(
    rollout_quota_source_t *source,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot)
{
    if (snapshot) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
    if (!source || !snapshot || now_unix_seconds < 0) {
        return ROLLOUT_QUOTA_INVALID;
    }
    if (!source->has_snapshot) {
        return ROLLOUT_QUOTA_UNAVAILABLE;
    }

    if (source->latest_snapshot.has_short_window
        && (!source->latest_snapshot.short_window.reset_marker_present
            || source->latest_snapshot.short_window.reset_marker
                   <= (uint64_t)now_unix_seconds)) {
        source->latest_snapshot.has_short_window = false;
        memset(&source->latest_snapshot.short_window, 0,
               sizeof(source->latest_snapshot.short_window));
        memset(&source->reset_detector.short_window, 0,
               sizeof(source->reset_detector.short_window));
    }
    if (source->latest_snapshot.has_long_window
        && (!source->latest_snapshot.long_window.reset_marker_present
            || source->latest_snapshot.long_window.reset_marker
                   <= (uint64_t)now_unix_seconds)) {
        source->latest_snapshot.has_long_window = false;
        memset(&source->latest_snapshot.long_window, 0,
               sizeof(source->latest_snapshot.long_window));
        memset(&source->reset_detector.long_window, 0,
               sizeof(source->reset_detector.long_window));
    }
    if (!source->latest_snapshot.has_short_window
        && !source->latest_snapshot.has_long_window) {
        memset(&source->latest_snapshot, 0, sizeof(source->latest_snapshot));
        source->has_snapshot = false;
        return ROLLOUT_QUOTA_UNAVAILABLE;
    }

    *snapshot = source->latest_snapshot;
    return ROLLOUT_QUOTA_AVAILABLE;
}
