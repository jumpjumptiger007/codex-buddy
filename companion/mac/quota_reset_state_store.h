#pragma once

#include "ambient_quota.h"

typedef enum {
    QUOTA_RESET_STATE_STORE_INVALID = 0,
    QUOTA_RESET_STATE_STORE_OK,
    QUOTA_RESET_STATE_STORE_NOT_FOUND,
    QUOTA_RESET_STATE_STORE_CORRUPT,
    QUOTA_RESET_STATE_STORE_IO_ERROR,
} quota_reset_state_store_result_t;

/* The versioned, bounded state contains only quota reset-detector fields. */
quota_reset_state_store_result_t quota_reset_state_store_load(
    const char *path,
    ambient_quota_reset_detector_t *detector);

quota_reset_state_store_result_t quota_reset_state_store_save_atomic(
    const char *path,
    const ambient_quota_reset_detector_t *detector);
