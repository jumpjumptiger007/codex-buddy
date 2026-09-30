#pragma once

typedef enum {
    PERMISSION_OBSERVER_INVALID = 0,
    PERMISSION_OBSERVER_NO_REQUEST_OBSERVED,
    PERMISSION_OBSERVER_REQUEST_OBSERVED,
    PERMISSION_OBSERVER_UNAVAILABLE,
    PERMISSION_OBSERVER_UNSUPPORTED,
    PERMISSION_OBSERVER_FAILED,
} permission_observer_result_t;

typedef permission_observer_result_t (*permission_observer_poll_fn)(
    void *context);

/*
 * This is an orchestration boundary only. It reports observations from an
 * injected provider; observations are not permission decisions. It does not
 * install hooks, synthesize events, grant permissions, or establish trust.
 */
typedef struct {
    void *context;
    permission_observer_poll_fn poll;
} permission_observer_t;

/* Missing or incomplete providers fail closed as unavailable. */
permission_observer_result_t permission_observer_poll(
    const permission_observer_t *observer);
