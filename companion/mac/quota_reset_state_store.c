#define _POSIX_C_SOURCE 200809L

#include "quota_reset_state_store.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "quota state requires 64-bit double storage");

enum {
    QUOTA_STATE_MAGIC_BYTES = 8,
    QUOTA_STATE_HEADER_BYTES = 12,
    QUOTA_STATE_WINDOW_BYTES = 33,
    QUOTA_STATE_CRC_BYTES = 4,
    QUOTA_STATE_FILE_BYTES = QUOTA_STATE_HEADER_BYTES
        + (2 * QUOTA_STATE_WINDOW_BYTES) + QUOTA_STATE_CRC_BYTES,
    QUOTA_STATE_PATH_MAX_BYTES = 4096,
};

static const uint8_t quota_state_magic[QUOTA_STATE_MAGIC_BYTES] = {
    'C', '2', 'C', 'Q', 'R', 'S', 'T', 'A'
};

static void quota_state_put_u64(uint8_t *destination, uint64_t value)
{
    for (size_t i = 0; i < 8; ++i) {
        destination[i] = (uint8_t)(value >> ((7U - (unsigned)i) * 8U));
    }
}

static uint64_t quota_state_get_u64(const uint8_t *source)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8) | source[i];
    }
    return value;
}

static uint32_t quota_state_crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint32_t low_bit = crc & 1U;
            crc >>= 1;
            if (low_bit) {
                crc ^= UINT32_C(0xedb88320);
            }
        }
    }
    return ~crc;
}

static void quota_state_put_window(
    uint8_t *destination,
    const ambient_quota_reset_window_state_t *state)
{
    uint64_t used_bits = 0;
    uint8_t flags = 0;

    if (state->has_baseline) {
        flags |= 1U;
    }
    if (state->has_candidate) {
        flags |= 2U;
    }
    destination[0] = flags;
    memcpy(&used_bits, &state->baseline_used_percent, sizeof(used_bits));
    quota_state_put_u64(destination + 1, used_bits);
    quota_state_put_u64(destination + 9, state->baseline_reset_marker);
    quota_state_put_u64(destination + 17, state->highest_reset_marker);
    quota_state_put_u64(destination + 25, state->candidate_reset_marker);
}

