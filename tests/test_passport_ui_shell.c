#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "passport_ui_shell.h"

#define FAKE_OBJECT_CAPACITY 32

static lv_obj_t s_objects[FAKE_OBJECT_CAPACITY];
static size_t s_object_count;
static size_t s_root_count;
static size_t s_screen_load_count;
static size_t s_delete_count;
lv_font_t lv_font_montserrat_14;
lv_font_t lv_font_montserrat_20;

static lv_obj_t *new_object(lv_obj_t *parent)
{
    assert(s_object_count < FAKE_OBJECT_CAPACITY);
    lv_obj_t *object = &s_objects[s_object_count++];
    *object = (lv_obj_t){ .parent = parent };
    if (!parent) ++s_root_count;
    return object;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
    return new_object(parent);
}

lv_obj_t *lv_label_create(lv_obj_t *parent)
{
    return new_object(parent);
}

void lv_obj_set_pos(lv_obj_t *object, int32_t x, int32_t y)
{
    object->x = x;
    object->y = y;
}

void lv_obj_set_size(lv_obj_t *object, int32_t width, int32_t height)
{
    object->width = width;
    object->height = height;
}

void lv_obj_set_style_bg_color(lv_obj_t *object, lv_color_t color, uint32_t selector)
{
    (void)selector;
    object->background = color;
}

void lv_obj_set_style_bg_opa(lv_obj_t *object, uint8_t opacity, uint32_t selector)
{
    (void)object;
    (void)opacity;
    (void)selector;
}

void lv_obj_set_style_border_width(lv_obj_t *object, int32_t width, uint32_t selector)
{
    (void)object;
    (void)width;
    (void)selector;
}

void lv_obj_set_style_radius(lv_obj_t *object, int32_t radius, uint32_t selector)
{
    (void)object;
    (void)radius;
    (void)selector;
}

void lv_obj_set_style_pad_all(lv_obj_t *object, int32_t padding, uint32_t selector)
{
    (void)object;
    (void)padding;
    (void)selector;
}

void lv_obj_set_style_text_font(lv_obj_t *object,
                                const lv_font_t *font,
                                uint32_t selector)
{
    (void)object;
    (void)font;
    (void)selector;
}

void lv_obj_set_style_text_color(lv_obj_t *object,
                                 lv_color_t color,
                                 uint32_t selector)
{
    (void)object;
    (void)color;
    (void)selector;
}

void lv_obj_set_style_text_align(lv_obj_t *object,
                                 lv_text_align_t alignment,
                                 uint32_t selector)
{
    (void)object;
    (void)alignment;
    (void)selector;
}

void lv_obj_add_flag(lv_obj_t *object, lv_obj_flag_t flag)
{
    if (flag == LV_OBJ_FLAG_HIDDEN) object->hidden = true;
    if (flag == LV_OBJ_FLAG_SCROLLABLE) object->scrollable = true;
}

void lv_obj_remove_flag(lv_obj_t *object, lv_obj_flag_t flag)
{
    if (flag == LV_OBJ_FLAG_HIDDEN) object->hidden = false;
    if (flag == LV_OBJ_FLAG_SCROLLABLE) object->scrollable = false;
}

void lv_label_set_text_static(lv_obj_t *label, const char *text)
{
    label->text = text;
}

void lv_screen_load(lv_obj_t *screen)
{
    assert(screen && !screen->parent);
    ++s_screen_load_count;
}

void lv_obj_delete(lv_obj_t *object)
{
    assert(object);
    object->deleted = true;
    ++s_delete_count;
}

static void reset_fake_lvgl(void)
{
    memset(s_objects, 0, sizeof(s_objects));
    s_object_count = 0;
    s_root_count = 0;
    s_screen_load_count = 0;
    s_delete_count = 0;
}

static void assert_complete_inventory(const passport_ui_shell_t *shell)
{
    const lv_obj_t *objects[] = {
        shell->root,
        shell->title,
        shell->status_panel,
        shell->status_label,
        shell->character_panel,
        shell->character_left_eye,
        shell->character_right_eye,
        shell->character_mouth,
        shell->project_panel,
        shell->project_caption,
        shell->project_value,
        shell->activity_panel,
        shell->activity_caption,
        shell->activity_value,
        shell->short_quota_panel,
        shell->short_quota_caption,
        shell->short_quota_value,
        shell->weekly_quota_panel,
        shell->weekly_quota_caption,
        shell->weekly_quota_value,
        shell->voice_panel,
        shell->voice_label,
        shell->toast_panel,
        shell->toast_label,
    };

    _Static_assert(sizeof(objects) / sizeof(objects[0]) ==
                       PASSPORT_UI_SHELL_OBJECT_COUNT,
                   "Test inventory must match the documented shell inventory");
    for (size_t i = 0; i < sizeof(objects) / sizeof(objects[0]); ++i) {
        assert(objects[i]);
    }
    assert(s_object_count == PASSPORT_UI_SHELL_OBJECT_COUNT);
}

