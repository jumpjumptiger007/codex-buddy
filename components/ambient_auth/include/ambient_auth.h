#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define AMBIENT_AUTH_MAX_RECORD 102U
#define AMBIENT_AUTH_PUBLIC_BYTES 65U
#define AMBIENT_AUTH_NONCE_BYTES 32U
#define AMBIENT_AUTH_SIGNATURE_BYTES 64U
/* Binary pre-R1 records: 'PA', version 1, type, big-endian payload length.
 * Types: 1 enrollment request (0), 2 public key (65), 3 challenge (32),
 * 4 response (nonce32 + raw r32/s32), 5 confirmation (SHA256 transcript32).
 * Never pass these records to ambient_wire. Serialized owner, one incarnation. */
typedef struct {
    bool (*random)(void *, uint8_t *, size_t);
    bool (*hash)(void *, const uint8_t *, size_t, uint8_t[32]);
    bool (*sign)(void *, const uint8_t[32], uint8_t[64]);
    bool (*verify)(void *, const uint8_t[65], const uint8_t[32], const uint8_t[64]);
    /* Approval must compare fingerprint with an independent local device source.
     * Callback persists only after explicit approval; false includes store error. */
    bool (*approve_and_pin)(void *, const uint8_t[65], const char *fingerprint);
    void *context;
} ambient_auth_crypto_t;
typedef enum { AMBIENT_AUTH_CLOSED, AMBIENT_AUTH_WAIT_KEY, AMBIENT_AUTH_WAIT_CHALLENGE,
    AMBIENT_AUTH_WAIT_RESPONSE, AMBIENT_AUTH_WAIT_CONFIRM, AMBIENT_AUTH_COMPLETE,
    AMBIENT_AUTH_FAILED } ambient_auth_state_t;
typedef struct {
    bool passport, pinned, enrollment_allowed;
    uint64_t incarnation;
    ambient_auth_state_t state;
    ambient_auth_crypto_t crypto;
    uint8_t public_key[65], challenge[32], nonce[32], transcript_hash[32];
    uint8_t input[102], output[102];
    size_t input_length, output_length;
} ambient_auth_t;
/* A missing host pin is allowed only for explicit enrollment, with callback.
 * Passport public key is mandatory; open is permitted only after BLE policy. */
bool ambient_auth_open(ambient_auth_t *, bool passport, uint64_t incarnation,
    bool transport_authorized, const uint8_t *public_key, bool explicit_enrollment,
    const ambient_auth_crypto_t *);
void ambient_auth_close(ambient_auth_t *);
/* Fail closed on malformed/unexpected/duplicate records, wrong incarnation or
 * authority loss. Finite feed <=129 bytes; pending output rejects further input. */
bool ambient_auth_feed(ambient_auth_t *, uint64_t incarnation, bool authorized,
    const uint8_t *, size_t);
/* Caller sends output all-or-none into bounded transport, then commits once.
 * Host COMPLETE only after confirmation admitted. Passport completes on receipt. */
const uint8_t *ambient_auth_output(const ambient_auth_t *, size_t *);
bool ambient_auth_output_committed(ambient_auth_t *);
bool ambient_auth_complete(const ambient_auth_t *, uint64_t incarnation);
bool ambient_auth_fingerprint(const ambient_auth_crypto_t *, const uint8_t[65], char[25]);
