#include "Product_ModeStrip.h"
#include "Product_ModeIcons.h"
#include "Product_CallIcon.h"
#include "Page_Transition.h"
#include "Scalar_Transition.h"
#include <string.h>
#define PITCH 48
#define STRIP_WIDTH 240
#define STRIP_COUNT 7U
#define PERIOD (PITCH*STRIP_COUNT)
static const uint8_t visible_modes[STRIP_COUNT]={0,1,2,3,8,6,7};
static struct {lv_obj_t *root;PageTransition motion;ScalarTransition opacity;uint32_t weights[8];int32_t centers[8];} strip;
volatile ProductModeStripDiagnostics g_mode_strip;

/* LVGL queues the visible image tasks; EVE commands are issued only when the
 * normal draw unit consumes them. Do not write GPU commands in this event.
 * All centers translate together. The root clips partial icons at its edges;
 * the invisible antipodal icon wraps outside that clipping area.
 * A4 maps to EVE L4; tinting does not paint a colored square background. */
static void Draw(lv_event_t *e)
{
    if(!strip.motion.initialized||!strip.opacity.value)return;
    lv_area_t bounds;lv_obj_get_coords(strip.root,&bounds);
    lv_layer_t *layer=lv_event_get_layer(e);
    for(uint32_t i=0;i<STRIP_COUNT;++i){
        uint32_t weight=strip.weights[i],size=PRODUCT_MODE_ICON_SMALL+
            ((PRODUCT_MODE_ICON_LARGE-PRODUCT_MODE_ICON_SMALL)*weight+127U)/255U;
        int x=strip.centers[i]-(int)size/2;
        if(x+(int)size<=0||x>=STRIP_WIDTH)continue;
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=visible_modes[i]==8?Product_CallIcon():Product_ModeIcon(visible_modes[i]);
        d.scale_x=d.scale_y=(size*256U+12U)/24U;d.pivot.x=d.pivot.y=0;
        d.opa=strip.opacity.value;d.recolor_opa=255;d.recolor=lv_color_mix(lv_color_hex(0xFFFFFFU),lv_color_hex(0x666666U),weight);
        x+=bounds.x1;int y=bounds.y1+PRODUCT_MODE_STRIP_HEIGHT/2-(int)size/2;
        lv_area_t a={x,y,x+23,y+23};lv_draw_image(layer,&d,&a);
    }
}

/* One clipped viewport atscreenX120..359/Y97..136; centerY117 is6px higher.
 * At rest the selected
 * icon and two neighbors on each side are visible. The eight modes form a
 * circular strip, so7->0 is one pitch with no visible end/reset jump. */
uint32_t ProductModeStrip_Create(lv_obj_t *shell)
{
    if(!shell)return 0;
    memset(&strip,0,sizeof(strip));memset((void*)&g_mode_strip,0,sizeof(g_mode_strip));g_mode_strip.magic=0x4D4F4431U;
    strip.opacity.value=strip.opacity.target=255;
    strip.root=lv_obj_create(shell);if(!strip.root)return 0;
    lv_obj_remove_style_all(strip.root);lv_obj_set_size(strip.root,STRIP_WIDTH,PRODUCT_MODE_STRIP_HEIGHT);lv_obj_set_pos(strip.root,120,PRODUCT_MODE_STRIP_Y);
    lv_obj_remove_flag(strip.root,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(strip.root,Draw,LV_EVENT_DRAW_MAIN,NULL);return 1;
}

/* Single nonblocking animation entry point. Finish the previous movement,
 * accept/coalesce the desired mode, then derive every icon's position, size
 * and neutral tint from one eased strip displacement. Normal next/previous
 * navigation travels exactly one48px pitch. Categories only advance; direct
 * jumps also move forward, never reverse the three-button category order.
 * Selection denotes location only: actual menu safety gates remain in App.
 * now is the same MCU monotonic clock used by the central page transition. */
void ProductModeStrip_Update(uint32_t mode,uint32_t now)
{
    if(!strip.root)return;
    PageTransitionFrame f;
    (void)PageTransition_Step(&strip.motion,now,&f);
    uint32_t ordinal=0U;while(ordinal<STRIP_COUNT&&visible_modes[ordinal]!=mode)++ordinal;
    if(ordinal<STRIP_COUNT)(void)PageTransition_Request(&strip.motion,ordinal,now);
    else if(strip.motion.initialized&&!strip.motion.active)(void)PageTransition_Request(&strip.motion,strip.motion.pending,now);
    if(!strip.motion.initialized||!strip.opacity.value)return;
    (void)PageTransition_Step(&strip.motion,now,&f);
    int travel=(int)strip.motion.target-(int)strip.motion.current;
    if(travel<0)travel+=(int)STRIP_COUNT;
    int shift=strip.motion.active?(travel*PITCH*(int)f.incoming_alpha+127)/255:0;
    int phase=(int)strip.motion.current*PITCH+shift,changed=0;
    for(uint32_t i=0;i<STRIP_COUNT;++i){
        int dx=(int)i*PITCH-phase;
        if(dx>=(int)PERIOD/2)dx-=(int)PERIOD;
        if(dx<-(int)PERIOD/2)dx+=(int)PERIOD;
        int distance=dx<0?-dx:dx;
        uint32_t weight=distance<PITCH?(uint32_t)((PITCH-distance)*255+PITCH/2)/PITCH:0;
        int center=STRIP_WIDTH/2+dx;
        if(strip.weights[i]!=weight||strip.centers[i]!=center)changed=1;
        strip.weights[i]=weight;strip.centers[i]=center;
    }
    if(changed)lv_obj_invalidate(strip.root);
    ++g_mode_strip.seq;g_mode_strip.active=strip.motion.active;
    g_mode_strip.current=visible_modes[strip.motion.current];g_mode_strip.target=visible_modes[strip.motion.target];
    g_mode_strip.elapsed_ms=strip.motion.active?now-strip.motion.start_ms:PAGE_TRANSITION_MS;
    /* Keep debugger indices at stable card IDs; retired id4 remains zero. */
    for(uint32_t i=0;i<9;++i){g_mode_strip.weights[i]=0;g_mode_strip.centers_x[i]=0;}
    for(uint32_t i=0;i<STRIP_COUNT;++i){uint32_t id=visible_modes[i];g_mode_strip.weights[id]=strip.weights[i];g_mode_strip.centers_x[id]=120+strip.centers[i];}
    ++g_mode_strip.seq;
}

void ProductModeStrip_SetVisible(uint32_t visible)
{if(strip.root)lv_obj_set_flag(strip.root,LV_OBJ_FLAG_HIDDEN,!visible);}

/* Page idle alpha composes with the shell hidden flag. One reusable eased
 * scalar handles reversal without recreating icons or resetting navigation. */
void ProductModeStrip_SetOpacity(uint32_t visible,uint32_t now)
{
    if(!strip.root)return;
    uint32_t before=strip.opacity.value;
    ScalarTransition_Request(&strip.opacity,visible?255U:0U,240U,now);
    (void)ScalarTransition_Value(&strip.opacity,now);
    if(before!=strip.opacity.value)lv_obj_invalidate(strip.root);
}
