#include "Product_MaintenanceBar.h"
#include "Graphics_SubpixelArc.h"
/* Same subpixel EVE arc as the speed ring. Outer radius223 and width9 put
 * its center atY458.5, retaining the old bar's center; the ends curve upward.
 * The full136px span stays centered atX240 and clear of footer ink. */
static void Draw(lv_event_t *e)
{
    ProductMaintenanceBar *b=lv_event_get_user_data(e);
    lv_area_t a;lv_obj_get_coords(b->track,&a);
    lv_layer_t *layer=lv_event_get_layer(e);
    int y=a.y1+240;
    Graphics_DrawSubpixelArc(layer,a.x1+240,y,223,9,7300,10700,
        b->valid&&!b->ratio?0x5A2929U:0x243036U);
    if(b->valid&&b->ratio)Graphics_DrawSubpixelArc(layer,a.x1+240,y,223,9,
        10700-3400*b->ratio/1000,10700,b->ratio<=200?0xFFB000U:0x56C6A9U);
}
uint32_t ProductMaintenanceBar_Create(ProductMaintenanceBar *b,lv_obj_t *parent)
{
    if(!b||!parent)return 0;
    *b=(ProductMaintenanceBar){0};b->track=lv_obj_create(parent);if(!b->track)return 0;
    lv_obj_remove_style_all(b->track);lv_obj_set_size(b->track,480,480);
    lv_obj_remove_flag(b->track,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b->track,Draw,LV_EVENT_DRAW_MAIN,b);
    lv_obj_add_flag(b->track,LV_OBJ_FLAG_HIDDEN);return 1;
}
void ProductMaintenanceBar_Set(ProductMaintenanceBar *b,uint32_t visible,uint32_t valid,uint32_t ratio)
{
    if(!b||!b->track)return;
    visible=!!visible;valid=!!valid&&ratio<=1000;if(!valid)ratio=0;
    if(b->visible==visible&&b->valid==valid&&b->ratio==ratio)return;
    b->visible=visible;b->valid=valid;b->ratio=ratio;
    lv_obj_set_flag(b->track,LV_OBJ_FLAG_HIDDEN,!visible);lv_obj_invalidate(b->track);
}
