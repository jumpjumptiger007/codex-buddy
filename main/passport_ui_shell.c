#include "passport_ui_shell.h"

#include <string.h>

#include "bsp_pins.h"

_Static_assert(PASSPORT_UI_SHELL_OBJECT_COUNT >= PASSPORT_UI_SHELL_OBJECT_MIN,
               "Product shell is below its documented object inventory");
_Static_assert(PASSPORT_UI_SHELL_OBJECT_COUNT <= PASSPORT_UI_SHELL_OBJECT_MAX,
               "Product shell exceeds its documented object inventory");

#define COLOR_BACKGROUND 0x0B1422
#define COLOR_PANEL 0x19283A
#define COLOR_STATUS_OFFLINE 0x27313D
#define COLOR_STATUS_IDLE 0x1E3247
#define COLOR_STATUS_WORKING 0x163B42
#define COLOR_STATUS_ATTENTION 0x55351F
#define COLOR_STATUS_DONE 0x1D4739
#define COLOR_ACCENT 0x79D7C8
#define COLOR_TEXT 0xEAF1F3
#define COLOR_MUTED 0x91A6B6
#define COLOR_STATUS_IDLE_TEXT 0xB8CBE0
#define COLOR_STATUS_ATTENTION_TEXT 0xFFC36A
#define COLOR_STATUS_DONE_TEXT 0x9BE6B5
#define COLOR_FACE 0x637CE8
#define COLOR_FACE_DETAIL 0x10192B
#define COLOR_TOAST 0x5B4823
#define COLOR_QUOTA_AVAILABLE 0x174039

typedef struct {
    uint32_t panel;
    uint32_t text;
} lifecycle_style_t;

