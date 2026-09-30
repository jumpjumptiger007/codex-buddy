#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "stt_backend.h"

typedef struct {
    companion_stt_result_t result;
    size_t calls;
    bool write_partial_output;
} fake_stt_t;

static companion_stt_result_t fake_transcribe(
    void *context,
    const uint8_t *audio,
    size_t audio_bytes,
    char *transcript,
    size_t transcript_capacity,
    size_t *transcript_bytes)
{
    fake_stt_t *fake = context;
    static const char recognized[] = "test transcript";
    static const char partial[] = "partial";

    assert(fake != NULL);
    assert(audio != NULL && audio_bytes > 0);
    assert(transcript != NULL && transcript_capacity > 0);
    assert(transcript_bytes != NULL);
    fake->calls++;

    if (fake->result == COMPANION_STT_OK) {
        size_t length = sizeof(recognized) - 1U;
        if (length > transcript_capacity) {
            return COMPANION_STT_FAILED;
        }
        memcpy(transcript, recognized, length);
        *transcript_bytes = length;
        return COMPANION_STT_OK;
    }
    if (fake->write_partial_output) {
        size_t length = sizeof(partial) - 1U;
        if (length > transcript_capacity) {
            length = transcript_capacity;
        }
        memcpy(transcript, partial, length);
        *transcript_bytes = length;
    }
    return fake->result;
}

int main(void)
{
    const uint8_t synthetic_audio[] = {0x01, 0x02, 0x03};
    char transcript[32] = "old text";
    size_t transcript_bytes = 99;
    fake_stt_t fake = {
        .result = COMPANION_STT_FAILED,
        .write_partial_output = true,
    };
    companion_stt_backend_t backend = {
        .context = &fake,
        .transcribe = fake_transcribe,
    };

    assert(companion_stt_transcribe(
               NULL, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_UNAVAILABLE);
    assert(transcript_bytes == 0 && transcript[0] == '\0');

    memcpy(transcript, "old text", sizeof("old text"));
    transcript_bytes = 99;
    assert(companion_stt_transcribe(
               &backend, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_FAILED);
    assert(fake.calls == 1);
    assert(transcript_bytes == 0 && transcript[0] == '\0');

    fake.result = COMPANION_STT_OK;
    fake.write_partial_output = false;
    assert(companion_stt_transcribe(
               &backend, synthetic_audio, sizeof(synthetic_audio), transcript,
               sizeof(transcript), &transcript_bytes)
           == COMPANION_STT_OK);
    assert(fake.calls == 2);
    assert(transcript_bytes == sizeof("test transcript") - 1U);
    assert(strcmp(transcript, "test transcript") == 0);
    return 0;
}
