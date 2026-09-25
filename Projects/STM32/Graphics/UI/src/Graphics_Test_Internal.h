#ifndef GRAPHICS_TEST_INTERNAL_H
#define GRAPHICS_TEST_INTERNAL_H

#include "Graphics_Test.h"
#include "Graphics.h"

/* 화면 하나가 소유하는 임시 참조다. 실제 객체/series 메모리는 LVGL 부모 삭제로
 * 반환한다. 외부 task나 하드웨어 포인터를 저장하지 않는다. */
typedef struct {
    lv_obj_t *body;
    lv_obj_t *obj[6];
    void *extra;
    uint32_t step;
} GT_Context;

typedef struct {
    GraphicsTest_CaseInfo info;
    uint32_t (*build)(GT_Context *ctx);
    void (*tick)(GT_Context *ctx, uint32_t elapsed_ms, uint32_t speed);
} GT_Case;

#define GT_FONT_SMALL (&lv_font_montserrat_14)
#define GT_FONT_BODY  (&lv_font_montserrat_20)
#define GT_FONT_TITLE (&lv_font_montserrat_28)
#define GT_FONT_SPEED (&lv_font_montserrat_40)
#define GT_TEXT_COLOR 0xDBE7F3U
#define GT_ACCENT 0x42D7C5U
#define GT_BG 0x101B2CU
#define GT_PANEL 0x1B2A40U
#define GT_NEW(c,n,expr) do { (c)->obj[n]=(expr); if(!GT_Check((c)->obj[n])) return 0U; } while(0)
#define GT_ENSURE(expr) do { if(!GT_Check(expr)) return 0U; } while(0)

uint32_t GT_Check(const void *ptr);
lv_obj_t *GT_Label(lv_obj_t *parent, const char *text, int32_t x, int32_t y,
                   int32_t width, const lv_font_t *font);
lv_obj_t *GT_Box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
void GT_Place(lv_obj_t *obj, int32_t x, int32_t y, int32_t w, int32_t h);
/* 시험의464x300 논리 좌표를 실제348x225 content에3/4로 배치한다.
 * GPU 객체 확대/축소나 framebuffer 변환을 사용하지 않는다. */
int32_t GT_Px(int32_t logical);
void GT_Position(lv_obj_t *obj, int32_t x, int32_t y);
void GT_Focus(lv_obj_t *obj);
void GT_FlatTree(lv_obj_t *obj);
void GT_Dense(lv_obj_t *obj);
void GT_Event(lv_event_t *event);
uint32_t GT_Skip(GT_Context *ctx);
void GT_NoTick(GT_Context *ctx, uint32_t elapsed_ms, uint32_t speed);
extern const lv_image_dsc_t gt_image_a, gt_image_b, gt_image_alpha;

extern const GT_Case gt_dashboard, gt_controls, gt_meters, gt_chart, gt_table;
extern const GT_Case gt_scale, gt_spinbox, gt_dropdown, gt_roller, gt_keyboard;
extern const GT_Case gt_buttonmatrix, gt_tabview, gt_tileview, gt_window, gt_menu;
extern const GT_Case gt_msgbox, gt_spinner, gt_span, gt_image, gt_animimage;
extern const GT_Case gt_imagebutton, gt_line_led, gt_calendar, gt_list, gt_flex;
extern const GT_Case gt_grid, gt_scroll, gt_events, gt_animation, gt_labels;
extern const GT_Case gt_skip_canvas, gt_skip_layer, gt_skip_shadow, gt_skip_gradient;
extern const GT_Case gt_skip_mask, gt_skip_svg, gt_skip_lottie, gt_skip_file_image;
extern const GT_Case gt_skip_arclabel, gt_skip_texture3d, gt_skip_ime;

#endif
