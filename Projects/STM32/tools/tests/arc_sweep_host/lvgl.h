#ifndef ARC_SWEEP_TEST_LVGL_H
#define ARC_SWEEP_TEST_LVGL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 실제 production C를 ARM에서 실행하기 위한 LVGL 경계 대역이다. 시간/기하/
 * easing/수명 로직은 대체하지 않고 객체 생성/삭제/속성 변경만 기록한다. */
typedef struct { int32_t x1,y1,x2,y2; } lv_area_t;
typedef struct { uint16_t line_height; } lv_font_t;
typedef uint32_t lv_color_t;
typedef struct lv_obj_t lv_obj_t;
typedef struct { lv_obj_t *target; } lv_event_t;
struct lv_obj_t {
    lv_obj_t *parent, *child, *next;
    void (*deleted)(lv_event_t *);
    uint32_t live, kind, flags, rounded[3], width[3], bg_opa[3];
    int32_t x,y,w,h,min,max,value,start,end,pad[3];
    const lv_font_t *font;
    char text[100];
};
extern const lv_font_t lv_font_montserrat_14, lv_font_montserrat_40;
#define LV_PART_MAIN 0U
#define LV_PART_INDICATOR 1U
#define LV_PART_KNOB 2U
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_TEXT_ALIGN_CENTER 1
#define LV_LABEL_LONG_DOT 1
#define LV_OBJ_FLAG_CLICKABLE 1U
#define LV_OBJ_FLAG_SCROLLABLE 2U
#define LV_OBJ_FLAG_CLICK_FOCUSABLE 4U
#define LV_ARC_MODE_NORMAL 0
#define LV_EVENT_DELETE 1
static inline lv_color_t lv_color_hex(uint32_t c) { return c; }
lv_obj_t *lv_obj_create(lv_obj_t *parent);
lv_obj_t *lv_arc_create(lv_obj_t *parent);
lv_obj_t *lv_label_create(lv_obj_t *parent);
void lv_obj_delete(lv_obj_t *obj);
void lv_obj_add_event_cb(lv_obj_t *,void (*)(lv_event_t *),uint32_t,void *);
lv_obj_t *lv_event_get_target(lv_event_t *event);
void lv_obj_remove_style_all(lv_obj_t *obj);
void lv_obj_update_layout(lv_obj_t *obj);
void lv_obj_get_content_coords(lv_obj_t *obj,lv_area_t *area);
void lv_obj_set_pos(lv_obj_t *obj,int32_t x,int32_t y);
void lv_obj_set_size(lv_obj_t *obj,int32_t w,int32_t h);
void lv_obj_remove_flag(lv_obj_t *obj,uint32_t flags);
void lv_obj_set_style_text_font(lv_obj_t *,const lv_font_t *,uint32_t);
#define STYLE_DECL(n) void lv_obj_set_style_##n(lv_obj_t *,int32_t,uint32_t)
STYLE_DECL(text_color); STYLE_DECL(text_align); STYLE_DECL(text_opa);
STYLE_DECL(pad_all); STYLE_DECL(arc_width); STYLE_DECL(arc_color);
STYLE_DECL(arc_opa); STYLE_DECL(arc_rounded); STYLE_DECL(bg_opa);
STYLE_DECL(border_width); STYLE_DECL(shadow_width); STYLE_DECL(outline_width);
void lv_label_set_long_mode(lv_obj_t *obj,uint32_t mode);
void lv_label_set_text(lv_obj_t *obj,const char *text);
void lv_label_set_text_fmt(lv_obj_t *obj,const char *format,...);
void lv_arc_set_mode(lv_obj_t *obj,uint32_t mode);
void lv_arc_set_range(lv_obj_t *obj,int32_t min,int32_t max);
void lv_arc_set_bg_angles(lv_obj_t *obj,int32_t start,int32_t end);
void lv_arc_set_value(lv_obj_t *obj,int32_t value);
#endif