static lv_obj_t *create_panel(lv_obj_t *parent,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              uint32_t color,
                              int32_t radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    if (!object) return NULL;

    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *create_label(lv_obj_t *parent,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height,
                              const lv_font_t *font,
                              uint32_t color,
                              lv_text_align_t alignment,
                              const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;

    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, alignment, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_label_set_text_static(label, text);
    return label;
}

static const char *lifecycle_text(passport_ui_lifecycle_t lifecycle)
{
    switch (lifecycle) {
    case PASSPORT_UI_LIFECYCLE_OFFLINE:
        return "OFFLINE";
    case PASSPORT_UI_LIFECYCLE_IDLE:
        return "IDLE";
    case PASSPORT_UI_LIFECYCLE_WORKING:
        return "WORKING";
    case PASSPORT_UI_LIFECYCLE_ATTENTION:
        return "ATTENTION";
    case PASSPORT_UI_LIFECYCLE_DONE:
        return "DONE";
    default:
        return "OFFLINE";
    }
}

static lifecycle_style_t lifecycle_style(passport_ui_lifecycle_t lifecycle)
{
    switch (lifecycle) {
    case PASSPORT_UI_LIFECYCLE_IDLE:
        return (lifecycle_style_t){ COLOR_STATUS_IDLE,
                                    COLOR_STATUS_IDLE_TEXT };
    case PASSPORT_UI_LIFECYCLE_WORKING:
        return (lifecycle_style_t){ COLOR_STATUS_WORKING, COLOR_ACCENT };
    case PASSPORT_UI_LIFECYCLE_ATTENTION:
        return (lifecycle_style_t){ COLOR_STATUS_ATTENTION,
                                    COLOR_STATUS_ATTENTION_TEXT };
    case PASSPORT_UI_LIFECYCLE_DONE:
        return (lifecycle_style_t){ COLOR_STATUS_DONE,
                                    COLOR_STATUS_DONE_TEXT };
    case PASSPORT_UI_LIFECYCLE_OFFLINE:
    default:
        return (lifecycle_style_t){ COLOR_STATUS_OFFLINE, COLOR_MUTED };
    }
}

bool passport_ui_shell_update(passport_ui_shell_t *shell,
                              const passport_ui_view_t *view)
{
    if (!shell || !shell->root || !view) return false;

    const uint8_t next_bank = (uint8_t)(shell->text_bank ^ 1u);
    if (!passport_ui_present_field(shell->project_text[next_bank],
                                   sizeof(shell->project_text[next_bank]),
                                   view->project_text,
                                   sizeof(view->project_text),
                                   view->project_text_rejected) ||
        !passport_ui_present_field(shell->activity_text[next_bank],
                                   sizeof(shell->activity_text[next_bank]),
                                   view->activity_text,
                                   sizeof(view->activity_text),
                                   view->activity_text_rejected) ||
        !passport_ui_present_quota(&view->short_window,
                                   shell->short_quota_text[next_bank],
                                   sizeof(shell->short_quota_text[next_bank])) ||
        !passport_ui_present_quota(&view->weekly_window,
                                   shell->weekly_quota_text[next_bank],
                                   sizeof(shell->weekly_quota_text[next_bank]))) {
        return false;
    }

    shell->text_bank = next_bank;
    const lifecycle_style_t status_style = lifecycle_style(view->lifecycle);
    lv_obj_set_style_bg_color(shell->status_panel,
                              lv_color_hex(status_style.panel), 0);
    lv_obj_set_style_text_color(shell->status_label,
                                lv_color_hex(status_style.text), 0);
    lv_label_set_text_static(shell->status_label,
                             lifecycle_text(view->lifecycle));
    lv_label_set_text_static(shell->project_value,
                             shell->project_text[next_bank]);
    lv_label_set_text_static(shell->activity_value,
                             shell->activity_text[next_bank]);

    const uint32_t short_quota_color = view->short_window.available
                                           ? COLOR_QUOTA_AVAILABLE
                                           : COLOR_PANEL;
    const uint32_t weekly_quota_color = view->weekly_window.available
                                            ? COLOR_QUOTA_AVAILABLE
                                            : COLOR_PANEL;
    lv_obj_set_style_bg_color(shell->short_quota_panel,
                              lv_color_hex(short_quota_color), 0);
    lv_obj_set_style_bg_color(shell->weekly_quota_panel,
                              lv_color_hex(weekly_quota_color), 0);
    lv_obj_set_style_text_color(shell->short_quota_value,
                                lv_color_hex(view->short_window.available
                                                 ? COLOR_ACCENT
                                                 : COLOR_MUTED),
                                0);
    lv_obj_set_style_text_color(shell->weekly_quota_value,
                                lv_color_hex(view->weekly_window.available
                                                 ? COLOR_ACCENT
                                                 : COLOR_MUTED),
                                0);
    lv_label_set_text_static(shell->short_quota_value,
                             shell->short_quota_text[next_bank]);
    lv_label_set_text_static(shell->weekly_quota_value,
                             shell->weekly_quota_text[next_bank]);
    return true;
}

bool passport_ui_shell_create(passport_ui_shell_t *shell,
                              const passport_ui_view_t *initial_view)
{
    if (!shell || !initial_view || shell->root) return false;

    shell->root = lv_obj_create(NULL);
    if (!shell->root) return false;
    lv_obj_set_size(shell->root, BSP_LCD_W, BSP_LCD_H);
    lv_obj_set_style_bg_color(shell->root, lv_color_hex(COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(shell->root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(shell->root, 0, 0);
    lv_obj_set_style_radius(shell->root, 0, 0);
    lv_obj_set_style_pad_all(shell->root, 0, 0);
    lv_obj_remove_flag(shell->root, LV_OBJ_FLAG_SCROLLABLE);

#define CREATE(member, expression) \
    do { \
        shell->member = (expression); \
        if (!shell->member) goto failed; \
    } while (0)

    CREATE(title, create_label(shell->root, 30, 30, 180, 22,
                               &lv_font_montserrat_20, COLOR_TEXT,
                               LV_TEXT_ALIGN_CENTER, "CODEX BUDDY"));

    CREATE(short_quota_panel, create_panel(shell->root, 30, 54, 86, 64,
                                            COLOR_PANEL, 8));
    CREATE(short_quota_caption, create_label(shell->short_quota_panel,
                                             2, 1, 82, 16,
                                             &lv_font_montserrat_14, COLOR_MUTED,
                                             LV_TEXT_ALIGN_CENTER, "5H U/R%"));
    CREATE(short_quota_value, create_label(shell->short_quota_panel,
                                           2, 18, 82, 44,
                                           &lv_font_montserrat_14, COLOR_MUTED,
                                           LV_TEXT_ALIGN_CENTER, "NO DATA"));

    CREATE(weekly_quota_panel, create_panel(shell->root, 124, 54, 86, 64,
                                             COLOR_PANEL, 8));
    CREATE(weekly_quota_caption, create_label(shell->weekly_quota_panel,
                                              2, 1, 82, 16,
                                              &lv_font_montserrat_14, COLOR_MUTED,
                                              LV_TEXT_ALIGN_CENTER, "WK U/R%"));
    CREATE(weekly_quota_value, create_label(shell->weekly_quota_panel,
                                            2, 18, 82, 44,
                                            &lv_font_montserrat_14, COLOR_MUTED,
                                            LV_TEXT_ALIGN_CENTER, "NO DATA"));

    CREATE(status_panel, create_panel(shell->root, 30, 122, 180, 34,
                                      COLOR_STATUS_OFFLINE, 11));
    CREATE(status_label, create_label(shell->status_panel, 4, 2, 172, 30,
                                      &lv_font_montserrat_20, COLOR_MUTED,
                                      LV_TEXT_ALIGN_CENTER, "OFFLINE"));

    CREATE(character_panel, create_panel(shell->root, 84, 160, 72, 64,
                                         COLOR_FACE, 20));
    CREATE(character_left_eye, create_panel(shell->character_panel,
                                             19, 19, 8, 8,
                                             COLOR_FACE_DETAIL, 4));
    CREATE(character_right_eye, create_panel(shell->character_panel,
                                              45, 19, 8, 8,
                                              COLOR_FACE_DETAIL, 4));
    CREATE(character_mouth, create_panel(shell->character_panel,
                                          27, 39, 18, 5,
                                          COLOR_FACE_DETAIL, 2));

    CREATE(project_panel, create_panel(shell->root, 30, 228, 180, 20,
                                       COLOR_PANEL, 7));
    CREATE(project_caption, create_label(shell->project_panel, 6, 0, 50, 20,
                                         &lv_font_montserrat_14, COLOR_MUTED,
                                         LV_TEXT_ALIGN_LEFT, "PROJECT"));
    CREATE(project_value, create_label(shell->project_panel, 60, 0, 114, 20,
                                       &lv_font_montserrat_14, COLOR_TEXT,
                                       LV_TEXT_ALIGN_LEFT, "--"));
    lv_label_set_long_mode(shell->project_value, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(shell->project_value, 114, 20);

    CREATE(activity_panel, create_panel(shell->root, 30, 251, 180, 20,
                                         COLOR_PANEL, 7));
    CREATE(activity_caption, create_label(shell->activity_panel,
                                          6, 0, 50, 20,
                                          &lv_font_montserrat_14, COLOR_MUTED,
                                          LV_TEXT_ALIGN_LEFT, "ACTIVITY"));
    CREATE(activity_value, create_label(shell->activity_panel,
                                        60, 0, 114, 20,
                                        &lv_font_montserrat_14, COLOR_TEXT,
                                        LV_TEXT_ALIGN_LEFT, "--"));
    lv_label_set_long_mode(shell->activity_value, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_size(shell->activity_value, 114, 20);

    CREATE(ptt_hint, create_label(shell->root, 30, 272, 180, 18,
                                  &lv_font_montserrat_14, COLOR_MUTED,
                                  LV_TEXT_ALIGN_CENTER, "HOLD OK TO TALK"));

    /* This hidden U4 placeholder is created after the U3 PTT hint so it can
     * overlay the same retained area when a future gate makes it visible. */
    CREATE(voice_panel, create_panel(shell->root, 30, 270, 180, 20,
                                      COLOR_STATUS_WORKING, 9));
    CREATE(voice_label, create_label(shell->voice_panel, 6, 2, 168, 16,
                                     &lv_font_montserrat_14, COLOR_ACCENT,
                                     LV_TEXT_ALIGN_CENTER, "VOICE"));
    lv_obj_add_flag(shell->voice_panel, LV_OBJ_FLAG_HIDDEN);

    CREATE(toast_panel, create_panel(shell->root, 38, 132, 164, 52,
                                      COLOR_TOAST, 12));
    CREATE(toast_label, create_label(shell->toast_panel, 8, 8, 148, 36,
                                     &lv_font_montserrat_14, COLOR_TEXT,
                                     LV_TEXT_ALIGN_CENTER, "NOTICE"));
    lv_obj_add_flag(shell->toast_panel, LV_OBJ_FLAG_HIDDEN);

#undef CREATE

    if (!passport_ui_shell_update(shell, initial_view)) goto failed;
    lv_screen_load(shell->root);
    return true;

failed:
#undef CREATE
    if (shell->root) lv_obj_delete(shell->root);
    memset(shell, 0, sizeof(*shell));
    return false;
}
