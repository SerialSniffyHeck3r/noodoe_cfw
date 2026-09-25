#include "Button_Hints.h"
#include "ButtonFeedback.h"
#include "BSP_Buttons.h"
#include "Product_MusicIcons.h"
#include "Product_TripIcons.h"
#include <string.h>
static struct {
    lv_obj_t *root;ButtonHintBinding rows[3];uint32_t scope,alpha,now;
    uint32_t seen[3];const lv_image_dsc_t *released_icon[3];
} hints;
static uint32_t Color(uint32_t phase)
{return phase==1?0x42A5F5U:phase==2?0xFF5252U:0xF2F5F7U;}
static void Icon(lv_layer_t *layer,const lv_image_dsc_t *image,int x,int y,uint32_t size,uint32_t phase)
{
    if(!image)return;
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=image;d.opa=hints.alpha;
    d.recolor_opa=255;d.recolor=lv_color_hex(Color(phase));d.pivot.x=d.pivot.y=0;
    d.scale_x=d.scale_y=size*256U/24U;lv_area_t a={x,y,x+23,y+23};lv_draw_image(layer,&d,&a);
}
/* Integer rounding:6px dot->5px,18x6 capsule->14x5. Functional20px
 * glyphs->25px; physical24px key symbols retain their established location. */
static void Action(lv_layer_t *layer,const ButtonHintBinding *row,uint32_t hold,int x,int y,int icon_x)
{
    const lv_image_dsc_t *image=hold?row->hold:row->tap;if(!image)return;
    uint32_t phase=ButtonFeedback_ActionPhase(row->button,hints.scope,hold,hints.now);
    if(phase&&hints.released_icon[row->button])image=hints.released_icon[row->button];
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=2;d.bg_opa=hints.alpha;d.bg_color=lv_color_hex(Color(phase));
    lv_area_t a={x,y-2,x+(hold?13:4),y+2};lv_draw_rect(layer,&d,&a);
    Icon(layer,image,icon_x,y-12,25,phase);
}
static void Draw(lv_event_t *event)
{
    lv_layer_t *layer=lv_event_get_layer(event);if(!hints.alpha)return;
    for(uint32_t i=0;i<3;++i){const ButtonHintBinding *r=&hints.rows[i];if(r->button>=3)continue;
        int y=188+(int)i*40;
        Icon(layer,r->key,421,y-12,24,ButtonFeedback_KeyPhase(r->button,hints.scope));
        if(r->tap)Action(layer,r,0,r->hold?303:366,y,r->hold?316:380);
        if(r->hold)Action(layer,r,1,355,y,380);
    }
}
uint32_t ButtonHints_Create(lv_obj_t *shell)
{
    if(!shell)return 0;
    memset(&hints,0,sizeof(hints));hints.root=lv_obj_create(shell);if(!hints.root)return 0;
    lv_obj_remove_style_all(hints.root);lv_obj_set_size(hints.root,480,480);
    lv_obj_remove_flag(hints.root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hints.root,Draw,LV_EVENT_DRAW_MAIN,NULL);return 1;
}
void ButtonHints_Update(const ButtonHintBinding rows[3],uint32_t scope,uint32_t alpha,uint32_t now)
{
    if(!hints.root)return;
    /* Snapshot the symbol BEFORE a toggle replaces play with pause. A1s
     * release pulse refers to the action that was pressed, not its successor. */
    if(rows)for(uint32_t i=0;i<3;++i){uint32_t b=rows[i].button;if(b>=3)continue;
        ButtonFeedbackKey *k=&g_button_feedback.keys[b];
        if(k->release_serial!=hints.seen[b]){
            const ButtonHintBinding *previous=hints.scope==scope?&hints.rows[i]:&rows[i];
            hints.released_icon[b]=k->release_long?previous->hold:previous->tap;hints.seen[b]=k->release_serial;
        }
    }
    if(rows)memcpy(hints.rows,rows,sizeof(hints.rows));
    hints.scope=scope;hints.alpha=rows?alpha*ButtonFeedback_Visibility(now)/255U:0;hints.now=now;
    lv_obj_invalidate(hints.root);
}

/* Keep shared hints above the full-screen settings root. */
void ButtonHints_Reparent(lv_obj_t *parent)
{if(parent&&hints.root){lv_obj_set_parent(hints.root,parent);lv_obj_move_foreground(hints.root);}}

/* No action bindings, dots or capsules outside Music. This facade keeps the
 * same physical key placement and activity/color feedback across all pages. */
void ButtonHints_ShowKeys(uint32_t scope,uint32_t alpha,uint32_t now)
{
    const ButtonHintBinding rows[3]={
        {BSP_BUTTON_UP,Product_TripIcon(PRODUCT_TRIP_MAXIMUM),NULL,NULL},
        {BSP_BUTTON_ENTER,Product_MusicIcon(MUSIC_ICON_CIRCLE),NULL,NULL},
        {BSP_BUTTON_DOWN,Product_MusicIcon(MUSIC_ICON_DOWN),NULL,NULL}};
    ButtonHints_Update(rows,scope,alpha,now);
}
