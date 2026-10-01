#include <assert.h>
#include <math.h>

#include "ambient_quota.h"

static ambient_quota_window_t quota_window(uint32_t duration,
                                           double used_percent,
                                           uint64_t reset_marker)
{
    ambient_quota_window_t window = {
        .duration_minutes = duration,
        .used_percent_present = true,
        .used_percent = used_percent,
        .reset_marker_present = true,
        .reset_marker = reset_marker,
    };
    return window;
}

static ambient_quota_snapshot_t snapshot_with(
    const ambient_quota_window_t *short_window,
    const ambient_quota_window_t *long_window)
{
    ambient_quota_snapshot_t snapshot = {0};

    if (short_window) {
        snapshot.has_short_window = true;
        snapshot.short_window = *short_window;
    }
    if (long_window) {
        snapshot.has_long_window = true;
        snapshot.long_window = *long_window;
    }
    return snapshot;
}

static void expect_update(ambient_quota_reset_detector_t *detector,
                          const ambient_quota_snapshot_t *snapshot,
                          uint8_t expected_mask)
{
    uint8_t mask = 0xff;
    assert(ambient_quota_reset_detector_update(detector, snapshot, &mask));
    assert(mask == expected_mask);
}

static void expect_short(ambient_quota_reset_detector_t *detector,
                         double used_percent,
                         uint64_t reset_marker,
                         uint8_t expected_mask)
{
    ambient_quota_window_t window = quota_window(
        300, used_percent, reset_marker);
    ambient_quota_snapshot_t snapshot = snapshot_with(&window, NULL);
    expect_update(detector, &snapshot, expected_mask);
}