static bool quota_state_get_window(
    const uint8_t *source,
    ambient_quota_reset_window_state_t *state)
{
    uint8_t flags;
    uint64_t used_bits;

    if (!source || !state) {
        return false;
    }
    flags = source[0];
    if ((flags & (uint8_t)~3U) != 0) {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->has_baseline = (flags & 1U) != 0;
    state->has_candidate = (flags & 2U) != 0;
    used_bits = quota_state_get_u64(source + 1);
    memcpy(&state->baseline_used_percent, &used_bits, sizeof(used_bits));
    state->baseline_reset_marker = quota_state_get_u64(source + 9);
    state->highest_reset_marker = quota_state_get_u64(source + 17);
    state->candidate_reset_marker = quota_state_get_u64(source + 25);

    if (state->has_baseline) {
        if (!isfinite(state->baseline_used_percent)
            || state->baseline_used_percent < 0.0
            || state->baseline_used_percent > 100.0
            || state->baseline_reset_marker > state->highest_reset_marker) {
            return false;
        }
    } else if (state->baseline_used_percent != 0.0
               || state->baseline_reset_marker != 0
               || state->highest_reset_marker != 0
               || state->has_candidate
               || state->candidate_reset_marker != 0) {
        return false;
    }

    if (state->has_candidate) {
        if (state->candidate_reset_marker <= state->baseline_reset_marker
            || state->candidate_reset_marker != state->highest_reset_marker) {
            return false;
        }
    } else if (state->candidate_reset_marker != 0) {
        return false;
    }
    return true;
}

static bool quota_state_detector_is_valid(
    const ambient_quota_reset_detector_t *detector)
{
    const ambient_quota_reset_window_state_t *states[2];

    if (!detector || !isfinite(detector->minimum_drop_percent)
        || detector->minimum_drop_percent <= 0.0
        || detector->minimum_drop_percent > 100.0) {
        return false;
    }
    states[0] = &detector->short_window;
    states[1] = &detector->long_window;
    for (size_t i = 0; i < 2; ++i) {
        const ambient_quota_reset_window_state_t *state = states[i];
        if (state->has_baseline
            && (!isfinite(state->baseline_used_percent)
                || state->baseline_used_percent < 0.0
                || state->baseline_used_percent > 100.0
                || state->baseline_reset_marker > state->highest_reset_marker)) {
            return false;
        }
        if (!state->has_baseline
            && (state->baseline_used_percent != 0.0
                || state->baseline_reset_marker != 0
                || state->highest_reset_marker != 0
                || state->has_candidate
                || state->candidate_reset_marker != 0)) {
            return false;
        }
        if ((state->has_candidate
             && (state->candidate_reset_marker <= state->baseline_reset_marker
                 || state->candidate_reset_marker != state->highest_reset_marker))
            || (!state->has_candidate && state->candidate_reset_marker != 0)) {
            return false;
        }
    }
    return true;
}

static bool quota_state_encode(
    const ambient_quota_reset_detector_t *detector,
    uint8_t bytes[QUOTA_STATE_FILE_BYTES])
{
    size_t offset = QUOTA_STATE_HEADER_BYTES;
    uint32_t crc;

    if (!quota_state_detector_is_valid(detector) || !bytes) {
        return false;
    }
    memset(bytes, 0, QUOTA_STATE_FILE_BYTES);
    memcpy(bytes, quota_state_magic, sizeof(quota_state_magic));
    bytes[8] = 1;
    quota_state_put_window(bytes + offset, &detector->short_window);
    offset += QUOTA_STATE_WINDOW_BYTES;
    quota_state_put_window(bytes + offset, &detector->long_window);
    offset += QUOTA_STATE_WINDOW_BYTES;
    crc = quota_state_crc32(bytes, offset);
    bytes[offset] = (uint8_t)(crc >> 24);
    bytes[offset + 1] = (uint8_t)(crc >> 16);
    bytes[offset + 2] = (uint8_t)(crc >> 8);
    bytes[offset + 3] = (uint8_t)crc;
    return true;
}

static bool quota_state_decode(
    const uint8_t bytes[QUOTA_STATE_FILE_BYTES],
    ambient_quota_reset_detector_t *detector)
{
    ambient_quota_reset_window_state_t short_window;
    ambient_quota_reset_window_state_t long_window;
    uint32_t expected_crc;
    uint32_t actual_crc;
    size_t offset = QUOTA_STATE_HEADER_BYTES;

    if (!bytes || !detector
        || memcmp(bytes, quota_state_magic, sizeof(quota_state_magic)) != 0
        || bytes[8] != 1 || bytes[9] != 0 || bytes[10] != 0 || bytes[11] != 0) {
        return false;
    }
    expected_crc = ((uint32_t)bytes[QUOTA_STATE_FILE_BYTES - 4] << 24)
        | ((uint32_t)bytes[QUOTA_STATE_FILE_BYTES - 3] << 16)
        | ((uint32_t)bytes[QUOTA_STATE_FILE_BYTES - 2] << 8)
        | (uint32_t)bytes[QUOTA_STATE_FILE_BYTES - 1];
    actual_crc = quota_state_crc32(bytes, QUOTA_STATE_FILE_BYTES - 4);
    if (expected_crc != actual_crc) {
        return false;
    }
    if (!quota_state_get_window(bytes + offset, &short_window)) {
        return false;
    }
    offset += QUOTA_STATE_WINDOW_BYTES;
    if (!quota_state_get_window(bytes + offset, &long_window)) {
        return false;
    }
    detector->short_window = short_window;
    detector->long_window = long_window;
    return true;
}

static void quota_state_rebaseline(
    ambient_quota_reset_detector_t *detector)
{
    double threshold;

    if (!detector) {
        return;
    }
    threshold = detector->minimum_drop_percent;
    (void)ambient_quota_reset_detector_init(detector, threshold);
}

static size_t quota_state_path_length(const char *path)
{
    size_t length = 0;
    if (!path) {
        return QUOTA_STATE_PATH_MAX_BYTES + 1U;
    }
    while (length <= QUOTA_STATE_PATH_MAX_BYTES && path[length] != '\0') {
        length++;
    }
    return length;
}

static bool quota_state_write_all(int fd,
                                  const uint8_t *bytes,
                                  size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t amount = write(fd, bytes + offset, length - offset);
        if (amount < 0 && errno == EINTR) {
            continue;
        }
        if (amount <= 0) {
            return false;
        }
        offset += (size_t)amount;
    }
    return true;
}

static bool quota_state_sync_parent(const char *path)
{
    size_t length = quota_state_path_length(path);
    char *parent;
    char *separator;
    int fd;
    bool ok;

    if (length == 0 || length > QUOTA_STATE_PATH_MAX_BYTES) {
        return false;
    }
    parent = malloc(length + 1);
    if (!parent) {
        return false;
    }
    memcpy(parent, path, length + 1);
    separator = strrchr(parent, '/');
    if (!separator) {
        parent[0] = '.';
        parent[1] = '\0';
    } else if (separator == parent) {
        parent[1] = '\0';
    } else {
        *separator = '\0';
    }
    fd = open(parent, O_RDONLY | O_DIRECTORY);
    free(parent);
    if (fd < 0) {
        return false;
    }
    ok = fsync(fd) == 0;
    if (close(fd) != 0) {
        ok = false;
    }
    return ok;
}

