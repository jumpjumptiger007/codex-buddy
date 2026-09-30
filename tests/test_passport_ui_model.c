#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "passport_ui_model.h"

static void test_default_state(void)
{
    passport_ui_view_t view;
    passport_ui_view_init(&view);
    assert(view.lifecycle == PASSPORT_UI_LIFECYCLE_OFFLINE);
    assert(view.voice_overlay == PASSPORT_UI_VOICE_NONE);
    assert(view.character == PASSPORT_UI_CHARACTER_OFFLINE);
    assert(view.project_text[0] == '\0' && view.activity_text[0] == '\0');
    assert(view.short_window_minutes == PASSPORT_UI_SHORT_WINDOW_MINUTES);
    assert(view.weekly_window_minutes == PASSPORT_UI_WEEKLY_WINDOW_MINUTES);
    assert(!view.short_window.available && !view.weekly_window.available);
}

static passport_ui_input_t base_input(passport_ui_lifecycle_t lifecycle)
{
    passport_ui_input_t input = {0};
    input.lifecycle = lifecycle;
    return input;
}

static void test_lifecycle_and_voice_mapping(void)
{
    static const struct {
        passport_ui_lifecycle_t lifecycle;
        passport_ui_character_t character;
    } cases[] = {
        {PASSPORT_UI_LIFECYCLE_OFFLINE, PASSPORT_UI_CHARACTER_OFFLINE},
        {PASSPORT_UI_LIFECYCLE_IDLE, PASSPORT_UI_CHARACTER_IDLE},
        {PASSPORT_UI_LIFECYCLE_WORKING, PASSPORT_UI_CHARACTER_WORKING},
        {PASSPORT_UI_LIFECYCLE_ATTENTION, PASSPORT_UI_CHARACTER_ATTENTION},
        {PASSPORT_UI_LIFECYCLE_DONE, PASSPORT_UI_CHARACTER_DONE},
    };
    static const passport_ui_voice_overlay_t voices[] = {
        PASSPORT_UI_VOICE_NONE,
        PASSPORT_UI_VOICE_LISTENING,
        PASSPORT_UI_VOICE_TRANSCRIBING,
        PASSPORT_UI_VOICE_READY,
        PASSPORT_UI_VOICE_FAILED,
    };
    passport_ui_view_t view;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        passport_ui_input_t input = base_input(cases[i].lifecycle);
        assert(passport_ui_view_map(&input, &view));
        assert(view.lifecycle == cases[i].lifecycle);
        assert(view.character == cases[i].character);
    }

    for (size_t i = 0; i < sizeof(voices) / sizeof(voices[0]); ++i) {
        passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_WORKING);
        input.voice_overlay = voices[i];
        assert(passport_ui_view_map(&input, &view));
        assert(view.voice_overlay == voices[i]);
        assert(view.lifecycle == PASSPORT_UI_LIFECYCLE_WORKING);
        assert(view.character == (voices[i] == PASSPORT_UI_VOICE_LISTENING
                                      ? PASSPORT_UI_CHARACTER_LISTENING
                                      : PASSPORT_UI_CHARACTER_WORKING));
    }

    passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_ATTENTION);
    input.voice_overlay = PASSPORT_UI_VOICE_LISTENING;
    assert(passport_ui_view_map(&input, &view));
    assert(view.lifecycle == PASSPORT_UI_LIFECYCLE_ATTENTION);
    assert(view.voice_overlay == PASSPORT_UI_VOICE_LISTENING);
    assert(view.character == PASSPORT_UI_CHARACTER_ATTENTION);
}

static void test_failed_or_aborted_work_never_looks_done(void)
{
    const passport_ui_outcome_t outcomes[] = {
        PASSPORT_UI_OUTCOME_FAILED,
        PASSPORT_UI_OUTCOME_ABORTED,
    };
    passport_ui_view_t view;

    for (size_t i = 0; i < sizeof(outcomes) / sizeof(outcomes[0]); ++i) {
        passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_DONE);
        input.outcome = outcomes[i];
        assert(passport_ui_view_map(&input, &view));
        assert(view.lifecycle == PASSPORT_UI_LIFECYCLE_IDLE);
        assert(view.character == PASSPORT_UI_CHARACTER_IDLE);
    }

    passport_ui_input_t success = base_input(PASSPORT_UI_LIFECYCLE_DONE);
    success.outcome = PASSPORT_UI_OUTCOME_SUCCESS;
    assert(passport_ui_view_map(&success, &view));
    assert(view.lifecycle == PASSPORT_UI_LIFECYCLE_DONE);
    assert(view.character == PASSPORT_UI_CHARACTER_DONE);
}

