#include "Trip_Graphic.h"
#include "Product_TripIcons.h"
#include "Trip_Layout.h"
#include "SpeedHome_Layout.h"
#include "src/misc/lv_area_private.h"
#define MOVING 0x48B6D0U
#define STOPPED 0xEEE5BCU
/* LVGL snapshots layer->_clip_area into each queued draw task. Restrict each
 * half independently, then restore it before any following command. The two
 * halves never overlap even during alpha fade: no yellow under the blue and
 * no offscreen opacity layer. Unknown ratio draws a neutral capsule. */
static void Draw(lv_event_t *e)
{
    TripGraphic *s=lv_event_get_user_data(e);if(!s)return;
    lv_layer_t *layer=lv_event_get_layer(e);lv_area_t origin;lv_obj_get_coords(s->root,&origin);
    lv_area_t pill={origin.x1+16,origin.y1+93,origin.x1+319,origin.y1+108};
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=8;d.bg_opa=s->alpha;
    d.bg_color=lv_color_hex(0x405159);
    if(!s->known)lv_draw_rect(layer,&d,&pill);
    else{
        uint32_t ratio=s->ratio>1000?1000:s->ratio;int split=pill.x1+(304*(int)ratio+500)/1000;
        lv_area_t saved=layer->_clip_area,half=pill,clip;
        for(uint32_t n=0;n<2;++n){half=pill;if(n)half.x1=split;else half.x2=split-1;
            if(half.x1<=half.x2&&lv_area_intersect(&clip,&saved,&half)){
                layer->_clip_area=clip;d.bg_color=lv_color_hex(n?STOPPED:MOVING);lv_draw_rect(layer,&d,&pill);
            }
        }layer->_clip_area=saved;
    }
    /* Thin fixed center tick gives an unambiguous50:50 reference while the
     * blue/yellow boundary follows moving/(moving+stopped) exactly. */
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.width=1;line.opa=s->alpha;
    line.color=lv_color_hex(0x93A6AD);line.p1.x=line.p2.x=origin.x1+168;
    line.p1.y=origin.y1+112;line.p2.y=origin.y1+116;lv_draw_line(layer,&line);
    /* Four cached ROM masks share the section's transition opacity. The
     * distance pair stays24px; speed marks retain their20px optical size. */
    for(uint32_t n=0;n<4;++n){
        lv_draw_image_dsc_t icon;lv_draw_image_dsc_init(&icon);icon.src=Product_TripIcon(n);
        icon.opa=s->alpha;icon.recolor_opa=255;icon.recolor=lv_color_hex(0xF2F5F7);
        icon.scale_x=icon.scale_y=n<2?213:256;icon.pivot.x=icon.pivot.y=0;
        int x=origin.x1+(n<2?(n?171:18):(n==PRODUCT_TRIP_SIGNPOST?TRIP_DISTANCE_SIGN_X:TRIP_DISTANCE_BIKE_X));
        int y=origin.y1+(n<2?212:145);
        lv_area_t a={x,y,x+23,y+23};lv_draw_image(layer,&icon,&a);
    }
    /* Signpost . . . motorcycle, before the fixed distance/unit slots. The
     * dots are2px wide with6px clear gaps, rather than a cramped ellipsis. */
    d.radius=1;d.bg_opa=s->alpha;d.bg_color=lv_color_hex(0xF2F5F7);
    for(int i=0;i<3;++i){int x=origin.x1+TRIP_DISTANCE_DOTS_X+TRIP_DISTANCE_DOT_STEP*i;
        lv_area_t dot={x,origin.y1+163,x+1,origin.y1+164};lv_draw_rect(layer,&d,&dot);}
}
uint32_t TripGraphic_Create(TripGraphic *s,lv_obj_t *parent)
{
    if(!s||!parent)return 0;
    *s=(TripGraphic){0};s->alpha=255;s->root=lv_obj_create(parent);if(!s->root)return 0;
    lv_obj_remove_style_all(s->root);lv_obj_set_size(s->root,336,SPEED_HOME_CONTENT_HEIGHT-40);
    lv_obj_remove_flag(s->root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(s->root,Draw,LV_EVENT_DRAW_MAIN,s);lv_obj_add_flag(s->root,LV_OBJ_FLAG_HIDDEN);return 1;
}
void TripGraphic_Update(TripGraphic *s,uint32_t show,uint32_t known,uint32_t ratio)
{
    if(!s||!s->root)return;
    lv_obj_set_flag(s->root,LV_OBJ_FLAG_HIDDEN,!show);
    if(s->known!=known||s->ratio!=ratio){
        s->known=known;s->ratio=ratio;lv_obj_invalidate(s->root);}
}
void TripGraphic_Alpha(TripGraphic *s,uint32_t alpha)
{
    if(!s||!s->root||s->alpha==alpha)return;
    s->alpha=alpha;lv_obj_invalidate(s->root);
}
