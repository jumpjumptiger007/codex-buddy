#include "ambient_generation.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t epoch;
    uint32_t random_value;
    bool has_epoch;
    bool read_fails;
    bool write_fails;
    bool random_fails;
} fake_store_t;

static bool read_epoch(void *context, uint32_t *epoch)
{
    fake_store_t *store = context;
    if (store->read_fails) return false;
    *epoch = store->has_epoch ? store->epoch : 0;
    return true;
}

static bool write_epoch(void *context, uint32_t epoch)
{
    fake_store_t *store = context;
    if (store->write_fails) return false;
    store->epoch = epoch;
    store->has_epoch = true;
    return true;
}

static bool random_u32(void *context, uint32_t *value)
{
    fake_store_t *store = context;
    if (store->random_fails) return false;
    *value = store->random_value;
    return true;
}

int main(void)
{
    fake_store_t store = {.random_value = 0x11223344};
    ambient_generation_backend_t backend = {
        .read_epoch = read_epoch,
        .write_epoch = write_epoch,
        .random_u32 = random_u32,
        .context = &store,
    };
    uint64_t first = 99;
    uint64_t second = 0;

    assert(ambient_generation_next(&backend, &first));
    assert(first == UINT64_C(0x0000000111223344));
    assert(store.epoch == 1);

    /* Identical random output still yields a never-reused generation. */
    assert(ambient_generation_next(&backend, &second));
    assert(second == UINT64_C(0x0000000211223344) && second > first);

    /* Reopening after restart continues from durable storage. */
    fake_store_t restarted = store;
    backend.context = &restarted;
    assert(ambient_generation_next(&backend, &second));
    assert(second == UINT64_C(0x0000000311223344) && second > first);

    restarted.random_value = 0;
    assert(ambient_generation_next(&backend, &second) && second != 0);
    assert(second == (UINT64_C(4) << 32));
    uint32_t committed = restarted.epoch;
    restarted.write_fails = true;
    assert(!ambient_generation_next(&backend, &second) && second == 0);
    assert(restarted.epoch == committed);
    restarted.write_fails = false;
    restarted.random_fails = true;
    assert(!ambient_generation_next(&backend, &second) && second == 0);
    assert(restarted.epoch == committed);
    restarted.random_fails = false;
    restarted.read_fails = true;
    assert(!ambient_generation_next(&backend, &second) && second == 0);
    restarted.read_fails = false;
    restarted.epoch = UINT32_MAX;
    assert(!ambient_generation_next(&backend, &second) && second == 0);

    assert(!ambient_generation_next(NULL, &second));
    assert(!ambient_generation_next(&backend, NULL));
    return 0;
}
