#include <assert.h>
#include <math.h>
#include <string.h>

#include "app_server_quota_source.h"
#include "rollout_quota_source.h"

static rollout_quota_window_input_t input_window(int64_t minutes,
                                                 double used_percent,
                                                 int64_t resets_at)
{
    rollout_quota_window_input_t window = {
        .used_percent_present = true,
        .used_percent = used_percent,
        .window_minutes_present = true,
        .window_minutes = minutes,
        .resets_at_present = true,
        .resets_at = resets_at,
    };
    return window;
}

static rollout_rate_limits_input_t codex_limits(
    rollout_quota_window_input_t primary,
    rollout_quota_window_input_t secondary)
{
    rollout_rate_limits_input_t limits = {0};
    memcpy(limits.limit_id, "codex", sizeof("codex"));
    limits.has_primary = true;
    limits.primary = primary;
    limits.has_secondary = true;
    limits.secondary = secondary;
    return limits;
}

static void test_extracts_supported_windows_and_ignores_other_limits(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.5, 1000),
        input_window(10080, 35.0, 2000));
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(snapshot.has_short_window && snapshot.has_long_window);
    assert(snapshot.short_window.duration_minutes == 300);
    assert(snapshot.short_window.used_percent == 80.5);
    assert(snapshot.short_window.reset_marker == 1000);
    assert(snapshot.long_window.duration_minutes == 10080);
    assert(snapshot.long_window.used_percent == 35.0);
    assert(snapshot.long_window.reset_marker == 2000);
    assert(reset_mask == 0);
    assert(rollout_quota_source_current(&source, 1000, &snapshot)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(!snapshot.has_short_window && snapshot.has_long_window);
    assert(!snapshot.short_window.used_percent_present);
    assert(rollout_quota_source_current(&source, 2000, &snapshot)
           == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);

    memcpy(limits.limit_id, "codex_other", sizeof("codex_other"));
    assert(rollout_quota_source_update(&source, &limits, 901,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_IGNORED);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);
    assert(reset_mask == 0);
    assert(!source.reset_detector.short_window.has_baseline);
}

static void test_stale_and_incomplete_windows_are_unavailable(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.0, 1000),
        input_window(10080, 40.0, 1100));
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 1000,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(!snapshot.has_short_window && snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline);

    limits.primary = input_window(300, 80.0, 1200);
    limits.secondary = input_window(10080, 40.0, 1300);
    limits.primary.resets_at_present = false;
    limits.secondary.used_percent_present = false;
    assert(rollout_quota_source_update(&source, &limits, 1100,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline);
    assert(!source.reset_detector.long_window.has_baseline);
    assert(reset_mask == 0);
}

static void test_expiry_then_reappearance_rebaselines_without_reset(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.0, 1000), (rollout_quota_window_input_t){0});
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == 0);
    assert(source.reset_detector.short_window.has_baseline);

    assert(rollout_quota_source_current(&source, 1000, &snapshot)
           == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!snapshot.has_short_window && !source.has_snapshot);
    assert(!source.latest_snapshot.has_short_window);
    assert(!source.reset_detector.short_window.has_baseline);

    limits.primary = input_window(300, 50.0, 1100);
    assert(rollout_quota_source_update(&source, &limits, 1001,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(snapshot.has_short_window);
    assert(reset_mask == 0);
    assert(source.reset_detector.short_window.has_baseline);
    assert(source.reset_detector.short_window.baseline_used_percent == 50.0);
    assert(!source.reset_detector.short_window.has_candidate);

    limits.primary.used_percent = 55.0;
    assert(rollout_quota_source_update(&source, &limits, 1002,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == 0);
    assert(!source.reset_detector.short_window.has_candidate);
}

static void test_expiring_short_window_preserves_long_window_state(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.0, 1000),
        input_window(10080, 70.0, 5000));
    ambient_quota_reset_window_state_t long_state;
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(snapshot.has_short_window && snapshot.has_long_window);
    assert(source.reset_detector.short_window.has_baseline);
    assert(source.reset_detector.long_window.has_baseline);
    long_state = source.reset_detector.long_window;

    assert(rollout_quota_source_current(&source, 1000, &snapshot)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(!snapshot.has_short_window && snapshot.has_long_window);
    assert(!source.latest_snapshot.has_short_window);
    assert(source.latest_snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline);
    assert(source.reset_detector.long_window.has_baseline);
    assert(source.reset_detector.long_window.baseline_used_percent
           == long_state.baseline_used_percent);
    assert(source.reset_detector.long_window.baseline_reset_marker
           == long_state.baseline_reset_marker);
    assert(source.reset_detector.long_window.highest_reset_marker
           == long_state.highest_reset_marker);
    assert(source.reset_detector.long_window.has_candidate
           == long_state.has_candidate);
    assert(source.reset_detector.long_window.candidate_reset_marker
           == long_state.candidate_reset_marker);
    assert(source.has_snapshot);
}

static void test_repeated_expiry_is_idempotent(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.0, 1000),
        input_window(10080, 70.0, 2000));
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(rollout_quota_source_current(&source, 2000, &snapshot)
           == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!source.has_snapshot);
    assert(!source.latest_snapshot.has_short_window
           && !source.latest_snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline
           && !source.reset_detector.long_window.has_baseline);

    assert(rollout_quota_source_current(&source, 2001, &snapshot)
           == ROLLOUT_QUOTA_UNAVAILABLE);
    assert(!source.has_snapshot);
    assert(!source.latest_snapshot.has_short_window
           && !source.latest_snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline
           && !source.reset_detector.long_window.has_baseline);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);
}

