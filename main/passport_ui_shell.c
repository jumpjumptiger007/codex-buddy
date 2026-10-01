#include "passport_ui_shell.h"

#include <stddef.h>
#include <string.h>

#include "bsp_pins.h"

_Static_assert(PASSPORT_UI_SHELL_OBJECT_COUNT >= PASSPORT_UI_SHELL_OBJECT_MIN,
               "Product shell is below its documented object inventory");
_Static_assert(PASSPORT_UI_SHELL_OBJECT_COUNT <= PASSPORT_UI_SHELL_OBJECT_MAX,
               "Product shell exceeds its documented object inventory");

#define COLOR_BACKGROUND 0x0B1422
#define COLOR_PANEL 0x19283A
#define COLOR_STATUS 0x163B42
#define COLOR_ACCENT 0x79D7C8
#define COLOR_TEXT 0xEAF1F3
#define COLOR_MUTED 0x91A6B6
#define COLOR_FACE 0x637CE8
#define COLOR_FACE_DETAIL 0x10192B
#define COLOR_TOAST 0x5B4823

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

static const char *quota_availability_text(bool available)
{
    return available ? "AVAILABLE" : "NO DATA";
}

static void copy_view_text(char *destination,
                           size_t destination_capacity,
                           const char *source,
                           size_t source_capacity)
{
    static const char placeholder[] = "--";
    size_t length = 0;

    if (!source || destination_capacity < sizeof(placeholder)) {
        if (destination_capacity > 0) destination[0] = '\0';
        return;
    }

    while (length < source_capacity && source[length] != '\0') ++length;
    if (length == 0 || length == source_capacity ||
        length >= destination_capacity) {
        memcpy(destination, placeholder, sizeof(placeholder));
        return;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
}

bool passport_ui_shell_update(passport_ui_shell_t *shell,
                              const passport_ui_view_t *view)
{
    if (!shell || !shell->root || !view) return false;

    const uint8_t next_bank = (uint8_t)(shell->text_bank ^ 1u);
    copy_view_text(shell->project_text[next_bank],
                   sizeof(shell->project_text[next_bank]),
                   view->project_text,
                   sizeof(view->project_text));
    copy_view_text(shell->activity_text[next_bank],
                   sizeof(shell->activity_text[next_bank]),
                   view->activity_text,
                   sizeof(view->activity_text));

    shell->text_bank = next_bank;
    lv_label_set_text_static(shell->status_label,
                             lifecycle_text(view->lifecycle));
    lv_label_set_text_static(shell->project_value,
                             shell->project_text[next_bank]);
    lv_label_set_text_static(shell->activity_value,
                             shell->activity_text[next_bank]);
    lv_label_set_text_static(
        shell->short_quota_value,
        quota_availability_text(view->short_window.available));
    lv_label_set_text_static(
        shell->weekly_quota_value,
        quota_availability_text(view->weekly_window.available));
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

    CREATE(title, create_label(shell->root, 32, 36, 176, 28,
                               &lv_font_montserrat_20, COLOR_TEXT,
                               LV_TEXT_ALIGN_CENTER, "CODEX BUDDY"));

    CREATE(status_panel, create_panel(shell->root, 24, 70, 192, 38,
                                      COLOR_STATUS, 12));
    CREATE(status_label, create_label(shell->status_panel, 6, 4, 180, 30,
                                      &lv_font_montserrat_20, COLOR_ACCENT,
                                      LV_TEXT_ALIGN_CENTER, "OFFLINE"));

    CREATE(character_panel, create_panel(shell->root, 84, 114, 72, 64,
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

    CREATE(project_panel, create_panel(shell->root, 24, 188, 192, 29,
                                       COLOR_PANEL, 8));
    CREATE(project_caption, create_label(shell->project_panel, 8, 2, 54, 25,
                                         &lv_font_montserrat_14, COLOR_MUTED,
                                         LV_TEXT_ALIGN_LEFT, "PROJECT"));
    CREATE(project_value, create_label(shell->project_panel, 64, 2, 120, 25,
                                       &lv_font_montserrat_14, COLOR_TEXT,
                                       LV_TEXT_ALIGN_LEFT, "--"));

    CREATE(activity_panel, create_panel(shell->root, 24, 221, 192, 29,
                                         COLOR_PANEL, 8));
    CREATE(activity_caption, create_label(shell->activity_panel,
                                          8, 2, 54, 25,
                                          &lv_font_montserrat_14, COLOR_MUTED,
                                          LV_TEXT_ALIGN_LEFT, "ACTIVITY"));
    CREATE(activity_value, create_label(shell->activity_panel,
                                        64, 2, 120, 25,
                                        &lv_font_montserrat_14, COLOR_TEXT,
                                        LV_TEXT_ALIGN_LEFT, "--"));

    CREATE(short_quota_panel, create_panel(shell->root, 24, 254, 92, 34,
                                            COLOR_PANEL, 8));
    CREATE(short_quota_caption, create_label(shell->short_quota_panel,
                                             5, 1, 82, 15,
                                             &lv_font_montserrat_14, COLOR_MUTED,
                                             LV_TEXT_ALIGN_CENTER, "5 HOUR"));
    CREATE(short_quota_value, create_label(shell->short_quota_panel,
                                           4, 17, 84, 15,
                                           &lv_font_montserrat_14, COLOR_TEXT,
                                           LV_TEXT_ALIGN_CENTER, "NO DATA"));

    CREATE(weekly_quota_panel, create_panel(shell->root, 124, 254, 92, 34,
                                             COLOR_PANEL, 8));
    CREATE(weekly_quota_caption, create_label(shell->weekly_quota_panel,
                                              5, 1, 82, 15,
                                              &lv_font_montserrat_14, COLOR_MUTED,
                                              LV_TEXT_ALIGN_CENTER, "WEEK"));
    CREATE(weekly_quota_value, create_label(shell->weekly_quota_panel,
                                            4, 17, 84, 15,
                                            &lv_font_montserrat_14, COLOR_TEXT,
                                            LV_TEXT_ALIGN_CENTER, "NO DATA"));

    CREATE(voice_panel, create_panel(shell->root, 36, 290, 168, 24,
                                      COLOR_STATUS, 10));
    CREATE(voice_label, create_label(shell->voice_panel, 8, 2, 152, 20,
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
