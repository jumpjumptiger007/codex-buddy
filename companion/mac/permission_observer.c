#include "permission_observer.h"

permission_observer_result_t permission_observer_poll(
    const permission_observer_t *observer)
{
    permission_observer_result_t result;

    if (!observer || !observer->poll) {
        return PERMISSION_OBSERVER_UNAVAILABLE;
    }

    result = observer->poll(observer->context);
    switch (result) {
    case PERMISSION_OBSERVER_NO_REQUEST_OBSERVED:
    case PERMISSION_OBSERVER_REQUEST_OBSERVED:
    case PERMISSION_OBSERVER_UNAVAILABLE:
    case PERMISSION_OBSERVER_UNSUPPORTED:
    case PERMISSION_OBSERVER_FAILED:
        return result;
    case PERMISSION_OBSERVER_INVALID:
    default:
        return PERMISSION_OBSERVER_INVALID;
    }
}
