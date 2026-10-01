#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "passport_ui_model.h"

#define PASSPORT_UI_PRESENTATION_TEXT_CAPACITY 40u

/* Format source-only quota values into a fixed-size, display-ready string. */
bool passport_ui_present_quota(
    const passport_ui_quota_view_t *quota,
    char *destination,
    size_t destination_capacity);

/* Copy a bounded field, using "--" for empty, rejected, or malformed input. */
bool passport_ui_present_field(char *destination,
                               size_t destination_capacity,
                               const char *source,
                               size_t source_capacity,
                               bool rejected);
