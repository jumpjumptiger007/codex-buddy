#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "quota_reset_state_store.h"
#include "rollout_quota_source.h"

static rollout_quota_window_input_t short_window(double used,
                                                 int64_t resets_at)
{
    rollout_quota_window_input_t input = {
        .used_percent_present = true,
        .used_percent = used,
        .window_minutes_present = true,
        .window_minutes = 300,
        .resets_at_present = true,
        .resets_at = resets_at,
    };
    return input;
}

static rollout_rate_limits_input_t quota(double used, int64_t resets_at)
{
    rollout_rate_limits_input_t input = {0};
    memcpy(input.limit_id, "codex", sizeof("codex"));
    input.has_primary = true;
    input.primary = short_window(used, resets_at);
    return input;
}

static void make_test_directory(char *path)
{
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(path) == 0);
    assert(mkdir(path, 0700) == 0);
}

static void test_missing_and_corrupt_state_rebaseline(void)
{
    char directory[] = "/tmp/quota-reset-state.XXXXXX";
    char path[256];
    ambient_quota_reset_detector_t detector;
    int fd;

    make_test_directory(directory);
    assert(snprintf(path, sizeof(path), "%s/state.bin", directory) > 0);
    assert(ambient_quota_reset_detector_init(&detector, 15.0));
    detector.short_window.has_baseline = true;
    detector.short_window.baseline_used_percent = 80.0;
    detector.short_window.baseline_reset_marker = 1000;
    detector.short_window.highest_reset_marker = 1000;
    assert(quota_reset_state_store_load(path, &detector)
           == QUOTA_RESET_STATE_STORE_NOT_FOUND);
    assert(!detector.short_window.has_baseline);
    assert(detector.minimum_drop_percent == 15.0);

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    assert(write(fd, "corrupt", 7) == 7);
    assert(close(fd) == 0);
    assert(ambient_quota_reset_detector_init(&detector, 15.0));
    detector.long_window.has_baseline = true;
    detector.long_window.baseline_used_percent = 44.0;
    detector.long_window.baseline_reset_marker = 1000;
    detector.long_window.highest_reset_marker = 1000;
    assert(quota_reset_state_store_load(path, &detector)
           == QUOTA_RESET_STATE_STORE_CORRUPT);
    assert(!detector.short_window.has_baseline);
    assert(!detector.long_window.has_baseline);
    assert(detector.minimum_drop_percent == 15.0);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}

static void test_reset_candidate_and_confirmation_survive_restart(void)
{
    char directory[] = "/tmp/quota-reset-restart.XXXXXX";
    char path[256];
    rollout_quota_source_t first;
    rollout_quota_source_t restarted;
    rollout_quota_source_t after_confirmation;
    rollout_rate_limits_input_t input;
    ambient_quota_snapshot_t snapshot;
    uint8_t reset_mask;
    struct stat file_status;

    make_test_directory(directory);
    assert(snprintf(path, sizeof(path), "%s/state.bin", directory) > 0);
    assert(rollout_quota_source_init(&first, 15.0));
    input = quota(80.0, 1000);
    assert(rollout_quota_source_update(&first, &input, 900,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    input = quota(50.0, 1100);
    assert(rollout_quota_source_update(&first, &input, 950,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(first.reset_detector.short_window.has_candidate);
    assert(quota_reset_state_store_save_atomic(path, &first.reset_detector)
           == QUOTA_RESET_STATE_STORE_OK);
    assert(stat(path, &file_status) == 0);
    assert((file_status.st_mode & 0777) == 0600);

    assert(rollout_quota_source_init(&restarted, 15.0));
    assert(quota_reset_state_store_load(path, &restarted.reset_detector)
           == QUOTA_RESET_STATE_STORE_OK);
    assert(restarted.reset_detector.short_window.has_candidate);
    input = quota(55.0, 1100);
    assert(rollout_quota_source_update(&restarted, &input, 960,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == AMBIENT_QUOTA_RESET_CONFIRMED_SHORT);
    assert(quota_reset_state_store_save_atomic(path,
                                                &restarted.reset_detector)
           == QUOTA_RESET_STATE_STORE_OK);

    assert(rollout_quota_source_init(&after_confirmation, 15.0));
    assert(quota_reset_state_store_load(path,
                                         &after_confirmation.reset_detector)
           == QUOTA_RESET_STATE_STORE_OK);
    assert(rollout_quota_source_update(&after_confirmation, &input, 970,
                                       &snapshot, &reset_mask)
           == ROLLOUT_QUOTA_AVAILABLE);
    assert(reset_mask == 0);
    assert(!after_confirmation.reset_detector.short_window.has_candidate);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}

static void test_checksum_corruption_rebaselines(void)
{
    char directory[] = "/tmp/quota-reset-corrupt.XXXXXX";
    char path[256];
    ambient_quota_reset_detector_t detector;
    uint8_t byte;
    int fd;

    make_test_directory(directory);
    assert(snprintf(path, sizeof(path), "%s/state.bin", directory) > 0);
    assert(ambient_quota_reset_detector_init(&detector, 15.0));
    detector.short_window.has_baseline = true;
    detector.short_window.baseline_used_percent = 40.0;
    detector.short_window.baseline_reset_marker = 100;
    detector.short_window.highest_reset_marker = 100;
    assert(quota_reset_state_store_save_atomic(path, &detector)
           == QUOTA_RESET_STATE_STORE_OK);
    fd = open(path, O_RDWR);
    assert(fd >= 0);
    assert(pread(fd, &byte, 1, 24) == 1);
    byte ^= 1U;
    assert(pwrite(fd, &byte, 1, 24) == 1);
    assert(close(fd) == 0);
    assert(quota_reset_state_store_load(path, &detector)
           == QUOTA_RESET_STATE_STORE_CORRUPT);
    assert(!detector.short_window.has_baseline);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
}

static void test_write_failure_and_recovery(void)
{
    char directory[] = "/tmp/quota-reset-failure.XXXXXX", bad_path[256], good_path[256];
    make_test_directory(directory);
    assert(snprintf(bad_path, sizeof(bad_path), "%s/missing/state.bin", directory) > 0);
    assert(snprintf(good_path, sizeof(good_path), "%s/state.bin", directory) > 0);
    ambient_quota_reset_detector_t detector, loaded;
    assert(ambient_quota_reset_detector_init(&detector, AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT));
    assert(quota_reset_state_store_save_atomic(bad_path, &detector) == QUOTA_RESET_STATE_STORE_IO_ERROR);
    assert(quota_reset_state_store_save_atomic(directory, &detector) == QUOTA_RESET_STATE_STORE_IO_ERROR);
    assert(quota_reset_state_store_save_atomic(good_path, &detector) == QUOTA_RESET_STATE_STORE_OK);
    assert(ambient_quota_reset_detector_init(&loaded, AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT));
    assert(quota_reset_state_store_load(good_path, &loaded) == QUOTA_RESET_STATE_STORE_OK);
    assert(!loaded.short_window.has_baseline && !loaded.long_window.has_baseline);
    assert(unlink(good_path) == 0); assert(rmdir(directory) == 0);
}

int main(void)
{
    test_write_failure_and_recovery();
    test_missing_and_corrupt_state_rebaseline();
    test_reset_candidate_and_confirmation_survive_restart();
    test_checksum_corruption_rebaselines();
    return 0;
}
