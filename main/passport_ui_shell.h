#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "passport_ui_model.h"
#include "passport_ui_presenter.h"

#define PASSPORT_UI_SHELL_OBJECT_MIN 20u
#define PASSPORT_UI_SHELL_OBJECT_MAX 30u
#define PASSPORT_UI_SHELL_OBJECT_COUNT 25u

/*
 * The caller owns this shell for the lifetime of the product screen and must
 * zero-initialize it. All functions access LVGL and require the caller to hold
 * bsp_lvgl_lock() whenever called outside the LVGL task.
 */
typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *status_panel;
    lv_obj_t *status_label;
    lv_obj_t *character_panel;
    lv_obj_t *character_left_eye;
    lv_obj_t *character_right_eye;
    lv_obj_t *character_mouth;
    lv_obj_t *project_panel;
    lv_obj_t *project_caption;
    lv_obj_t *project_value;
    lv_obj_t *activity_panel;
    lv_obj_t *activity_caption;
    lv_obj_t *activity_value;
    lv_obj_t *short_quota_panel;
    lv_obj_t *short_quota_caption;
    lv_obj_t *short_quota_value;
    lv_obj_t *weekly_quota_panel;
    lv_obj_t *weekly_quota_caption;
    lv_obj_t *weekly_quota_value;
    lv_obj_t *ptt_hint;
    lv_obj_t *voice_panel;
    lv_obj_t *voice_label;
    lv_obj_t *toast_panel;
    lv_obj_t *toast_label;

    uint8_t text_bank;
    char project_text[2][PASSPORT_UI_PROJECT_TEXT_CAPACITY];
    char activity_text[2][PASSPORT_UI_ACTIVITY_TEXT_CAPACITY];
    char short_quota_text[2][PASSPORT_UI_PRESENTATION_TEXT_CAPACITY];
    char weekly_quota_text[2][PASSPORT_UI_PRESENTATION_TEXT_CAPACITY];
} passport_ui_shell_t;

#define PASSPORT_UI_SHELL_INITIALIZER {0}

/* Create and load the sole product root once. The initial view comes from U0/U1. */
bool passport_ui_shell_create(passport_ui_shell_t *shell,
                              const passport_ui_view_t *initial_view);

/* Update retained objects only; this function never creates or deletes objects. */
bool passport_ui_shell_update(passport_ui_shell_t *shell,
                              const passport_ui_view_t *view);
