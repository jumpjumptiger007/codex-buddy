#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "passport_ui_presenter.h"

static void test_quota_availability_and_source_values(void)
{
    char text[PASSPORT_UI_PRESENTATION_TEXT_CAPACITY];
    passport_ui_quota_view_t quota = {0};

    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "NO DATA") == 0);

    quota.available = true;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE") == 0);

    quota.used_percent_present = true;
    quota.used_percent = 72.5;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE\nUSED 73%") == 0);

    quota.used_percent_present = false;
    quota.remaining_percent_present = true;
    quota.remaining_percent = 27.5;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE\nLEFT 28%") == 0);

    quota.used_percent_present = true;
    quota.used_percent = 72.5;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE\n73/28") == 0);

    quota.used_percent = 100.0;
    quota.remaining_percent = 100.0;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE\n100/100") == 0);

    quota.used_percent = NAN;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE") == 0);
    quota.used_percent = INFINITY;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE") == 0);
    quota.used_percent = -0.1;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE") == 0);
    quota.used_percent = 100.1;
    assert(passport_ui_present_quota(&quota, text, sizeof(text)));
    assert(strcmp(text, "AVAILABLE") == 0);

    quota.used_percent = 43.0;
    quota.remaining_percent = 61.0;
    char fallback[PASSPORT_UI_PRESENTATION_TEXT_CAPACITY];
    assert(passport_ui_present_quota(&quota, fallback, 10));
    assert(strcmp(fallback, "AVAILABLE") == 0);
    fallback[0] = 'x';
    assert(!passport_ui_present_quota(&quota, fallback, 9));
    assert(fallback[0] == '\0');

    text[0] = 'x';
    assert(!passport_ui_present_quota(&quota, text, 4));
    assert(text[0] == '\0');
    assert(!passport_ui_present_quota(NULL, text, sizeof(text)));
    assert(text[0] == '\0');
}

static void test_bounded_field_fallbacks(void)
{
    char text[PASSPORT_UI_ACTIVITY_TEXT_CAPACITY];
    static const char activity[] = "Building firmware";
    char maximum_project[PASSPORT_UI_PROJECT_TEXT_CAPACITY];
    char maximum_activity[PASSPORT_UI_ACTIVITY_TEXT_CAPACITY];

    memset(maximum_project, 'P', sizeof(maximum_project) - 1u);
    maximum_project[sizeof(maximum_project) - 1u] = '\0';
    memset(maximum_activity, 'A', sizeof(maximum_activity) - 1u);
    maximum_activity[sizeof(maximum_activity) - 1u] = '\0';

    char project_copy[PASSPORT_UI_PROJECT_TEXT_CAPACITY];
    assert(passport_ui_present_field(project_copy, sizeof(project_copy),
                                     maximum_project, sizeof(maximum_project),
                                     false));
    assert(strcmp(project_copy, maximum_project) == 0);
    assert(passport_ui_present_field(text, sizeof(text), maximum_activity,
                                     sizeof(maximum_activity), false));
    assert(strcmp(text, maximum_activity) == 0);

    assert(passport_ui_present_field(text, sizeof(text), activity,
                                     sizeof(activity), false));
    assert(strcmp(text, activity) == 0);

    assert(passport_ui_present_field(text, sizeof(text), "", 1, false));
    assert(strcmp(text, "--") == 0);

    assert(passport_ui_present_field(text, sizeof(text), activity,
                                     sizeof(activity), true));
    assert(strcmp(text, "--") == 0);

    char unterminated[] = { 'x', 'y', 'z' };
    assert(passport_ui_present_field(text, sizeof(text), unterminated,
                                     sizeof(unterminated), false));
    assert(strcmp(text, "--") == 0);

    assert(passport_ui_present_field(text, sizeof(text), activity,
                                     sizeof(activity), false));
    assert(!passport_ui_present_field(text, 2, activity,
                                      sizeof(activity), false));
    assert(text[0] == '\0');
    assert(!passport_ui_present_field(NULL, sizeof(text), activity,
                                      sizeof(activity), false));
}

int main(void)
{
    test_quota_availability_and_source_values();
    test_bounded_field_fallbacks();
    puts("Passport UI presenter tests: PASS");
    return 0;
}
