#include "passport_ui_presenter.h"

#include <stdio.h>
#include <string.h>

static bool percent_is_valid(double percent)
{
    /* The comparisons also reject NaN and both infinities. */
    return percent >= 0.0 && percent <= 100.0;
}

static unsigned int rounded_percent(double percent)
{
    return (unsigned int)(percent + 0.5);
}

static bool copy_availability(const passport_ui_quota_view_t *quota,
                             char *destination,
                             size_t destination_capacity)
{
    const char *availability = quota->available ? "AVAILABLE" : "NO DATA";
    const size_t length = strlen(availability);
    if (length >= destination_capacity) return false;
    memcpy(destination, availability, length + 1u);
    return true;
}

bool passport_ui_present_quota(
    const passport_ui_quota_view_t *quota,
    char *destination,
    size_t destination_capacity)
{
    if (!destination || destination_capacity == 0) return false;
    destination[0] = '\0';
    if (!quota) return false;

    char formatted[PASSPORT_UI_PRESENTATION_TEXT_CAPACITY];
    int length;
    if (!quota->available) {
        length = snprintf(formatted, sizeof(formatted), "NO DATA");
    } else if ((quota->used_percent_present &&
                !percent_is_valid(quota->used_percent)) ||
               (quota->remaining_percent_present &&
                !percent_is_valid(quota->remaining_percent))) {
        /* Keep availability visible, but never show an invalid numeric value. */
        length = snprintf(formatted, sizeof(formatted), "AVAILABLE");
    } else if (quota->used_percent_present &&
               quota->remaining_percent_present) {
        /* The card caption marks this compact pair as used/remaining %. The
         * worst-case "100/100" uses 7 glyphs and advances about 52.6 px in
         * the configured Montserrat 14 font, inside the 82 px value label. */
        length = snprintf(formatted, sizeof(formatted),
                          "AVAILABLE\n%u/%u",
                          rounded_percent(quota->used_percent),
                          rounded_percent(quota->remaining_percent));
    } else if (quota->used_percent_present) {
        length = snprintf(formatted, sizeof(formatted),
                          "AVAILABLE\nUSED %u%%",
                          rounded_percent(quota->used_percent));
    } else if (quota->remaining_percent_present) {
        length = snprintf(formatted, sizeof(formatted),
                          "AVAILABLE\nLEFT %u%%",
                          rounded_percent(quota->remaining_percent));
    } else {
        length = snprintf(formatted, sizeof(formatted), "AVAILABLE");
    }

    if (length < 0 || (size_t)length >= sizeof(formatted) ||
        (size_t)length >= destination_capacity) {
        /* Keep the source's explicit availability if details cannot fit. */
        return copy_availability(quota, destination, destination_capacity);
    }
    memcpy(destination, formatted, (size_t)length + 1u);
    return true;
}

bool passport_ui_present_field(char *destination,
                               size_t destination_capacity,
                               const char *source,
                               size_t source_capacity,
                               bool rejected)
{
    static const char placeholder[] = "--";
    if (!destination || destination_capacity == 0) return false;
    destination[0] = '\0';
    if (destination_capacity < sizeof(placeholder)) return false;

    if (rejected || !source) {
        memcpy(destination, placeholder, sizeof(placeholder));
        return true;
    }

    size_t length = 0;
    while (length < source_capacity && source[length] != '\0') ++length;
    if (length == 0 || length == source_capacity ||
        length >= destination_capacity) {
        memcpy(destination, placeholder, sizeof(placeholder));
        return true;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}