static void test_initial_offline_root_and_placeholders(void)
{
    reset_fake_lvgl();
    passport_ui_view_t view;
    passport_ui_view_init(&view);
    passport_ui_shell_t shell = PASSPORT_UI_SHELL_INITIALIZER;

    assert(PASSPORT_UI_SHELL_OBJECT_COUNT >= PASSPORT_UI_SHELL_OBJECT_MIN);
    assert(PASSPORT_UI_SHELL_OBJECT_COUNT <= PASSPORT_UI_SHELL_OBJECT_MAX);
    assert(passport_ui_shell_create(&shell, &view));
    assert_complete_inventory(&shell);
    assert(s_root_count == 1);
    assert(s_screen_load_count == 1);
    assert(s_delete_count == 0);
    assert(strcmp(shell.status_label->text, "OFFLINE") == 0);
    assert(strcmp(shell.project_value->text, "--") == 0);
    assert(strcmp(shell.activity_value->text, "--") == 0);
    assert(strcmp(shell.short_quota_value->text, "NO DATA") == 0);
    assert(strcmp(shell.weekly_quota_value->text, "NO DATA") == 0);
    assert(shell.voice_panel->hidden);
    assert(shell.toast_panel->hidden);

    const size_t objects_before_second_create = s_object_count;
    assert(!passport_ui_shell_create(&shell, &view));
    assert(s_object_count == objects_before_second_create);
    assert(s_root_count == 1 && s_screen_load_count == 1);
}

static void test_updates_reuse_persistent_objects(void)
{
    reset_fake_lvgl();
    passport_ui_view_t initial;
    passport_ui_view_init(&initial);
    passport_ui_shell_t shell = PASSPORT_UI_SHELL_INITIALIZER;
    assert(passport_ui_shell_create(&shell, &initial));

    const size_t objects_after_create = s_object_count;
    lv_obj_t *const root = shell.root;
    passport_ui_view_t next;
    passport_ui_view_init(&next);
    next.lifecycle = PASSPORT_UI_LIFECYCLE_WORKING;
    strcpy(next.project_text, "codex-buddy");
    strcpy(next.activity_text, "Building");
    next.short_window.available = true;
    next.weekly_window.available = true;
    next.voice_overlay = PASSPORT_UI_VOICE_LISTENING;

    assert(passport_ui_shell_update(&shell, &next));
    assert(shell.root == root);
    assert(s_object_count == objects_after_create);
    assert(s_root_count == 1);
    assert(s_screen_load_count == 1);
    assert(s_delete_count == 0);
    assert(strcmp(shell.status_label->text, "WORKING") == 0);
    assert(strcmp(shell.project_value->text, "codex-buddy") == 0);
    assert(strcmp(shell.activity_value->text, "Building") == 0);
    assert(strcmp(shell.short_quota_value->text, "AVAILABLE") == 0);
    assert(strcmp(shell.weekly_quota_value->text, "AVAILABLE") == 0);
    assert(shell.voice_panel->hidden);
    assert(shell.toast_panel->hidden);
}

static void test_unterminated_model_text_fails_closed(void)
{
    reset_fake_lvgl();
    passport_ui_view_t view;
    passport_ui_view_init(&view);
    memset(view.project_text, 'P', sizeof(view.project_text));
    memset(view.activity_text, 'A', sizeof(view.activity_text));

    passport_ui_shell_t shell = PASSPORT_UI_SHELL_INITIALIZER;
    assert(passport_ui_shell_create(&shell, &view));
    assert(strcmp(shell.project_value->text, "--") == 0);
    assert(strcmp(shell.activity_value->text, "--") == 0);
}

int main(void)
{
    test_initial_offline_root_and_placeholders();
    test_updates_reuse_persistent_objects();
    test_unterminated_model_text_fails_closed();
    puts("Passport UI shell tests: PASS");
    return 0;
}
