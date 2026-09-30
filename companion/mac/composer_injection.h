#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint64_t composer_target_handle_t;

typedef enum {
    COMPOSER_TARGET_INVALID = 0,
    COMPOSER_TARGET_VERIFIED_CODEX_COMPOSER,
    COMPOSER_TARGET_UNVERIFIED,
    COMPOSER_TARGET_OTHER_APPLICATION,
    COMPOSER_TARGET_UNAVAILABLE,
    COMPOSER_TARGET_UNSUPPORTED,
    COMPOSER_TARGET_FAILED,
} composer_target_verification_t;

typedef enum {
    COMPOSER_INSERTION_BACKEND_INVALID = 0,
    COMPOSER_INSERTION_BACKEND_OK,
    COMPOSER_INSERTION_BACKEND_UNAVAILABLE,
    COMPOSER_INSERTION_BACKEND_UNSUPPORTED,
    COMPOSER_INSERTION_BACKEND_FAILED,
} composer_insertion_backend_result_t;

typedef enum {
    COMPOSER_INJECTION_INVALID = 0,
    COMPOSER_INJECTION_INSERTED,
    COMPOSER_INJECTION_UNAVAILABLE,
    COMPOSER_INJECTION_UNSUPPORTED,
    COMPOSER_INJECTION_TARGET_REJECTED,
    COMPOSER_INJECTION_FAILED,
} composer_injection_result_t;

typedef composer_target_verification_t (*composer_target_verify_fn)(
    void *context,
    composer_target_handle_t target);

typedef composer_insertion_backend_result_t (*composer_insert_text_fn)(
    void *context,
    composer_target_handle_t target,
    const char *text,
    size_t text_bytes);

/*
 * Opaque target verification and text insertion only. A real backend may
 * report VERIFIED_CODEX only after it has actually verified that target.
 * This repository provides no Accessibility selector or real verifier.
 */
typedef struct {
    void *context;
    composer_target_verify_fn verify_target;
    composer_insert_text_fn insert_text;
} composer_injection_backend_t;

/*
 * The caller chooses exactly one opaque target. Failure never searches for or
 * retargets another application. The backend surface has no submit operation.
 */
composer_injection_result_t composer_injection_insert_text(
    const composer_injection_backend_t *backend,
    composer_target_handle_t target,
    const char *text,
    size_t text_bytes);
