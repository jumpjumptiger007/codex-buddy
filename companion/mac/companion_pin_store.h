#pragma once
#include "ambient_auth.h"
#include "companion_generation_store.h"
typedef bool (*companion_pin_approval_fn)(void *, const uint8_t[65], const char *);
typedef struct {
    bool open, poisoned, pinned;
    int lock_fd;
    char path[COMPANION_GENERATION_PATH_BYTES];
    uint8_t key[65];
    companion_generation_io_t io;
} companion_pin_store_t;
/* Zero initialize; serialize calls; owner and explicit path remain caller-owned. */
bool companion_pin_store_open(companion_pin_store_t *,const char *,const companion_generation_io_t *);
void companion_pin_store_close(companion_pin_store_t *);
bool companion_pin_store_key(const companion_pin_store_t *,uint8_t[65]);
bool companion_pin_store_enroll(companion_pin_store_t *,const uint8_t[65],
    const ambient_auth_crypto_t *,companion_pin_approval_fn,void *);
typedef struct { companion_pin_store_t *store; companion_pin_approval_fn approve; void *context; } companion_pin_auth_owner_t;
ambient_auth_crypto_t companion_pin_auth_crypto(companion_pin_auth_owner_t *);
