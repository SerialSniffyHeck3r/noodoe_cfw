#include "Product_StatusIconsView.h"
#include "Product_StatusIcons.h"
#include "Product_CallIcon.h"
#include "Settings_Icons.h"
static lv_obj_t *icons[2];
static lv_obj_t *dash_badge;
static uint32_t call_active;
/* A rounded image style requests a bitmap clip mask, which this EVE port
 * does not support. Draw the circle before the ordinary24px A4 image instead;
 * the image keeps radius0 and uses the same path as every other status icon. */
static void DashBackground(lv_event_t *event)
{
    lv_area_t area;lv_obj_get_coords(lv_event_get_current_target(event),&area);
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.radius=16;d.bg_color=lv_color_hex(0x123B78);d.bg_opa=255;
    lv_draw_rect(lv_event_get_layer(event),&d,&area);
}
void ProductStatusIcons_SetCall(uint32_t active)
{active=!!active;if(icons[0]&&call_active!=active){call_active=active;lv_image_set_src(icons[0],active?Product_CallIcon():Product_StatusIcon(1));}}
uint32_t ProductStatusIcons_Create(lv_obj_t *parent)
{
    for(uint32_t i=0;i<2;i++){
        icons[i]=lv_image_create(parent);if(!icons[i])return 0;
        lv_image_set_src(icons[i],Product_StatusIcon(i+1));
        /* Match the EVE A4 image-origin adapter, including at unit scale. */
        /* 24px masks sit below the clock's sloping sides, above the main
         * viewport (Y105). Mirror about X240; do not move the clock or ring. */
        lv_image_set_pivot(icons[i],0,0);lv_obj_set_pos(icons[i],i?344:112,73);
        lv_obj_set_style_image_recolor_opa(icons[i],255,0);
    }
    /* Persistent switch indicator, outside the central336px viewport. Use
     * the existing Google Material Round speed mask; no new asset package.
     * The shell owns its lifetime; this left gutter stays outside settings
     * rows and beneath power, installation and recovery overlays. */
    dash_badge=lv_image_create(parent);if(!dash_badge)return 0;
    lv_image_set_src(dash_badge,Settings_Icon(SETTINGS_ICON_SPEED));lv_image_set_pivot(dash_badge,0,0);
    lv_obj_remove_style_all(dash_badge);lv_obj_set_pos(dash_badge,38,224);lv_obj_set_size(dash_badge,32,32);
    lv_obj_remove_flag(dash_badge,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(dash_badge,DashBackground,LV_EVENT_DRAW_MAIN_BEGIN,NULL);
    /* The24px mask is centered in the32px image object's round background. */
    lv_obj_set_style_image_recolor(dash_badge,lv_color_hex(0x8CCFFF),0);
    lv_obj_set_style_image_recolor_opa(dash_badge,255,0);
    lv_obj_add_flag(dash_badge,LV_OBJ_FLAG_HIDDEN);return 1;
}
void ProductStatusIcons_Render(uint32_t visible,uint32_t bt,uint32_t gps,uint32_t dash_mode)
{
    for(uint32_t i=0;i<2;i++)if(icons[i]){
        lv_obj_set_flag(icons[i],LV_OBJ_FLAG_HIDDEN,!visible);
        lv_obj_set_style_image_recolor(icons[i],lv_color_hex(i?gps:bt),0);
    }
    if(dash_badge)lv_obj_set_flag(dash_badge,LV_OBJ_FLAG_HIDDEN,!dash_mode);
}