static void test_uses_gate1_reset_detector_and_fails_invalid_data_closed(void)
{
    rollout_quota_source_t source;
    ambient_quota_snapshot_t snapshot;
    rollout_rate_limits_input_t limits = codex_limits(
        input_window(300, 80.0, 1000),
        input_window(10080, 90.0, 2000));
    uint8_t reset_mask = 0xff;

    assert(rollout_quota_source_init(&source, 15.0));
    assert(rollout_quota_source_update(&source, &limits, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    limits.primary = input_window(300, 50.0, 1100);
    limits.secondary = input_window(10080, 70.0, 2100);
    assert(rollout_quota_source_update(&source, &limits, 950,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == 0);
    limits.primary.used_percent = 55.0;
    limits.secondary.used_percent = 72.0;
    assert(rollout_quota_source_update(&source, &limits, 960,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == (AMBIENT_QUOTA_RESET_CONFIRMED_SHORT
                          | AMBIENT_QUOTA_RESET_CONFIRMED_LONG));

    limits.primary.used_percent = NAN;
    assert(rollout_quota_source_update(&source, &limits, 970,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_INVALID);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);
    assert(!source.reset_detector.short_window.has_baseline);
    assert(reset_mask == 0);
}

typedef struct {
    ambient_quota_snapshot_t snapshot;
    bool available;
} fake_app_server_quota_t;

static bool fake_app_server_quota_read(void *context,
                                       int64_t now_unix_seconds,
                                       ambient_quota_snapshot_t *snapshot)
{
    fake_app_server_quota_t *fake = context;
    (void)now_unix_seconds;
    if (!fake || !snapshot || !fake->available) {
        return false;
    }
    *snapshot = fake->snapshot;
    return true;
}

static void test_app_server_interface_stays_unavailable_without_adapter(void)
{
    ambient_quota_snapshot_t snapshot;
    app_server_quota_source_t source = {0};
    fake_app_server_quota_t fake = {
        .available = true,
        .snapshot = {
            .has_short_window = true,
            .short_window = {
                .duration_minutes = 300,
                .used_percent_present = true,
                .used_percent = 25.0,
                .reset_marker_present = true,
                .reset_marker = 1200,
            },
        },
    };

    assert(app_server_quota_source_read(&source, 1000, &snapshot)
           == APP_SERVER_QUOTA_UNAVAILABLE);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);

    source.read = fake_app_server_quota_read;
    source.context = &fake;
    assert(app_server_quota_source_read(&source, 1000, &snapshot)
           == APP_SERVER_QUOTA_AVAILABLE);
    assert(snapshot.has_short_window && snapshot.short_window.used_percent == 25.0);
    assert(app_server_quota_source_read(&source, 1200, &snapshot)
           == APP_SERVER_QUOTA_UNAVAILABLE);
    assert(!snapshot.has_short_window && !snapshot.has_long_window);
}

int main(void)
{
    test_extracts_supported_windows_and_ignores_other_limits();
    test_stale_and_incomplete_windows_are_unavailable();
    test_expiry_then_reappearance_rebaselines_without_reset();
    test_expiring_short_window_preserves_long_window_state();
    test_repeated_expiry_is_idempotent();
    test_uses_gate1_reset_detector_and_fails_invalid_data_closed();
    test_app_server_interface_stays_unavailable_without_adapter();
    return 0;
}
