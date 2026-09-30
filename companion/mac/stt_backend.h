#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    COMPANION_STT_INVALID = 0,
    COMPANION_STT_OK,
    COMPANION_STT_UNAVAILABLE,
    COMPANION_STT_UNSUPPORTED,
    COMPANION_STT_FAILED,
} companion_stt_result_t;

typedef companion_stt_result_t (*companion_stt_transcribe_fn)(
    void *context,
    const uint8_t *audio,
    size_t audio_bytes,
    char *transcript,
    size_t transcript_capacity,
    size_t *transcript_bytes);

/* Injectable backend boundary; no runtime or model is provided here. */
typedef struct {
    void *context;
    companion_stt_transcribe_fn transcribe;
} companion_stt_backend_t;

/* Audio and transcript buffers are borrowed for this call and never retained. */
companion_stt_result_t companion_stt_transcribe(
    const companion_stt_backend_t *backend,
    const uint8_t *audio,
    size_t audio_bytes,
    char *transcript,
    size_t transcript_capacity,
    size_t *transcript_bytes);
