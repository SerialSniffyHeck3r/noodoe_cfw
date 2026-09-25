#include "ScreenWarning_View.h"
#include "Product_FuelVector.h"
#include "Settings_Icons.h"
#include "Product_Fonts.h"
#include <string.h>
static lv_obj_t *root,*label;
static ScreenWarningState shown;
static void Draw(lv_event_t *event)
{
    if(!shown.active)return;
    lv_layer_t *layer=lv_event_get_layer(event);
    Product_FuelVector(layer,240,shown.y+(int)shown.size/2,shown.size,shown.color,shown.opacity);
    if(shown.icon){lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);
        d.src=Settings_Icon(SETTINGS_ICON_WARNING);d.scale_x=d.scale_y=768;d.pivot.x=d.pivot.y=0;
        d.opa=shown.opacity;d.recolor=lv_color_hex(shown.color);d.recolor_opa=255;
        lv_area_t a={290,245,313,268};lv_draw_image(layer,&d,&a);}
}
/* Created once above normal views; draw jobs retain the ordinary EVE clip,
 * swap fence and snapshot contract. No display commands from button handlers. */
uint32_t ScreenWarningView_Create(lv_obj_t *parent)
{
    root=lv_obj_create(parent);if(!root)return 0;
    lv_obj_remove_style_all(root);lv_obj_set_size(root,480,480);
    lv_obj_set_style_bg_color(root,lv_color_black(),0);lv_obj_set_style_bg_opa(root,235,0);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root,Draw,LV_EVENT_DRAW_MAIN,NULL);
    label=lv_label_create(root);if(!label)return 0;
    lv_obj_remove_style_all(label);lv_obj_set_size(label,360,68);lv_obj_set_pos(label,60,354);
    lv_obj_set_style_text_font(label,Product_TextFont(32),0);lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_label_set_text_static(label,shown.message);
    lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);return 1;
}
void ScreenWarningView_Render(const ScreenWarningState *state)
{
    if(!root||!state)return;
    if(!memcmp(&shown,state,sizeof(shown)))return;
    shown=*state;lv_obj_set_flag(root,LV_OBJ_FLAG_HIDDEN,!shown.active);
    lv_obj_set_style_text_color(label,lv_color_hex(shown.color),0);
    lv_obj_set_style_text_opa(label,shown.phase==0?0:shown.phase==2?255:(432-shown.size)*255U/96U,0);
    lv_obj_set_y(label,354+(shown.size-336)*30/96);
    lv_label_set_text_static(label,shown.message);lv_obj_invalidate(root);
}
