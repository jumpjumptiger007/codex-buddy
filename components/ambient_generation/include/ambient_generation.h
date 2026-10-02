#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* Companion-owned backend: calls are serialized under one store owner.
     * Successful write_epoch must durably and atomically commit before return;
     * never roll back/reuse an epoch across reconnect or process restart.
     * random_u32 must use the host CSPRNG and report failure, not a fallback.
     * No user configuration is installed by this injectable seam.
     * read_epoch succeeds with zero when no value has ever been committed. */
    bool (*read_epoch)(void *context, uint32_t *epoch);
    bool (*write_epoch)(void *context, uint32_t epoch);
    bool (*random_u32)(void *context, uint32_t *value);
    void *context;
} ambient_generation_backend_t;

/* Persist a never-reused 32-bit high epoch before returning a monotone,
 * CSPRNG-salted 64-bit ID. On epoch exhaustion or storage/RNG failure,
 * fail closed. */
bool ambient_generation_next(const ambient_generation_backend_t *backend,
                             uint64_t *generation);
