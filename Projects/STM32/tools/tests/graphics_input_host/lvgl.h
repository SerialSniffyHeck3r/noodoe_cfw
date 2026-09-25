#ifndef GRAPHICS_INPUT_HOST_LVGL_H
#define GRAPHICS_INPUT_HOST_LVGL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { int32_t x1,y1,x2,y2; } lv_area_t;

/* 실제 Graphics.h/graphics_internal.h를 그대로 컴파일하기 위한 LVGL 경계다.
 * production 입력 로직은 이 파일에 구현하지 않는다. mock은 호출 순서와
 * 삭제된 indev 접근만 감시하고 timer callback은 시험이 명시적으로 호출한다. */
typedef struct { uint32_t id; } lv_display_t;
typedef struct { uint32_t id; } lv_group_t;
typedef struct { uint32_t id; } lv_obj_t;
typedef struct { uint32_t id; } lv_image_dsc_t;
typedef struct { uint32_t id; } lv_font_t;
typedef enum { LV_INDEV_STATE_RELEASED = 0, LV_INDEV_STATE_PRESSED = 1 } lv_indev_state_t;
typedef enum { LV_INDEV_TYPE_ENCODER = 3 } lv_indev_type_t;
typedef struct { int16_t enc_diff; lv_indev_state_t state; } lv_indev_data_t;
typedef struct lv_indev_t lv_indev_t;
typedef void (*lv_indev_read_cb_t)(lv_indev_t *, lv_indev_data_t *);
struct lv_indev_t {
    uint32_t alive, type;
    lv_indev_read_cb_t read_cb;
    lv_display_t *display;
    lv_group_t *group;
};
lv_indev_t *lv_indev_create(void);
void lv_indev_delete(lv_indev_t *device);
void lv_indev_reset(lv_indev_t *device, lv_obj_t *object);
void lv_indev_set_type(lv_indev_t *device, lv_indev_type_t type);
void lv_indev_set_read_cb(lv_indev_t *device, lv_indev_read_cb_t read_cb);
void lv_indev_set_display(lv_indev_t *device, lv_display_t *display);
void lv_indev_set_group(lv_indev_t *device, lv_group_t *group);
void lv_deinit(void);
#endif
