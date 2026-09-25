#include "Background_View.h"
#include "Graphics_Background.h"
#include "Graphics_BackgroundDraw.h"

/* First shell child: every ring, separator, page, toast and glyph is drawn
 * above this object. Final shared circular stencil still removes all corners. */
static void Draw(lv_event_t *event)
{GraphicsBackground_Draw(lv_event_get_layer(event));}
uint32_t BackgroundView_Create(lv_obj_t *parent)
{
    if(!parent)return 0;
    lv_obj_t *object=lv_obj_create(parent);if(!object)return 0;
    lv_obj_remove_style_all(object);lv_obj_set_size(object,480,480);lv_obj_set_pos(object,0,0);
    lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object,Draw,LV_EVENT_DRAW_MAIN,NULL);GraphicsBackground_Init();return 1;
}
