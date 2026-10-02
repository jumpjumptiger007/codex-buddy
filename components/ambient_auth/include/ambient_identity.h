#pragma once
#include "ambient_auth.h"
/* Backend operations never export private material; serialized single owner. */
typedef struct {
    /* load: 1 valid identity, 0 never created, -1 corrupt/partial/storage error. */
    int (*load)(void *);
    bool (*mark_creation)(void *);
    bool (*create_and_commit)(void *);
    bool (*public_key)(void *, uint8_t[65]);
    bool (*sign)(void *, const uint8_t[32], uint8_t[64]);
    void *context;
} ambient_identity_backend_t;
typedef struct { bool ready; ambient_identity_backend_t backend; uint8_t public_key[65]; } ambient_identity_t;
bool ambient_identity_open(ambient_identity_t *, const ambient_identity_backend_t *);
void ambient_identity_close(ambient_identity_t *);
bool ambient_identity_public(const ambient_identity_t *, uint8_t[65]);
bool ambient_identity_sign(void *, const uint8_t[32], uint8_t[64]);
/* ESP factory must run before BSP/RF/ADC initialization to seed hardware entropy.
 * It is a singleton; caller serializes crypto. Exhausted/reseed-failed DRBG fails
 * closed (re-entry requires the same early-startup entropy ownership).
 * ESP factory owns crypto/NVS context; no reset or private-key export API. */
typedef struct ambient_identity_esp ambient_identity_esp_t;
bool ambient_identity_esp_open(ambient_identity_esp_t **);
void ambient_identity_esp_close(ambient_identity_esp_t *);
bool ambient_identity_esp_public(ambient_identity_esp_t *, uint8_t[65]);
ambient_auth_crypto_t ambient_identity_esp_crypto(ambient_identity_esp_t *);