static void test_quota_availability_and_source_values(void)
{
    passport_ui_view_t view;
    passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_IDLE);
    input.short_window = (passport_ui_quota_input_t){
        .available = true,
        .used_percent_present = true,
        .used_percent = 72.5,
        .remaining_percent_present = true,
        .remaining_percent = 27.5,
        .reset_at_epoch_seconds_present = true,
        .reset_at_epoch_seconds = 1900000000,
        .reset_marker_present = true,
        .reset_marker = 41,
    };
    input.weekly_window = (passport_ui_quota_input_t){
        .available = true,
        .used_percent_present = true,
        .used_percent = 15.0,
    };
    assert(passport_ui_view_map(&input, &view));
    assert(view.short_window_minutes == 300 && view.short_window.available);
    assert(view.short_window.used_percent_present &&
           view.short_window.used_percent == 72.5);
    assert(view.short_window.remaining_percent_present &&
           view.short_window.remaining_percent == 27.5);
    assert(view.short_window.reset_at_epoch_seconds_present &&
           view.short_window.reset_at_epoch_seconds == 1900000000);
    assert(view.short_window.reset_marker_present &&
           view.short_window.reset_marker == 41);
    assert(view.weekly_window_minutes == 10080 && view.weekly_window.available);
    assert(view.weekly_window.used_percent_present &&
           view.weekly_window.used_percent == 15.0);
    assert(!view.weekly_window.remaining_percent_present);

    input.short_window.available = false;
    assert(passport_ui_view_map(&input, &view));
    assert(!view.short_window.available && view.weekly_window.available);
    assert(!view.short_window.used_percent_present &&
           !view.short_window.reset_marker_present);

    input.short_window.available = true;
    input.weekly_window.available = false;
    assert(passport_ui_view_map(&input, &view));
    assert(view.short_window.available && !view.weekly_window.available);
    assert(!view.weekly_window.used_percent_present &&
           !view.weekly_window.reset_at_epoch_seconds_present);

    input.short_window.available = false;
    assert(passport_ui_view_map(&input, &view));
    assert(!view.short_window.available && !view.weekly_window.available);
    assert(!view.short_window.used_percent_present &&
           !view.weekly_window.used_percent_present);
}

static void test_notifications_are_separate_facts(void)
{
    static const struct {
        passport_ui_notification_t notification;
        passport_ui_character_t character;
        uint32_t window_minutes;
    } cases[] = {
        {PASSPORT_UI_NOTIFICATION_ATTENTION, PASSPORT_UI_CHARACTER_ATTENTION, 0},
        {PASSPORT_UI_NOTIFICATION_COMPLETED, PASSPORT_UI_CHARACTER_DONE, 0},
        {PASSPORT_UI_NOTIFICATION_RELIABLE_ERROR, PASSPORT_UI_CHARACTER_ATTENTION, 0},
        {PASSPORT_UI_NOTIFICATION_SHORT_QUOTA_RESET,
         PASSPORT_UI_CHARACTER_QUOTA_RESET, 300},
        {PASSPORT_UI_NOTIFICATION_WEEKLY_QUOTA_RESET,
         PASSPORT_UI_CHARACTER_QUOTA_RESET, 10080},
        {PASSPORT_UI_NOTIFICATION_CONNECTION_LOST,
         PASSPORT_UI_CHARACTER_OFFLINE, 0},
    };
    passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_WORKING);
    passport_ui_view_t persistent_before;
    passport_ui_view_t persistent_after;
    passport_ui_notification_view_t notification_view;

    assert(passport_ui_view_map(&input, &persistent_before));
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        passport_ui_notification_map(cases[i].notification, &notification_view);
        assert(notification_view.active);
        assert(notification_view.notification == cases[i].notification);
        assert(notification_view.character_override == cases[i].character);
        assert(notification_view.quota_window_minutes == cases[i].window_minutes);
        assert(passport_ui_view_map(&input, &persistent_after));
        assert(memcmp(&persistent_before, &persistent_after,
                      sizeof(persistent_before)) == 0);
    }
    passport_ui_notification_map(PASSPORT_UI_NOTIFICATION_NONE,
                                 &notification_view);
    assert(!notification_view.active);
    assert(notification_view.character_override == PASSPORT_UI_CHARACTER_NONE);
}

static void test_bounded_text_fields(void)
{
    char project[PASSPORT_UI_PROJECT_TEXT_CAPACITY + 1];
    char overlong[PASSPORT_UI_ACTIVITY_TEXT_CAPACITY + 1];
    passport_ui_input_t input = base_input(PASSPORT_UI_LIFECYCLE_IDLE);
    passport_ui_view_t view;

    memset(project, 'P', sizeof(project) - 2);
    project[sizeof(project) - 2] = '\0';
    memset(overlong, 'A', sizeof(overlong) - 1);
    overlong[sizeof(overlong) - 1] = '\0';
    input.project_text = project;
    input.activity_text = "Building";
    assert(passport_ui_view_map(&input, &view));
    assert(!view.project_text_rejected);
    assert(strlen(view.project_text) == PASSPORT_UI_PROJECT_TEXT_CAPACITY - 1);
    assert(strcmp(view.activity_text, "Building") == 0);

    memset(project, 'X', sizeof(project) - 1);
    project[sizeof(project) - 1] = '\0';
    input.project_text = project;
    input.activity_text = overlong;
    assert(passport_ui_view_map(&input, &view));
    assert(view.project_text_rejected && view.project_text[0] == '\0');
    assert(view.activity_text_rejected && view.activity_text[0] == '\0');
}

int main(void)
{
    test_default_state();
    test_lifecycle_and_voice_mapping();
    test_failed_or_aborted_work_never_looks_done();
    test_quota_availability_and_source_values();
    test_notifications_are_separate_facts();
    test_bounded_text_fields();
    puts("Passport UI model tests: PASS");
    return 0;
}
