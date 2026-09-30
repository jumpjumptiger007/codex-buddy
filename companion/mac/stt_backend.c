#include "stt_backend.h"

companion_stt_result_t companion_stt_transcribe(
    const companion_stt_backend_t *backend,
    const uint8_t *audio,
    size_t audio_bytes,
    char *transcript,
    size_t transcript_capacity,
    size_t *transcript_bytes)
{
    companion_stt_result_t result;
    size_t produced_bytes = 0;

    if (transcript_bytes) {
        *transcript_bytes = 0;
    }
    if (transcript && transcript_capacity > 0) {
        transcript[0] = '\0';
    }
    if (!audio || audio_bytes == 0 || !transcript
        || transcript_capacity == 0 || !transcript_bytes) {
        return COMPANION_STT_INVALID;
    }
    if (!backend || !backend->transcribe) {
        return COMPANION_STT_UNAVAILABLE;
    }

    result = backend->transcribe(backend->context, audio, audio_bytes,
                                 transcript, transcript_capacity,
                                 &produced_bytes);
    if (result == COMPANION_STT_OK) {
        if (produced_bytes > transcript_capacity) {
            transcript[0] = '\0';
            return COMPANION_STT_FAILED;
        }
        *transcript_bytes = produced_bytes;
        if (produced_bytes < transcript_capacity) {
            transcript[produced_bytes] = '\0';
        }
        return COMPANION_STT_OK;
    }

    transcript[0] = '\0';
    switch (result) {
    case COMPANION_STT_UNAVAILABLE:
    case COMPANION_STT_UNSUPPORTED:
    case COMPANION_STT_FAILED:
    case COMPANION_STT_INVALID:
        return result;
    case COMPANION_STT_OK:
    default:
        return COMPANION_STT_INVALID;
    }
}
