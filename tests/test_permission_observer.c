#include <assert.h>
#include <stddef.h>

#include "permission_observer.h"

typedef struct {
    size_t calls;
} fake_observer_t;

static permission_observer_result_t report_unsupported(void *context)
{
    fake_observer_t *fake = context;
    assert(fake != NULL);
    fake->calls++;
    return PERMISSION_OBSERVER_UNSUPPORTED;
}

static permission_observer_result_t report_unavailable(void *context)
{
    fake_observer_t *fake = context;
    assert(fake != NULL);
    fake->calls++;
    return PERMISSION_OBSERVER_UNAVAILABLE;
}

int main(void)
{
    fake_observer_t fake = {0};
    permission_observer_t observer = {
        .context = &fake,
        .poll = NULL,
    };

    assert(permission_observer_poll(NULL) == PERMISSION_OBSERVER_UNAVAILABLE);
    assert(permission_observer_poll(&observer)
           == PERMISSION_OBSERVER_UNAVAILABLE);
    assert(fake.calls == 0);

    observer.poll = report_unavailable;
    assert(permission_observer_poll(&observer)
           == PERMISSION_OBSERVER_UNAVAILABLE);
    assert(fake.calls == 1);

    observer.poll = report_unsupported;
    assert(permission_observer_poll(&observer)
           == PERMISSION_OBSERVER_UNSUPPORTED);
    assert(fake.calls == 2);
    /* Missing support is never converted into a fabricated observation. */
    assert(permission_observer_poll(&observer)
           != PERMISSION_OBSERVER_REQUEST_OBSERVED);
    return 0;
}
