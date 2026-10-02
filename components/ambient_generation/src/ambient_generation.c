#include "ambient_generation.h"

#include <stddef.h>

bool ambient_generation_next(const ambient_generation_backend_t *backend,
                             uint64_t *generation)
{
    uint32_t previous = 0;
    uint32_t random_part = 0;
    uint32_t next;

    if (generation != NULL) {
        *generation = 0;
    }
    if (backend == NULL || generation == NULL || backend->read_epoch == NULL ||
        backend->write_epoch == NULL || backend->random_u32 == NULL ||
        !backend->read_epoch(backend->context, &previous) ||
        previous == UINT32_MAX ||
        !backend->random_u32(backend->context, &random_part)) {
        return false;
    }

    next = previous + 1U;
    if (!backend->write_epoch(backend->context, next)) {
        return false;
    }
    /* The durable epoch occupies the high half so generations increase across
     * Companion process restarts as required by the R2 Companion runtime. */
    *generation = ((uint64_t)next << 32) | random_part;
    return *generation != 0;
}
