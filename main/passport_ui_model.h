#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Application-side bounds; capacities include the trailing NUL byte. */
#define PASSPORT_UI_PROJECT_TEXT_CAPACITY 48u
#define PASSPORT_UI_ACTIVITY_TEXT_CAPACITY 96u

#define PASSPORT_UI_SHORT_WINDOW_MINUTES 300u
#define PASSPORT_UI_WEEKLY_WINDOW_MINUTES 10080u

typedef enum {
    PASSPORT_UI_LIFECYCLE_OFFLINE = 0,
    PASSPORT_UI_LIFECYCLE_IDLE,
    PASSPORT_UI_LIFECYCLE_WORKING,
    PASSPORT_UI_LIFECYCLE_ATTENTION,
    PASSPORT_UI_LIFECYCLE_DONE,
} passport_ui_lifecycle_t;

typedef enum {
    PASSPORT_UI_OUTCOME_NONE = 0,
    PASSPORT_UI_OUTCOME_SUCCESS,
    PASSPORT_UI_OUTCOME_FAILED,
    PASSPORT_UI_OUTCOME_ABORTED,
} passport_ui_outcome_t;

typedef enum {
    PASSPORT_UI_VOICE_NONE = 0,
    PASSPORT_UI_VOICE_LISTENING,
    PASSPORT_UI_VOICE_TRANSCRIBING,
    PASSPORT_UI_VOICE_READY,
    PASSPORT_UI_VOICE_FAILED,
} passport_ui_voice_overlay_t;

typedef enum {
    PASSPORT_UI_CHARACTER_NONE = 0,
    PASSPORT_UI_CHARACTER_OFFLINE,
    PASSPORT_UI_CHARACTER_IDLE,
    PASSPORT_UI_CHARACTER_WORKING,
    PASSPORT_UI_CHARACTER_ATTENTION,
    PASSPORT_UI_CHARACTER_DONE,
    PASSPORT_UI_CHARACTER_LISTENING,
    PASSPORT_UI_CHARACTER_QUOTA_RESET,
} passport_ui_character_t;

/*
 * Transport-neutral normalized data for the Passport application layer.
 * This synthetic/mock-facing contract is not a BLE payload or wire schema.
 * Percentages and reset data are copied only when their presence flags are set;
 * the mapper never derives one quota value from another or from token counts.
 */
typedef struct {
    bool available;
    bool used_percent_present;
    double used_percent;
    bool remaining_percent_present;
    double remaining_percent;
    bool reset_at_epoch_seconds_present;
    uint64_t reset_at_epoch_seconds;
    bool reset_marker_present;
    uint64_t reset_marker;
} passport_ui_quota_input_t;

typedef struct {
    bool available;
    bool used_percent_present;
    double used_percent;
    bool remaining_percent_present;
    double remaining_percent;
    bool reset_at_epoch_seconds_present;
    uint64_t reset_at_epoch_seconds;
    bool reset_marker_present;
    uint64_t reset_marker;
} passport_ui_quota_view_t;

typedef struct {
    /* Already normalized by the source; Passport does not aggregate sessions. */
    passport_ui_lifecycle_t lifecycle;
    passport_ui_outcome_t outcome;
    passport_ui_voice_overlay_t voice_overlay;
    const char *project_text;
    const char *activity_text;
    passport_ui_quota_input_t short_window;
    passport_ui_quota_input_t weekly_window;
} passport_ui_input_t;

typedef struct {
    passport_ui_lifecycle_t lifecycle;
    passport_ui_voice_overlay_t voice_overlay;
    passport_ui_character_t character;
    char project_text[PASSPORT_UI_PROJECT_TEXT_CAPACITY];
    char activity_text[PASSPORT_UI_ACTIVITY_TEXT_CAPACITY];
    bool project_text_rejected;
    bool activity_text_rejected;
    uint32_t short_window_minutes;
    passport_ui_quota_view_t short_window;
    uint32_t weekly_window_minutes;
    passport_ui_quota_view_t weekly_window;
} passport_ui_view_t;

typedef enum {
    PASSPORT_UI_NOTIFICATION_NONE = 0,
    PASSPORT_UI_NOTIFICATION_ATTENTION,
    PASSPORT_UI_NOTIFICATION_COMPLETED,
    PASSPORT_UI_NOTIFICATION_RELIABLE_ERROR,
    PASSPORT_UI_NOTIFICATION_SHORT_QUOTA_RESET,
    PASSPORT_UI_NOTIFICATION_WEEKLY_QUOTA_RESET,
    /* Generic future link-loss event; it does not imply a BLE wire contract. */
    PASSPORT_UI_NOTIFICATION_CONNECTION_LOST,
} passport_ui_notification_t;

/*
 * A one-shot presentation fact, intentionally separate from persistent truth.
 * The caller decides how long to display it; this module owns no timer/task.
 */
typedef struct {
    bool active;
    passport_ui_notification_t notification;
    passport_ui_character_t character_override;
    uint32_t quota_window_minutes;
} passport_ui_notification_view_t;

/* Sets a deterministic OFFLINE/NONE/unavailable-quota default view. */
void passport_ui_view_init(passport_ui_view_t *view);

/* Map one normalized snapshot without heap allocation or transport dependencies. */
bool passport_ui_view_map(const passport_ui_input_t *input,
                          passport_ui_view_t *view);

/* Map one transient event independently of the persistent snapshot. */
void passport_ui_notification_map(
    passport_ui_notification_t notification,
    passport_ui_notification_view_t *view);
