#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t lv_color_t;
typedef uint32_t lv_obj_flag_t;

typedef struct {
    int unused;
} lv_font_t;

typedef enum {
    LV_TEXT_ALIGN_LEFT = 0,
    LV_TEXT_ALIGN_CENTER,
    LV_TEXT_ALIGN_RIGHT,
} lv_text_align_t;

typedef enum {
    LV_LABEL_LONG_MODE_WRAP = 0,
    LV_LABEL_LONG_MODE_DOTS,
    LV_LABEL_LONG_MODE_SCROLL,
    LV_LABEL_LONG_MODE_SCROLL_CIRCULAR,
    LV_LABEL_LONG_MODE_CLIP,
} lv_label_long_mode_t;

typedef struct _lv_obj_t {
    struct _lv_obj_t *parent;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    lv_color_t background;
    lv_color_t text_color;
    const char *text;
    lv_label_long_mode_t long_mode;
    bool hidden;
    bool scrollable;
    bool deleted;
} lv_obj_t;

extern lv_font_t lv_font_montserrat_14;
extern lv_font_t lv_font_montserrat_20;

#define LV_OPA_COVER 255u
#define LV_OBJ_FLAG_HIDDEN 1u
#define LV_OBJ_FLAG_SCROLLABLE 2u

static inline lv_color_t lv_color_hex(uint32_t color)
{
    return color;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent);
lv_obj_t *lv_label_create(lv_obj_t *parent);
void lv_obj_set_pos(lv_obj_t *object, int32_t x, int32_t y);
void lv_obj_set_size(lv_obj_t *object, int32_t width, int32_t height);
void lv_obj_set_style_bg_color(lv_obj_t *object, lv_color_t color, uint32_t selector);
void lv_obj_set_style_bg_opa(lv_obj_t *object, uint8_t opacity, uint32_t selector);
void lv_obj_set_style_border_width(lv_obj_t *object, int32_t width, uint32_t selector);
void lv_obj_set_style_radius(lv_obj_t *object, int32_t radius, uint32_t selector);
void lv_obj_set_style_pad_all(lv_obj_t *object, int32_t padding, uint32_t selector);
void lv_obj_set_style_text_font(lv_obj_t *object,
                                const lv_font_t *font,
                                uint32_t selector);
void lv_obj_set_style_text_color(lv_obj_t *object,
                                 lv_color_t color,
                                 uint32_t selector);
void lv_obj_set_style_text_align(lv_obj_t *object,
                                 lv_text_align_t alignment,
                                 uint32_t selector);
void lv_obj_add_flag(lv_obj_t *object, lv_obj_flag_t flag);
void lv_obj_remove_flag(lv_obj_t *object, lv_obj_flag_t flag);
void lv_label_set_text_static(lv_obj_t *label, const char *text);
void lv_label_set_long_mode(lv_obj_t *label, lv_label_long_mode_t long_mode);
void lv_screen_load(lv_obj_t *screen);
void lv_obj_delete(lv_obj_t *object);
