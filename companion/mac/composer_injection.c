#include "composer_injection.h"

#include <string.h>

static composer_injection_result_t composer_map_backend_result(
    composer_insertion_backend_result_t result)
{
    switch (result) {
    case COMPOSER_INSERTION_BACKEND_OK:
        return COMPOSER_INJECTION_INSERTED;
    case COMPOSER_INSERTION_BACKEND_UNAVAILABLE:
        return COMPOSER_INJECTION_UNAVAILABLE;
    case COMPOSER_INSERTION_BACKEND_UNSUPPORTED:
        return COMPOSER_INJECTION_UNSUPPORTED;
    case COMPOSER_INSERTION_BACKEND_FAILED:
        return COMPOSER_INJECTION_FAILED;
    case COMPOSER_INSERTION_BACKEND_INVALID:
    default:
        return COMPOSER_INJECTION_INVALID;
    }
}

composer_injection_result_t composer_injection_insert_text(
    const composer_injection_backend_t *backend,
    composer_target_handle_t target,
    const char *text,
    size_t text_bytes)
{
    composer_target_verification_t verification;
    composer_insertion_backend_result_t insertion_result;

    if (target == 0 || !text || text_bytes == 0
        || memchr(text, '\0', text_bytes) != NULL) {
        return COMPOSER_INJECTION_INVALID;
    }
    if (!backend) {
        return COMPOSER_INJECTION_UNAVAILABLE;
    }
    if (!backend->verify_target || !backend->insert_text) {
        return COMPOSER_INJECTION_UNSUPPORTED;
    }

    verification = backend->verify_target(backend->context, target);
    switch (verification) {
    case COMPOSER_TARGET_VERIFIED_CODEX_COMPOSER:
        break;
    case COMPOSER_TARGET_UNVERIFIED:
    case COMPOSER_TARGET_OTHER_APPLICATION:
        return COMPOSER_INJECTION_TARGET_REJECTED;
    case COMPOSER_TARGET_UNAVAILABLE:
        return COMPOSER_INJECTION_UNAVAILABLE;
    case COMPOSER_TARGET_UNSUPPORTED:
        return COMPOSER_INJECTION_UNSUPPORTED;
    case COMPOSER_TARGET_FAILED:
        return COMPOSER_INJECTION_FAILED;
    case COMPOSER_TARGET_INVALID:
    default:
        return COMPOSER_INJECTION_INVALID;
    }

    insertion_result = backend->insert_text(backend->context, target, text,
                                             text_bytes);
    return composer_map_backend_result(insertion_result);
}
