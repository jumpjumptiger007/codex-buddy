#include "companion_diagnostics.h"
#include <stdio.h>
_Static_assert(sizeof(companion_diagnostics_t) <= 96, "bounded diagnostic state");
void companion_diagnostic_increment(uint32_t *counter)
{
    if (counter && *counter != UINT32_MAX) ++*counter;
}
bool companion_diagnostics_encode(const companion_diagnostics_t *d,
                                  char *out, size_t capacity, size_t *length)
{
    if (length) *length = 0;
    if (!d || !out || !length || !capacity) return false;
    out[0] = 0;
    if (d->profile > CODEX_SOURCE_SYNTHETIC || d->profile < CODEX_SOURCE_DISABLED
        || d->queued > 8 || d->sessions > 16 || d->turns > 32
        || d->candidate_mask > 3 || d->confirmed_mask > 3
        || d->load_result < QUOTA_RESET_STATE_STORE_INVALID
        || d->write_result < QUOTA_RESET_STATE_STORE_INVALID
        || d->load_result > QUOTA_RESET_STATE_STORE_IO_ERROR
        || d->write_result > QUOTA_RESET_STATE_STORE_IO_ERROR) return false;
    char record[COMPANION_DIAGNOSTICS_MAX_BYTES];
    int n = snprintf(record, sizeof(record),
        "D1|%08X|%08X|%08X|%08X|%08X|%08X|%02X|%02X|%02X|%02X|%02X|%u|%u|%u|%u|%u|%u|%016llX|%016llX",
        d->accepted, d->rejected, d->queue_dropped, d->notice_dropped,
        d->source_losses, d->persistence_failures, d->queued, d->sessions,
        d->turns, d->candidate_mask, d->confirmed_mask, d->running,
        d->source_available, d->profile, d->load_result, d->write_result, d->persistence_degraded,
        (unsigned long long)d->short_marker, (unsigned long long)d->long_marker);
    if (n < 0 || (size_t)n >= sizeof(record) || (size_t)n >= capacity) return false;
    for (size_t j = 0; j <= (size_t)n; ++j) out[j] = record[j];
    *length = (size_t)n; return true;
}
