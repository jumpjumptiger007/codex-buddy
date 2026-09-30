#include <assert.h>
#include <string.h>

#include "composer_injection.h"

typedef enum {
    FAKE_ACTION_INSERT_TEXT = 1,
} fake_action_t;

typedef struct {
    composer_target_handle_t verified_target;
    composer_target_handle_t unverified_target;
    composer_target_handle_t other_application_target;
    composer_target_handle_t unavailable_target;
    composer_target_handle_t verified_fallback_target;
    composer_target_handle_t verified_handles[4];
    size_t verification_calls;
    composer_target_handle_t inserted_target;
    size_t insertion_calls;
    fake_action_t actions[4];
    size_t action_count;
    composer_insertion_backend_result_t insertion_result;
    char inserted_text[64];
    size_t inserted_text_bytes;
} fake_composer_t;

static composer_target_verification_t fake_verify_target(
    void *context,
    composer_target_handle_t target)
{
    fake_composer_t *fake = context;

    assert(fake != NULL);
    assert(fake->verification_calls
           < sizeof(fake->verified_handles) / sizeof(fake->verified_handles[0]));
    fake->verified_handles[fake->verification_calls++] = target;
    if (target == fake->verified_target
        || target == fake->verified_fallback_target) {
        return COMPOSER_TARGET_VERIFIED_CODEX_COMPOSER;
    }
    if (target == fake->unverified_target) {
        return COMPOSER_TARGET_UNVERIFIED;
    }
    if (target == fake->other_application_target) {
        return COMPOSER_TARGET_OTHER_APPLICATION;
    }
    if (target == fake->unavailable_target) {
        return COMPOSER_TARGET_UNAVAILABLE;
    }
    return COMPOSER_TARGET_UNSUPPORTED;
}

static composer_insertion_backend_result_t fake_insert_text(
    void *context,
    composer_target_handle_t target,
    const char *text,
    size_t text_bytes)
{
    fake_composer_t *fake = context;

    assert(fake != NULL);
    assert(text != NULL && text_bytes > 0);
    assert(fake->action_count
           < sizeof(fake->actions) / sizeof(fake->actions[0]));
    fake->actions[fake->action_count++] = FAKE_ACTION_INSERT_TEXT;
    fake->insertion_calls++;
    fake->inserted_target = target;
    fake->inserted_text_bytes = text_bytes;
    if (text_bytes >= sizeof(fake->inserted_text)) {
        return COMPOSER_INSERTION_BACKEND_FAILED;
    }
    memcpy(fake->inserted_text, text, text_bytes);
    fake->inserted_text[text_bytes] = '\0';
    return fake->insertion_result;
}

static void reset_fake_calls(fake_composer_t *fake)
{
    fake->verification_calls = 0;
    fake->insertion_calls = 0;
    fake->action_count = 0;
    fake->inserted_target = 0;
    fake->inserted_text_bytes = 0;
    memset(fake->verified_handles, 0, sizeof(fake->verified_handles));
    memset(fake->actions, 0, sizeof(fake->actions));
    memset(fake->inserted_text, 0, sizeof(fake->inserted_text));
}

int main(void)
{
    static const char recognized_text[] = "turn left at the next street";
    fake_composer_t fake = {
        .verified_target = 11,
        .unverified_target = 22,
        .other_application_target = 33,
        .unavailable_target = 44,
        .verified_fallback_target = 12,
        .insertion_result = COMPOSER_INSERTION_BACKEND_OK,
    };
    composer_injection_backend_t backend = {
        .context = &fake,
        .verify_target = fake_verify_target,
        .insert_text = fake_insert_text,
    };
    composer_injection_backend_t missing_verifier = {
        .context = &fake,
        .verify_target = NULL,
        .insert_text = fake_insert_text,
    };

    assert(composer_injection_insert_text(
               NULL, fake.verified_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_UNAVAILABLE);
    assert(composer_injection_insert_text(
               &missing_verifier, fake.verified_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_UNSUPPORTED);
    assert(fake.verification_calls == 0 && fake.insertion_calls == 0);

    assert(composer_injection_insert_text(
               &backend, fake.unverified_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_TARGET_REJECTED);
    assert(fake.verification_calls == 1);
    assert(fake.verified_handles[0] == fake.unverified_target);
    assert(fake.insertion_calls == 0);
    /* A verified alternate exists, but the failed target is never retargeted. */
    assert(fake.verified_handles[0] != fake.verified_fallback_target);

    reset_fake_calls(&fake);
    assert(composer_injection_insert_text(
               &backend, fake.other_application_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_TARGET_REJECTED);
    assert(fake.verification_calls == 1 && fake.insertion_calls == 0);
    assert(fake.verified_handles[0] == fake.other_application_target);

    reset_fake_calls(&fake);
    assert(composer_injection_insert_text(
               &backend, fake.unavailable_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_UNAVAILABLE);
    assert(fake.verification_calls == 1 && fake.insertion_calls == 0);
    assert(fake.verified_handles[0] == fake.unavailable_target);

    reset_fake_calls(&fake);
    assert(composer_injection_insert_text(
               &backend, fake.verified_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_INSERTED);
    assert(fake.verification_calls == 1 && fake.insertion_calls == 1);
    assert(fake.verified_handles[0] == fake.verified_target);
    assert(fake.inserted_target == fake.verified_target);
    assert(fake.inserted_text_bytes == sizeof(recognized_text) - 1U);
    assert(memcmp(fake.inserted_text, recognized_text,
                  sizeof(recognized_text)) == 0);
    assert(fake.action_count == 1);
    assert(fake.actions[0] == FAKE_ACTION_INSERT_TEXT);

    reset_fake_calls(&fake);
    fake.insertion_result = COMPOSER_INSERTION_BACKEND_FAILED;
    assert(composer_injection_insert_text(
               &backend, fake.verified_target, recognized_text,
               sizeof(recognized_text) - 1U)
           == COMPOSER_INJECTION_FAILED);
    assert(fake.verification_calls == 1 && fake.insertion_calls == 1);
    assert(fake.verified_handles[0] == fake.verified_target);
    assert(fake.action_count == 1);
    assert(fake.actions[0] == FAKE_ACTION_INSERT_TEXT);
    return 0;
}
