#ifndef TEST_LVGL_H
#define TEST_LVGL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t lv_color_t;
typedef struct {int32_t x1,y1,x2,y2;} lv_area_t;
typedef struct {int32_t x,y;} point_t;
typedef struct {uint32_t id;} lv_obj_t;
typedef struct {uint32_t id;} lv_layer_t;
typedef struct {uint32_t id;} lv_event_t;
typedef struct {uint32_t id;} lv_image_dsc_t;
typedef struct {const void *src;int32_t scale_x,scale_y;point_t pivot;uint32_t opa,recolor_opa;lv_color_t recolor;} lv_draw_image_dsc_t;
#define LV_OBJ_FLAG_HIDDEN 8U
static inline void lv_obj_set_flag(lv_obj_t *p,uint32_t f,uint32_t on){(void)p;(void)f;(void)on;}
#define LV_OBJ_FLAG_CLICKABLE 1U
#define LV_OBJ_FLAG_SCROLLABLE 2U
#define LV_OBJ_FLAG_OVERFLOW_VISIBLE 4U
#define LV_EVENT_DRAW_MAIN 5U
lv_obj_t *lv_obj_create(lv_obj_t *);
void lv_obj_remove_style_all(lv_obj_t *);
void lv_obj_set_size(lv_obj_t *,int,int);
void lv_obj_set_pos(lv_obj_t *,int,int);
void lv_obj_remove_flag(lv_obj_t *,uint32_t);
void lv_obj_add_event_cb(lv_obj_t *,void (*)(lv_event_t *),uint32_t,void *);
void lv_obj_invalidate(lv_obj_t *);
void lv_obj_get_coords(lv_obj_t *,lv_area_t *);
lv_layer_t *lv_event_get_layer(lv_event_t *);
void lv_draw_image(lv_layer_t *,const lv_draw_image_dsc_t *,const lv_area_t *);
static inline void lv_draw_image_dsc_init(lv_draw_image_dsc_t *d){memset(d,0,sizeof(*d));}
static inline lv_color_t lv_color_hex(uint32_t c){return c;}
static inline lv_color_t lv_color_mix(lv_color_t a,lv_color_t b,uint32_t w)
{uint32_t c=0;for(uint32_t s=0;s<24;s+=8)c|=(((((a>>s)&255)*w)+(((b>>s)&255)*(255-w)))/255)<<s;return c;}
#endif
