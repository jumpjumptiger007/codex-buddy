#pragma once

#include <stdint.h>

#include "ambient_quota.h"

typedef enum {
    APP_SERVER_QUOTA_INVALID = 0,
    APP_SERVER_QUOTA_UNAVAILABLE,
    APP_SERVER_QUOTA_AVAILABLE,
} app_server_quota_result_t;

typedef bool (*app_server_quota_read_fn)(
    void *context,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot);

/* The interface is injectable for tests; no real transport adapter is wired. */
typedef struct {
    app_server_quota_read_fn read;
    void *context;
} app_server_quota_source_t;

app_server_quota_result_t app_server_quota_source_read(
    const app_server_quota_source_t *source,
    int64_t now_unix_seconds,
    ambient_quota_snapshot_t *snapshot);