quota_reset_state_store_result_t quota_reset_state_store_load(
    const char *path,
    ambient_quota_reset_detector_t *detector)
{
    uint8_t bytes[QUOTA_STATE_FILE_BYTES];
    struct stat file_status;
    size_t offset = 0;
    double threshold;
    int fd;
    bool read_ok = true;
    uint8_t extra;
    ssize_t extra_bytes;

    if (quota_state_path_length(path) == 0
        || quota_state_path_length(path) > QUOTA_STATE_PATH_MAX_BYTES
        || !quota_state_detector_is_valid(detector)) {
        return QUOTA_RESET_STATE_STORE_INVALID;
    }
    threshold = detector->minimum_drop_percent;
    fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        quota_state_rebaseline(detector);
        return errno == ENOENT ? QUOTA_RESET_STATE_STORE_NOT_FOUND
                               : QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (fstat(fd, &file_status) != 0) {
        (void)close(fd);
        quota_state_rebaseline(detector);
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (!S_ISREG(file_status.st_mode)
        || file_status.st_size != QUOTA_STATE_FILE_BYTES) {
        (void)close(fd);
        quota_state_rebaseline(detector);
        return QUOTA_RESET_STATE_STORE_CORRUPT;
    }
    while (read_ok && offset < sizeof(bytes)) {
        ssize_t amount = read(fd, bytes + offset, sizeof(bytes) - offset);
        if (amount < 0 && errno == EINTR) {
            continue;
        }
        if (amount < 0) {
            (void)close(fd);
            quota_state_rebaseline(detector);
            return QUOTA_RESET_STATE_STORE_IO_ERROR;
        }
        if (amount == 0) {
            read_ok = false;
            break;
        }
        offset += (size_t)amount;
    }
    extra_bytes = read(fd, &extra, 1);
    if (extra_bytes < 0 && errno == EINTR) {
        do {
            extra_bytes = read(fd, &extra, 1);
        } while (extra_bytes < 0 && errno == EINTR);
    }
    if (extra_bytes < 0) {
        (void)close(fd);
        quota_state_rebaseline(detector);
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (close(fd) != 0) {
        quota_state_rebaseline(detector);
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (!read_ok || offset != sizeof(bytes) || extra_bytes != 0
        || !quota_state_decode(bytes, detector)) {
        (void)ambient_quota_reset_detector_init(detector, threshold);
        return QUOTA_RESET_STATE_STORE_CORRUPT;
    }
    return QUOTA_RESET_STATE_STORE_OK;
}

quota_reset_state_store_result_t quota_reset_state_store_save_atomic(
    const char *path,
    const ambient_quota_reset_detector_t *detector)
{
    uint8_t bytes[QUOTA_STATE_FILE_BYTES];
    size_t path_length = quota_state_path_length(path);
    char *temporary_path;
    int fd = -1;
    bool renamed = false;
    bool ok = false;

    if (path_length == 0 || path_length > QUOTA_STATE_PATH_MAX_BYTES
        || !quota_state_encode(detector, bytes)) {
        return QUOTA_RESET_STATE_STORE_INVALID;
    }
    temporary_path = malloc(path_length + sizeof(".tmp.XXXXXX"));
    if (!temporary_path) {
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (snprintf(temporary_path, path_length + sizeof(".tmp.XXXXXX"),
                 "%s.tmp.XXXXXX", path) < 0) {
        free(temporary_path);
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    fd = mkstemp(temporary_path);
    if (fd < 0) {
        free(temporary_path);
        return QUOTA_RESET_STATE_STORE_IO_ERROR;
    }
    if (fchmod(fd, S_IRUSR | S_IWUSR) == 0
        && quota_state_write_all(fd, bytes, sizeof(bytes))
        && fsync(fd) == 0) {
        int close_result = close(fd);
        fd = -1;
        if (close_result == 0 && rename(temporary_path, path) == 0) {
            renamed = true;
            ok = quota_state_sync_parent(path);
        }
    }
    if (fd >= 0) {
        (void)close(fd);
    }
    if (!renamed) {
        (void)unlink(temporary_path);
    }
    free(temporary_path);
    return ok ? QUOTA_RESET_STATE_STORE_OK
              : QUOTA_RESET_STATE_STORE_IO_ERROR;
}
