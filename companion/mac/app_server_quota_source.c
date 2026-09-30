#include "app_server_quota_source.h"

#include <string.h>

app_server_quota_result_t app_server_quota_source_read(
    const app_server_quota_source_t *source,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot)
{
    ambient_quota_window_t windows[2] = {0};
    ambient_quota_snapshot_t normalized = {0};
    size_t count = 0;

    if (snapshot) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
    if (!source || !snapshot || now_unix_seconds < 0) {
        return APP_SERVER_QUOTA_INVALID;
    }
    if (!source->read) {
        return APP_SERVER_QUOTA_UNAVAILABLE;
    }
    if (!source->read(source->context, now_unix_seconds, &normalized)) {
        return APP_SERVER_QUOTA_UNAVAILABLE;
    }

    if (normalized.has_short_window) {
        if (!normalized.short_window.reset_marker_present
            || normalized.short_window.reset_marker
                   <= (uint64_t)now_unix_seconds) {
            normalized.has_short_window = false;
        } else {
            windows[count++] = normalized.short_window;
        }
    }
    if (normalized.has_long_window) {
        if (!normalized.long_window.reset_marker_present
            || normalized.long_window.reset_marker
                   <= (uint64_t)now_unix_seconds) {
            normalized.has_long_window = false;
        } else {
            windows[count++] = normalized.long_window;
        }
    }
    if (ambient_quota_normalize(windows, count,
                                sizeof(windows) / sizeof(windows[0]),
                                snapshot) != AMBIENT_QUOTA_NORMALIZED) {
        memset(snapshot, 0, sizeof(*snapshot));
        return APP_SERVER_QUOTA_INVALID;
    }
    if (!snapshot->has_short_window && !snapshot->has_long_window) {
        return APP_SERVER_QUOTA_UNAVAILABLE;
    }
    return APP_SERVER_QUOTA_AVAILABLE;
}