int main(void)
{
    ambient_quota_window_t windows[] = {
        {.duration_minutes = 10080, .used_percent_present = true,
         .used_percent = 25.0, .reset_marker_present = true,
         .reset_marker = 500},
        {.duration_minutes = 300, .used_percent_present = true,
         .used_percent = 80.0, .reset_marker_present = true,
         .reset_marker = 100},
        {.duration_minutes = 60, .used_percent_present = true,
         .used_percent = 12.0},
    };
    ambient_quota_snapshot_t snapshot = {0};
    ambient_quota_snapshot_t prior = {
        .has_short_window = true,
        .short_window = {.duration_minutes = 123},
    };
    ambient_quota_window_t before = quota_window(300, 80.0, 100);
    ambient_quota_window_t after = before;
    ambient_quota_reset_detector_t detector;
    uint8_t mask = 0;

    assert(ambient_quota_normalize(windows, 3, 3, &snapshot)
           == AMBIENT_QUOTA_NORMALIZED);
    assert(snapshot.has_short_window && snapshot.has_long_window);
    assert(ambient_quota_find(&snapshot, 300)->used_percent == 80.0);
    assert(ambient_quota_find(&snapshot, 10080)->used_percent == 25.0);
    assert(ambient_quota_find(&snapshot, 60) == NULL);
    assert(ambient_quota_normalize(windows, 3, 2, &snapshot)
           == AMBIENT_QUOTA_INVALID);
    assert(ambient_quota_normalize(NULL, 1, 1, &snapshot)
           == AMBIENT_QUOTA_INVALID);

    ambient_quota_window_t duplicates[] = {windows[1], windows[1]};
    assert(ambient_quota_normalize(duplicates, 2, 2, &prior)
           == AMBIENT_QUOTA_INVALID);
    assert(prior.short_window.duration_minutes == 123);

    assert(ambient_quota_compare(&before, &after) == AMBIENT_QUOTA_MATCH);
    after.reset_marker++;
    assert(ambient_quota_compare(&before, &after)
           == AMBIENT_QUOTA_MARKER_CHANGED);
    after = before;
    after.used_percent = 81.0;
    assert(ambient_quota_compare(&before, &after)
           == AMBIENT_QUOTA_USAGE_CHANGED);
    after = before;
    after.used_percent = NAN;
    assert(ambient_quota_compare(&before, &after)
           == AMBIENT_QUOTA_NOT_COMPARABLE);
    after = before;
    after.duration_minutes = 10080;
    assert(ambient_quota_compare(&before, &after)
           == AMBIENT_QUOTA_NOT_COMPARABLE);
    assert(ambient_quota_compare(NULL, &after) == AMBIENT_QUOTA_UNAVAILABLE);

    /* The first valid snapshot only establishes a baseline. */
    assert(!ambient_quota_reset_detector_init(&detector, 0.0));
    assert(!ambient_quota_reset_detector_init(&detector, -1.0));
    assert(!ambient_quota_reset_detector_init(&detector, 100.01));
    assert(!ambient_quota_reset_detector_init(&detector, NAN));
    assert(!ambient_quota_reset_detector_init(&detector, INFINITY));
    assert(ambient_quota_reset_detector_init(&detector, 100.0));
    assert(ambient_quota_reset_detector_init(&detector, AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT));
    expect_short(&detector, 80.0, 10, 0);

    /* Unavailable/missing fields discard state; availability re-baselines. */
    expect_update(&detector, NULL, 0);
    expect_short(&detector, 5.0, 11, 0);
    snapshot = (ambient_quota_snapshot_t){0};
    expect_update(&detector, &snapshot, 0);
    ambient_quota_window_t missing_used = quota_window(300, 0.0, 12);
    missing_used.used_percent_present = false;
    snapshot = snapshot_with(&missing_used, NULL);
    expect_update(&detector, &snapshot, 0);
    expect_short(&detector, 90.0, 13, 0);
    ambient_quota_window_t missing_marker = quota_window(300, 10.0, 14);
    missing_marker.reset_marker_present = false;
    snapshot = snapshot_with(&missing_marker, NULL);
    expect_update(&detector, &snapshot, 0);
    expect_short(&detector, 10.0, 14, 0);
    ambient_quota_window_t mismatched_window = quota_window(60, 0.0, 15);
    snapshot = snapshot_with(&mismatched_window, NULL);
    expect_update(&detector, &snapshot, 0);
    expect_short(&detector, 0.0, 15, 0);

    /* A large usage drop without a later reset marker is not a reset. */
    expect_short(&detector, 80.0, 20, 0);
    expect_short(&detector, 40.0, 20, 0);
    expect_short(&detector, 10.0, 21, 0);

    /* Marker advance without a material drop is not a reset. */
    expect_short(&detector, 80.0, 30, 0);
    expect_short(&detector, 70.0, 31, 0);

    /* Both signals create a candidate only; the matching next read confirms. */
    expect_short(&detector, 80.0, 40, 0);
    expect_short(&detector, 60.0, 41, 0);
    expect_short(&detector, 65.0, 41, AMBIENT_QUOTA_RESET_CONFIRMED_SHORT);
    expect_short(&detector, 65.0, 41, 0);
    expect_short(&detector, 65.0, 41, 0);

    /* A candidate is cancelled if the next valid read loses the material drop. */
    expect_short(&detector, 80.0, 50, 0);
    expect_short(&detector, 50.0, 51, 0);
    expect_short(&detector, 70.0, 51, 0);
    expect_short(&detector, 50.0, 51, 0);

    /* Missing data also cancels a candidate and requires re-baselining. */
    expect_short(&detector, 80.0, 60, 0);
    expect_short(&detector, 50.0, 61, 0);
    snapshot = (ambient_quota_snapshot_t){0};
    expect_update(&detector, &snapshot, 0);
    expect_short(&detector, 50.0, 61, 0);
    expect_short(&detector, 50.0, 61, 0);

    /* Marker regression, usage increase, and an insufficient drop are inert. */
    expect_short(&detector, 80.0, 70, 0);
    expect_short(&detector, 40.0, 69, 0);
    expect_short(&detector, 10.0, 70, 0);
    expect_short(&detector, 5.0, 71, 0);
    expect_short(&detector, 40.0, 80, 0);
    expect_short(&detector, 55.0, 81, 0);
    expect_short(&detector, 50.0, 90, 0);
    expect_short(&detector, 40.0, 91, 0);

    /* The 300- and 10080-minute windows are tracked and confirmed separately. */
    ambient_quota_window_t short_window = quota_window(300, 80.0, 100);
    ambient_quota_window_t long_window = quota_window(10080, 90.0, 200);
    snapshot = snapshot_with(&short_window, &long_window);
    expect_update(&detector, &snapshot, 0);
    short_window = quota_window(300, 50.0, 101);
    long_window = quota_window(10080, 70.0, 201);
    snapshot = snapshot_with(&short_window, &long_window);
    expect_update(&detector, &snapshot, 0);
    short_window = quota_window(300, 55.0, 101);
    long_window = quota_window(10080, 72.0, 201);
    snapshot = snapshot_with(&short_window, &long_window);
    expect_update(&detector, &snapshot,
                  AMBIENT_QUOTA_RESET_CONFIRMED_SHORT
                      | AMBIENT_QUOTA_RESET_CONFIRMED_LONG);
    expect_update(&detector, &snapshot, 0);

    /* Supported usage values are finite inclusive [0,100], atomic on rejection. */
    const double invalid_usage[] = {NAN, INFINITY, -INFINITY, -0.01, 100.01};
    for (size_t i = 0; i < sizeof(invalid_usage)/sizeof(invalid_usage[0]); i++) {
        ambient_quota_window_t invalid = quota_window(300, invalid_usage[i], 301);
        prior = snapshot_with(&short_window, NULL);
        double saved = prior.short_window.used_percent;
        assert(ambient_quota_normalize(&invalid, 1, 1, &prior) == AMBIENT_QUOTA_INVALID);
        assert(prior.short_window.used_percent == saved);
        snapshot = snapshot_with(&invalid, NULL);
        expect_update(&detector, &snapshot, 0);
        assert(!detector.short_window.has_baseline && !detector.short_window.has_candidate);
        expect_short(&detector, 80.0, 300, 0);
    }
    assert(ambient_quota_reset_detector_init(&detector, AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT));
    expect_short(&detector, 100.0, 1000, 0);
    expect_short(&detector, 85.01, 1001, 0); /* 14.99pp below threshold */
    assert(!detector.short_window.has_candidate);
    expect_short(&detector, 100.0, 1002, 0);
    expect_short(&detector, 85.0, 1003, 0); /* exact15pp candidate */
    assert(detector.short_window.has_candidate);
    expect_short(&detector, 85.0, 1003, AMBIENT_QUOTA_RESET_CONFIRMED_SHORT);
    expect_short(&detector, 0.0, 1002, 0); /* regressed marker cannot confirm */
    expect_short(&detector, 0.0, 1003, 0);
    assert(!detector.short_window.has_candidate);

    mask = 0xff;
    assert(!ambient_quota_reset_detector_init(&detector, 101.0));
    assert(!ambient_quota_reset_detector_update(&detector, &snapshot, &mask));
    assert(mask == 0);
    assert(!ambient_quota_reset_detector_update(&detector, &snapshot, NULL));
    return 0;
}
