#include "passport_ui_model.h"

#include <stddef.h>
#include <string.h>

static passport_ui_lifecycle_t normalize_lifecycle(passport_ui_lifecycle_t value)
{
    switch (value) {
    case PASSPORT_UI_LIFECYCLE_OFFLINE:
    case PASSPORT_UI_LIFECYCLE_IDLE:
    case PASSPORT_UI_LIFECYCLE_WORKING:
    case PASSPORT_UI_LIFECYCLE_ATTENTION:
    case PASSPORT_UI_LIFECYCLE_DONE:
        return value;
    default:
        return PASSPORT_UI_LIFECYCLE_OFFLINE;
    }
}

static passport_ui_voice_overlay_t normalize_voice(
    passport_ui_voice_overlay_t value)
{
    switch (value) {
    case PASSPORT_UI_VOICE_NONE:
    case PASSPORT_UI_VOICE_LISTENING:
    case PASSPORT_UI_VOICE_TRANSCRIBING:
    case PASSPORT_UI_VOICE_READY:
    case PASSPORT_UI_VOICE_FAILED:
        return value;
    default:
        return PASSPORT_UI_VOICE_NONE;
    }
}

/* Reject a whole over-limit field so the output is never a partial string. */
static bool copy_text(char *destination, size_t capacity, const char *source)
{
    size_t length = 0;

    destination[0] = '\0';
    if (!source) return false;

    while (length < capacity && source[length] != '\0') ++length;
    if (length == capacity) return true;

    memcpy(destination, source, length);
    destination[length] = '\0';
    return false;
}

static passport_ui_character_t character_for_lifecycle(
    passport_ui_lifecycle_t lifecycle)
{
    switch (lifecycle) {
    case PASSPORT_UI_LIFECYCLE_IDLE:
        return PASSPORT_UI_CHARACTER_IDLE;
    case PASSPORT_UI_LIFECYCLE_WORKING:
        return PASSPORT_UI_CHARACTER_WORKING;
    case PASSPORT_UI_LIFECYCLE_ATTENTION:
        return PASSPORT_UI_CHARACTER_ATTENTION;
    case PASSPORT_UI_LIFECYCLE_DONE:
        return PASSPORT_UI_CHARACTER_DONE;
    case PASSPORT_UI_LIFECYCLE_OFFLINE:
    default:
        return PASSPORT_UI_CHARACTER_OFFLINE;
    }
}

static void copy_quota(const passport_ui_quota_input_t *input,
                       passport_ui_quota_view_t *view)
{
    memset(view, 0, sizeof(*view));
    if (!input || !input->available) return;

    view->available = true;
    view->used_percent_present = input->used_percent_present;
    if (input->used_percent_present) view->used_percent = input->used_percent;
    view->remaining_percent_present = input->remaining_percent_present;
    if (input->remaining_percent_present) {
        view->remaining_percent = input->remaining_percent;
    }
    view->reset_at_epoch_seconds_present =
        input->reset_at_epoch_seconds_present;
    if (input->reset_at_epoch_seconds_present) {
        view->reset_at_epoch_seconds = input->reset_at_epoch_seconds;
    }
    view->reset_marker_present = input->reset_marker_present;
    if (input->reset_marker_present) view->reset_marker = input->reset_marker;
}

void passport_ui_view_init(passport_ui_view_t *view)
{
    if (!view) return;
    memset(view, 0, sizeof(*view));
    view->lifecycle = PASSPORT_UI_LIFECYCLE_OFFLINE;
    view->voice_overlay = PASSPORT_UI_VOICE_NONE;
    view->character = PASSPORT_UI_CHARACTER_OFFLINE;
    view->short_window_minutes = PASSPORT_UI_SHORT_WINDOW_MINUTES;
    view->weekly_window_minutes = PASSPORT_UI_WEEKLY_WINDOW_MINUTES;
}

bool passport_ui_view_map(const passport_ui_input_t *input,
                          passport_ui_view_t *view)
{
    if (!input || !view) return false;

    passport_ui_view_init(view);
    view->lifecycle = normalize_lifecycle(input->lifecycle);
    if ((input->outcome == PASSPORT_UI_OUTCOME_FAILED ||
         input->outcome == PASSPORT_UI_OUTCOME_ABORTED) &&
        view->lifecycle == PASSPORT_UI_LIFECYCLE_DONE) {
        /* A failed or aborted outcome can never produce a success presentation. */
        view->lifecycle = PASSPORT_UI_LIFECYCLE_IDLE;
    }

    view->voice_overlay = normalize_voice(input->voice_overlay);
    view->character = character_for_lifecycle(view->lifecycle);

    /* Listening changes the character only when offline/attention has priority. */
    if (view->voice_overlay == PASSPORT_UI_VOICE_LISTENING &&
        view->lifecycle != PASSPORT_UI_LIFECYCLE_OFFLINE &&
        view->lifecycle != PASSPORT_UI_LIFECYCLE_ATTENTION) {
        view->character = PASSPORT_UI_CHARACTER_LISTENING;
    }

    view->project_text_rejected = copy_text(
        view->project_text, sizeof(view->project_text), input->project_text);
    view->activity_text_rejected = copy_text(
        view->activity_text, sizeof(view->activity_text), input->activity_text);

    view->short_window_minutes = PASSPORT_UI_SHORT_WINDOW_MINUTES;
    copy_quota(&input->short_window, &view->short_window);
    view->weekly_window_minutes = PASSPORT_UI_WEEKLY_WINDOW_MINUTES;
    copy_quota(&input->weekly_window, &view->weekly_window);
    return true;
}

void passport_ui_notification_map(
    passport_ui_notification_t notification,
    passport_ui_notification_view_t *view)
{
    if (!view) return;
    *view = (passport_ui_notification_view_t){0};
    view->notification = PASSPORT_UI_NOTIFICATION_NONE;

    switch (notification) {
    case PASSPORT_UI_NOTIFICATION_ATTENTION:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_ATTENTION;
        break;
    case PASSPORT_UI_NOTIFICATION_COMPLETED:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_DONE;
        break;
    case PASSPORT_UI_NOTIFICATION_RELIABLE_ERROR:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_ATTENTION;
        break;
    case PASSPORT_UI_NOTIFICATION_SHORT_QUOTA_RESET:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_QUOTA_RESET;
        view->quota_window_minutes = PASSPORT_UI_SHORT_WINDOW_MINUTES;
        break;
    case PASSPORT_UI_NOTIFICATION_WEEKLY_QUOTA_RESET:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_QUOTA_RESET;
        view->quota_window_minutes = PASSPORT_UI_WEEKLY_WINDOW_MINUTES;
        break;
    case PASSPORT_UI_NOTIFICATION_CONNECTION_LOST:
        view->active = true;
        view->notification = notification;
        view->character_override = PASSPORT_UI_CHARACTER_OFFLINE;
        break;
    case PASSPORT_UI_NOTIFICATION_NONE:
    default:
        break;
    }
}
