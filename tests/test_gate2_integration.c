#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "companion_core.h"
#include "composer_injection.h"
#include "permission_observer.h"
#include "stt_backend.h"

typedef struct {
    permission_observer_result_t result;
    size_t calls;
} fake_permission_observer_t;

typedef struct {
    companion_stt_result_t result;
    const char *text;
    bool write_partial_output;
    size_t calls;
} fake_stt_t;

typedef struct {
    size_t verification_calls;
    size_t insertion_calls;
    composer_target_handle_t verified_target;
    composer_target_handle_t inserted_target;
    char inserted_text[64];
    size_t inserted_text_bytes;
} fake_composer_t;

typedef struct {
    size_t count;
} notification_capture_t;

static permission_observer_result_t fake_permission_poll(void *context)
{
    fake_permission_observer_t *fake = context;
    assert(fake != NULL);
    fake->calls++;
    return fake->result;
}

static companion_stt_result_t fake_transcribe(
    void *context,
    const uint8_t *audio,
    size_t audio_bytes,
    char *transcript,
    size_t transcript_capacity,
    size_t *transcript_bytes)
{
    fake_stt_t *fake = context;
    const char *text;
    size_t text_bytes;

    assert(fake != NULL);
    assert(audio != NULL && audio_bytes > 0);
    assert(transcript != NULL && transcript_capacity > 0);
    assert(transcript_bytes != NULL);
    fake->calls++;
    text = fake->result == COMPANION_STT_OK
        ? fake->text : "partial failure output";
    text_bytes = strlen(text);

    if (fake->result == COMPANION_STT_OK || fake->write_partial_output) {
        assert(text_bytes < transcript_capacity);
        memcpy(transcript, text, text_bytes);
        *transcript_bytes = text_bytes;
    }
    return fake->result;
}

static composer_target_verification_t fake_verify_target(
    void *context,
    composer_target_handle_t target)
{
    fake_composer_t *fake = context;
    assert(fake != NULL);
    fake->verification_calls++;
    return target == fake->verified_target
        ? COMPOSER_TARGET_VERIFIED_CODEX_COMPOSER
        : COMPOSER_TARGET_UNVERIFIED;
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
    assert(text_bytes < sizeof(fake->inserted_text));
    fake->insertion_calls++;
    fake->inserted_target = target;
    fake->inserted_text_bytes = text_bytes;
    memcpy(fake->inserted_text, text, text_bytes);
    fake->inserted_text[text_bytes] = '\0';
    return COMPOSER_INSERTION_BACKEND_OK;
}

static bool map_identifier(void *context,
                           companion_identifier_kind_t kind,
                           const char *identifier,
                           size_t identifier_length,
                           ambient_key_t *key)
{
    (void)context;
    if (!identifier || !key || identifier_length == 0) {
        return false;
    }
    *key = kind == COMPANION_IDENTIFIER_SESSION ? 1U : 2U;
    return true;
}

static void capture_notification(void *context,
                                 const companion_notification_t *notification)
{
    notification_capture_t *capture = context;
    assert(capture != NULL);
    assert(notification != NULL);
    capture->count++;
}

static void test_unavailable_permission_does_not_create_attention(void)
{
    companion_core_t core;
    ambient_session_slot_t slots[2];
    ambient_notification_key_t dedup_entries[4];
    notification_capture_t notifications = {0};
    companion_core_options_t options = {
        .session_slots = slots,
        .session_capacity = 2,
        .notification_entries = dedup_entries,
        .notification_capacity = 4,
        .session_freshness_ms = 1000,
        .done_hold_ms = 50,
        .minimum_quota_reset_drop_percent = AMBIENT_QUOTA_PRODUCT_RESET_DROP_PERCENT,
        .map_identifier = map_identifier,
        .on_notification = capture_notification,
        .notification_context = &notifications,
    };
    fake_permission_observer_t fake = {
        .result = PERMISSION_OBSERVER_UNAVAILABLE,
    };
    permission_observer_t observer = {
        .context = &fake,
        .poll = fake_permission_poll,
    };
    companion_snapshot_t snapshot;

    assert(companion_core_init(&core, &options));
    assert(companion_core_snapshot(&core, 10, 100, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_OFFLINE);
    assert(snapshot.lifecycle.fresh_session_count == 0);

    assert(permission_observer_poll(&observer)
           == PERMISSION_OBSERVER_UNAVAILABLE);
    assert(companion_core_snapshot(&core, 11, 101, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_OFFLINE);
    assert(snapshot.lifecycle.attention_session_count == 0);
    assert(notifications.count == 0);

    fake.result = PERMISSION_OBSERVER_UNSUPPORTED;
    assert(permission_observer_poll(&observer)
           == PERMISSION_OBSERVER_UNSUPPORTED);
    assert(companion_core_snapshot(&core, 12, 102, &snapshot));
    assert(snapshot.lifecycle.status == AMBIENT_STATUS_OFFLINE);
    assert(snapshot.lifecycle.attention_session_count == 0);
    assert(notifications.count == 0);
    assert(fake.calls == 2);
}

static void test_fake_stt_output_flows_to_verified_text_insertion_only(void)
{
    static const uint8_t synthetic_audio[] = {0x11, 0x22, 0x33};
    static const char recognized_text[] = "synthetic recognized words";
    fake_stt_t fake_stt = {
        .result = COMPANION_STT_OK,
        .text = recognized_text,
    };
    companion_stt_backend_t stt = {
        .context = &fake_stt,
        .transcribe = fake_transcribe,
    };
    fake_composer_t fake_composer = {
        .verified_target = 41,
    };
    composer_injection_backend_t composer = {
        .context = &fake_composer,
        .verify_target = fake_verify_target,
        .insert_text = fake_insert_text,
    };
    char transcript[64] = "stale text";
    size_t transcript_bytes = 99;

    assert(companion_stt_transcribe(
               &stt, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_OK);
    assert(transcript_bytes == sizeof(recognized_text) - 1U);
    assert(composer_injection_insert_text(
               &composer, fake_composer.verified_target, transcript,
               transcript_bytes)
           == COMPOSER_INJECTION_INSERTED);
    assert(fake_composer.verification_calls == 1);
    assert(fake_composer.insertion_calls == 1);
    assert(fake_composer.inserted_target == fake_composer.verified_target);
    assert(fake_composer.inserted_text_bytes == transcript_bytes);
    assert(memcmp(fake_composer.inserted_text, recognized_text,
                  transcript_bytes) == 0);

    /* Missing and failed STT results never supply usable composer input. */
    assert(companion_stt_transcribe(
               NULL, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_UNAVAILABLE);
    assert(transcript_bytes == 0 && transcript[0] == '\0');
    fake_stt.result = COMPANION_STT_UNAVAILABLE;
    assert(companion_stt_transcribe(
               &stt, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_UNAVAILABLE);
    assert(transcript_bytes == 0 && transcript[0] == '\0');
    assert(fake_composer.insertion_calls == 1);

    fake_stt.result = COMPANION_STT_FAILED;
    fake_stt.write_partial_output = true;
    assert(companion_stt_transcribe(
               &stt, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_FAILED);
    assert(transcript_bytes == 0 && transcript[0] == '\0');
    assert(fake_composer.insertion_calls == 1);
    assert(fake_stt.calls == 3);
}

int main(void)
{
    test_unavailable_permission_does_not_create_attention();
    test_fake_stt_output_flows_to_verified_text_insertion_only();
    return 0;
}
