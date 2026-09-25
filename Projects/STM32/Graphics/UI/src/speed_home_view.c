#include "SpeedHome_View.h"
#include "SpeedHome_Layout.h"
#include "Product_Fonts.h"
#include "Product_Icons.h"
#include "Product_MaintenanceBar.h"
#include "Graphics.h"
#include "Dashboard_PagesView.h"
#include "Graphics_SubpixelArc.h"
#include "Product_ModeStrip.h"
#include "Product_StatusIconsView.h"
#include "Toast_View.h"
#include "Background_View.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "Scene_Transition.h"
#include "Product_ModeStrip.h"

typedef struct {
    lv_obj_t *root,*content,*ring,*clock_group,*footer_group,*labels[SH_LABEL_COUNT],*icons[PRODUCT_ICON_COUNT];
    ProductMaintenanceBar maintenance;
    lv_font_t footer_fonts[3];
    SpeedHomeModel last;
    uint32_t peak_generation,drawn_peak,drawn_peak_color;
    uint32_t have_last,last_hud;ScenePose pose;
    char text[SH_LABEL_COUNT][40];
    lv_point_precise_t separator[3][SPEED_HOME_TRAPEZOID_POINTS];
    lv_obj_t *clock_segments[3];
} SpeedHomeView;
static SpeedHomeView *view_storage;
#define v (*view_storage)

/* Render the model's150ms interpolation on every display refresh. The normal
 * LVGL/EVE arc truncates values to integer degrees (~4px at this radius), so
 * this ring queues the same geometry with hundredth-degree angle precision.
 * Only visual angle/color are smoothed; trip/speed locks use raw UART speed. */
/* Both markers use the same radial bar. Average is twice the peak width;
 * shorten its centerline so round caps stay inside the22px ring. Interpolate
 * the same sine table as the arc to avoid whole-degree endpoint jumps. */
static int MarkerSine(uint32_t angle100)
{
    uint32_t degree=(angle100/100U)%360U,fraction=angle100%100U;
    int a=lv_trigo_sin(degree),b=lv_trigo_sin((degree+1U)%360U);
    return a+(b-a)*(int)fraction/100;
}
static void RingMarkers(lv_layer_t *layer)
{
    if(!v.last.session_valid||v.last.arc_override||!v.pose.ring_alpha)return;
    for(uint32_t i=0;i<2;i++){
        uint32_t ratio=i?v.last.peak_ratio:v.last.average_ratio;
        uint32_t angle=13500U+ratio*27000U/10000U;
        int x=MarkerSine(angle+9000U),y=MarkerSine(angle);
        int radius=v.pose.ring_radius-SPEED_HOME_RING_STROKE/2;
        lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=i?6:12;
        int half=(SPEED_HOME_RING_STROKE-d.width)/2;
        d.round_start=d.round_end=1;d.opa=v.pose.ring_alpha;
        d.color=lv_color_hex(i?v.last.peak_color:0xFFFFFF);
        d.p1=(lv_point_precise_t){240+x*(radius-half)/32768,240+y*(radius-half)/32768};
        d.p2=(lv_point_precise_t){240+x*(radius+half)/32768,240+y*(radius+half)/32768};
        lv_draw_line(layer,&d);
    }
}
static void RingDraw(lv_event_t *event)
{
    lv_layer_t *layer=lv_event_get_layer(event);
    /* Owner updates can outpace scanout. Remember only values submitted in
     * an actual ring draw, with marker and arc in the SAME display list. */
    if(v.peak_generation!=v.last.session_generation||!v.last.session_valid){
        v.drawn_peak=0;v.drawn_peak_color=v.last.peak_color;
        v.peak_generation=v.last.session_generation;
    }
    v.last.peak_ratio=v.drawn_peak;v.last.peak_color=v.drawn_peak_color;
    if(v.pose.ring_alpha)SpeedHome_RecordDisplayed(&v.last);
    v.drawn_peak=v.last.peak_ratio;v.drawn_peak_color=v.last.peak_color;
    Graphics_DrawSubpixelArcOpacity(layer,240,240,v.pose.ring_radius,SPEED_HOME_RING_STROKE,13500,40500,0x183440U,v.pose.ring_alpha);
    uint32_t value=v.last.arc_value>10000U?10000U:v.last.arc_value;
    if(value)Graphics_DrawSubpixelArcOpacity(layer,240,240,v.pose.ring_radius,SPEED_HOME_RING_STROKE,
                                    13500,13500+value*27000U/10000U,v.last.arc_color,v.pose.ring_alpha);
    RingMarkers(layer);
}

/* Parent deletion invalidates all view pointers. State/data live elsewhere. */
static void Deleted(lv_event_t *e)
{if(lv_event_get_target(e)==v.root)memset(&v,0,sizeof(v));}
void SpeedHome_Destroy(void){if(view_storage&&v.root)lv_obj_delete(v.root);}

/* Expose only the bounded content parent; shell/status objects stay private. */
lv_obj_t *SpeedHome_GetContentRoot(void){return view_storage?v.content:NULL;}
uint32_t SpeedHome_GetContentArea(lv_area_t *area)
{
    if(!view_storage||!v.content||!area)return 0U;
    *area=(lv_area_t){SPEED_HOME_CONTENT_X,SPEED_HOME_CONTENT_Y,
        SPEED_HOME_CONTENT_X+SPEED_HOME_CONTENT_WIDTH-1,
        SPEED_HOME_CONTENT_Y+SPEED_HOME_CONTENT_HEIGHT-1};
    return 1U;
}

/* Create a transparent, fixed-size LVGL parent. Normal LVGL child clipping
 * becomes EVE scissor commands, with no framebuffer/mask/layer allocation.
 * All riding card/menu/warning labels below use this same parent. */
static uint32_t Content(void)
{
    lv_area_t area={SPEED_HOME_CONTENT_X,SPEED_HOME_CONTENT_Y,
        SPEED_HOME_CONTENT_X+SPEED_HOME_CONTENT_WIDTH-1,
        SPEED_HOME_CONTENT_Y+SPEED_HOME_CONTENT_HEIGHT-1};
    if(!Graphics_IsAreaVisible(&area))return 0U;
    v.content=lv_obj_create(v.root);if(!v.content)return 0U;
    lv_obj_remove_style_all(v.content);
    lv_obj_set_pos(v.content,SPEED_HOME_CONTENT_X,SPEED_HOME_CONTENT_Y);
    lv_obj_set_size(v.content,SPEED_HOME_CONTENT_WIDTH,SPEED_HOME_CONTENT_HEIGHT);
    lv_obj_set_style_bg_opa(v.content,LV_OPA_TRANSP,0);
    lv_obj_remove_flag(v.content,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    return 1U;
}

/* Fixed baseline and width are resolved against actual font metrics. Reject
 * a box outside the circle or outside the lower round footer before creating
 * it. No auto-fit, ellipsis buffer, font scaling or mutable layout is used. */
static lv_obj_t *Label(uint32_t id)
{
    const SpeedHomeLabelLayout *r=&speed_home_labels[id];
    const lv_font_t *font=r->role==SH_NUMBER?Product_NumberFont(r->pixels):Product_TextFont(r->pixels);
    if(!font)return NULL;
    uint32_t compact=id>=SH_FOOTER_TITLE;
    /* Footer copies only line metrics, never bitmaps or glyph geometry. Trim
     * unused ascender/descender padding for this field's complete vocabulary,
     * so the validated object bounds match its actual visible text. */
    if(compact){
        const char *chars=r->role==SH_NUMBER?"0123456789.-#":
            id==SH_FOOTER_UNIT?"kmi":"ODOTRIP123FBELTSV ";
        int above=0,below=0;
        for(const char *c=chars;*c;++c){lv_font_glyph_dsc_t g;
            if(!lv_font_get_glyph_dsc(font,&g,(uint8_t)*c,0U))return NULL;
            if(g.box_h+g.ofs_y>above)above=g.box_h+g.ofs_y;
            if(-g.ofs_y>below)below=-g.ofs_y;
        }
        lv_font_t *trim=&v.footer_fonts[id-SH_FOOTER_TITLE];*trim=*font;
        trim->line_height=above+below;trim->base_line=below;font=trim;
    }
    int padding=compact?0:2;
    int y=r->baseline-(int)font->line_height+(int)font->base_line-padding;
    int height=font->line_height+2*padding;
    lv_area_t area={r->x,y,r->x+r->width-1,y+height-1};
    if(!Graphics_IsAreaVisible(&area))return NULL;
    if(id>=SH_FOOTER_TITLE&&(area.y1<SPEED_HOME_FOOTER_TOP||area.y2>SPEED_HOME_FOOTER_BOTTOM))return NULL;
    /* Catalog coordinates stay absolute so this reparenting cannot move text.
     * Reject malformed center layouts rather than silently cropping them. */
    uint32_t central=id>=SH_CARD_TITLE&&id<=SH_CARD_NUMBER;
    lv_area_t content;
    if(central&&(!SpeedHome_GetContentArea(&content)||area.x1<content.x1||
        area.y1<content.y1||area.x2>content.x2||area.y2>content.y2))return NULL;
    lv_obj_t *o=lv_label_create(central?v.content:id==SH_CLOCK?v.clock_group:v.footer_group);if(!o)return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,r->x-(central?SPEED_HOME_CONTENT_X:0),y-(central?SPEED_HOME_CONTENT_Y:0));
    lv_obj_set_size(o,r->width,height);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(0xF2F5F7U),0);
    lv_obj_set_style_text_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_text_align(o,r->align==SH_LEFT?LV_TEXT_ALIGN_LEFT:r->align==SH_RIGHT?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_letter_space(o,0,0);lv_obj_set_style_pad_top(o,padding,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);lv_label_set_text_static(o,v.text[id]);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);return o;
}

/* Update persistent text only on change. Empty fields keep their boxes but
 * draw no glyphs; shared hint/value box is controlled explicitly below. */
static void Text(uint32_t id,const char *value)
{
    if(strcmp(v.text[id],value)!=0){
        snprintf(v.text[id],sizeof(v.text[id]),"%s",value);
        lv_label_set_text_static(v.labels[id],v.text[id]);
    }
}

/* The clock and upper footer keep trapezoids; only the lower edge is straight.
 * Each object gets only its actual point count, with static lifetime storage.
 * All use simple EVE-supported lines, without a filled polygon or layer. */
static uint32_t Separators(void)
{
    for(uint32_t side=0;side<2U;++side){
        const int16_t (*points)[2]=side==0U?speed_home_separator:
            side==1U?speed_home_footer_separator:speed_home_footer_bottom_separator;
        uint32_t count=side==2U?2U:SPEED_HOME_TRAPEZOID_POINTS;
        for(uint32_t i=0;i<count;++i){
            v.separator[side][i].x=points[i][0];v.separator[side][i].y=points[i][1];
            if(!Graphics_IsPointVisible(points[i][0],points[i][1]))return 0U;
        }
        /* LVGL clears round_start after the first polyline segment. EVE's
         * unrounded horizontal/vertical RECTS path then omits LINE_WIDTH.
         * Give every segment its own two-point object and round both ends:
         * EVE uses LINE_STRIP with the requested width for all seven edges.
         * These objects are created only once, and their points stay static.
         * The1.5px caps also match the already-rounded inclined segments. */
        for(uint32_t segment=0;segment+1U<count;++segment){
            lv_obj_t *o=lv_line_create(side==0?v.clock_group:v.footer_group);if(!o)return 0U;
            if(side==0)v.clock_segments[segment]=o;
            lv_obj_remove_style_all(o);lv_obj_set_pos(o,0,0);
            lv_line_set_points(o,&v.separator[side][segment],2);
            lv_obj_set_style_line_width(o,SPEED_HOME_SEPARATOR_STROKE,0);
            lv_obj_set_style_line_rounded(o,true,0);
            lv_obj_set_style_line_color(o,lv_color_hex(0x71867CU),0);
            lv_obj_set_style_line_opa(o,LV_OPA_COVER,0);
            lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
        }
    }
    return 1U;
}

/* Material Round icon has a separate static font. Place its actual ink top
 * on the numeric/label ink top, independent of the font's padded line box. */
static uint32_t FooterIcons(void)
{
    const lv_font_t *font=Product_IconFont(24U);
    static const uint32_t codepoints[PRODUCT_ICON_COUNT]={0xE869U,0xE546U};
    if(!font)return 0U;
    for(uint32_t i=0;i<PRODUCT_ICON_COUNT;++i){
        lv_font_glyph_dsc_t glyph;
        if(!lv_font_get_glyph_dsc(font,&glyph,codepoints[i],0U))return 0U;
        int y=SPEED_HOME_FOOTER_INK_TOP-(font->line_height-font->base_line-glyph.box_h-glyph.ofs_y);
        lv_area_t box={SPEED_HOME_ICON_X,y,SPEED_HOME_ICON_X+23,y+font->line_height-1};
        if(!Graphics_IsAreaVisible(&box))return 0U;
        lv_obj_t *o=lv_label_create(v.footer_group);if(!o)return 0U;v.icons[i]=o;
        lv_obj_remove_style_all(o);lv_obj_set_pos(o,SPEED_HOME_ICON_X,y);
        lv_obj_set_size(o,24,font->line_height);lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(0xDCE5E9U),0);
        lv_obj_set_style_text_opa(o,LV_OPA_COVER,0);
        lv_label_set_text_static(o,Product_IconText((ProductIcon)i));
        lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
    }
    return 1U;
}

/* Create full-radius ring, fixed clock/footer and reusable center objects.
 * Switching mini modes changes text/visibility only, never rebuilds the tree. */
uint32_t SpeedHome_Create(lv_obj_t *parent)
{
    if(!view_storage){view_storage=pvPortMalloc(sizeof(*view_storage));if(!view_storage)return 0;memset(view_storage,0,sizeof(*view_storage));}
    SpeedHome_Destroy();if(!parent)return 0U;
    v.pose=(ScenePose){.ring_radius=240,.ring_alpha=255};
    lv_obj_update_layout(parent);lv_area_t a;lv_obj_get_content_coords(parent,&a);
    if(a.x1||a.y1||a.x2!=479||a.y2!=479)return 0U;
    v.root=lv_obj_create(parent);if(!v.root)return 0U;
    lv_obj_remove_style_all(v.root);lv_obj_set_pos(v.root,0,0);lv_obj_set_size(v.root,480,480);
    lv_obj_remove_flag(v.root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v.root,Deleted,LV_EVENT_DELETE,NULL);
    if(!BackgroundView_Create(v.root))goto fail;
    v.ring=lv_obj_create(v.root);if(!v.ring)goto fail;
    lv_obj_remove_style_all(v.ring);lv_obj_set_pos(v.ring,0,0);lv_obj_set_size(v.ring,480,480);
    lv_obj_add_event_cb(v.ring,RingDraw,LV_EVENT_DRAW_MAIN,NULL);
    lv_obj_remove_flag(v.ring,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_SCROLLABLE);
    v.clock_group=lv_obj_create(v.root);v.footer_group=lv_obj_create(v.root);
    if(!v.clock_group||!v.footer_group)goto fail;
    lv_obj_t *groups[]={v.clock_group,v.footer_group};
    for(uint32_t i=0;i<2;++i){lv_obj_remove_style_all(groups[i]);lv_obj_set_pos(groups[i],0,0);lv_obj_set_size(groups[i],480,480);lv_obj_remove_flag(groups[i],LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);}
    if(!Separators()||!ProductMaintenanceBar_Create(&v.maintenance,v.footer_group)||!Content())goto fail;
    for(uint32_t i=0;i<SH_LABEL_COUNT;++i){
        /* Legacy center/HUD IDs remain in the geometry catalog for evidence,
         * but no visible object or text is allocated for UART/FPS/CPU. */
        if((i>=SH_CARD_TITLE&&i<=SH_PERCENT)||i>=SH_FOOTER_AUX_TITLE)continue;
        v.labels[i]=Label(i);if(!v.labels[i])goto fail;
    }
    if(!ProductModeStrip_Create(v.root)||!DashboardPagesView_Create(v.content)||!ToastView_Create(v.content))goto fail;
    if(!FooterIcons()||!ProductStatusIcons_Create(v.root))goto fail;
    Text(SH_CLOCK,"--:--");Text(SH_FOOTER_TITLE,"ODO");Text(SH_FOOTER_VALUE,"------");Text(SH_FOOTER_UNIT,"km");
    return 1U;
fail:SpeedHome_Destroy();return 0U;
}

/* Independent model values and child presentation are consumed by the owner.
 * FPS/CPU remain a small development row; no numeric current-speed field is
 * introduced. Parent overlay and child card share fixed center rectangles. */
void SpeedHome_Render(const SpeedHomeModel *m,const UiDashboardPresentation *p,uint32_t now)
{
    if(!view_storage||!m||!p||!v.root)return;
    if(!v.have_last||v.last.arc_value!=m->arc_value||v.last.arc_color!=m->arc_color||v.last.session_valid!=m->session_valid||v.last.average_ratio!=m->average_ratio||v.last.arc_override!=m->arc_override||v.last.session_generation!=m->session_generation)lv_obj_invalidate(v.ring);
    Text(SH_CLOCK,m->clock);Text(SH_FOOTER_TITLE,p->footer_title);
    Text(SH_FOOTER_VALUE,p->footer_value);Text(SH_FOOTER_UNIT,p->unit);
    /* Only SERV and RESERVE replace their text with one Material Round icon.
     * OIL/BELT retain text. Only BELT/SERV show calendar DAYS below the row. */
    uint32_t service=p->footer==UI_SERV,reserve=p->footer==UI_RESV;
    lv_obj_set_flag(v.labels[SH_FOOTER_TITLE],LV_OBJ_FLAG_HIDDEN,service||reserve);
    lv_obj_set_flag(v.icons[PRODUCT_ICON_BUILD],LV_OBJ_FLAG_HIDDEN,!service);
    lv_obj_set_flag(v.icons[PRODUCT_ICON_LOCAL_GAS_STATION],LV_OBJ_FLAG_HIDDEN,!reserve);
    lv_obj_set_style_text_color(v.labels[SH_FOOTER_VALUE],
        lv_color_hex(p->reserve_distance_warning?0xFFD600U:0xF2F5F7U),0);
    /* Remove only the visible OIL hours row: its internal measurement and
     * service-life calculations continue. OIL uses the same remaining line
     * as distance modes; calendar modes keep their existing DAYS row. */
    ProductMaintenanceBar_Set(&v.maintenance,p->footer!=UI_BELT&&p->footer!=UI_SERV,p->remaining_valid,p->remaining_permille);
    /* UART/performance snapshots remain in RAM; product pages contain no HUD. */
    (void)now;
    v.last=*m;v.have_last=1U;
}

/* Groups keep their absolute catalog coordinates. Moving only their parent
 * carries separators, labels and service bar together through the same pose.
 * No transformed bitmap/layer is allocated; returning uses current telemetry. */
void SpeedHome_ApplyScene(const ScenePose *pose,uint32_t quick)
{
    (void)quick; /* Quick adjustment now shares the normal animated page bank. */
    if(!view_storage||!v.root||!pose)return;
    if(v.pose.ring_radius!=pose->ring_radius||v.pose.ring_alpha!=pose->ring_alpha)lv_obj_invalidate(v.ring);
    /* Start at the shared layout, not a second stale set of coordinates.
     * Once the ring has faded, extend the arms to the physical circle. */
    uint32_t q=pose->ring_alpha>=255U?0U:(255U-pose->ring_alpha)*1024U/255U;
    if(q>1024U)q=1024U;
    int x=speed_home_separator[0][0],y=speed_home_separator[0][1];
    x-=((x-111)*q+512U)/1024U;y-=((y-38)*q+512U)/1024U;
    if(v.separator[0][0].x!=x||v.separator[0][0].y!=y){
        v.separator[0][0]=(lv_point_precise_t){x,y};
        v.separator[0][3]=(lv_point_precise_t){480-x,y};
        for(uint32_t i=0;i<3;++i)lv_line_set_points(v.clock_segments[i],&v.separator[0][i],2);
    }
    v.pose=*pose;lv_obj_set_y(v.clock_group,pose->clock_y);lv_obj_set_y(v.footer_group,pose->footer_y);
    lv_obj_set_flag(v.content,LV_OBJ_FLAG_HIDDEN,pose->progress!=0);
    ProductModeStrip_SetVisible(pose->progress==0);
    /* Power/settings overlays carry only the shell. Normal Render restores
     * the live maintenance indication when the driving scene returns. */
    if(pose->progress)ProductMaintenanceBar_Set(&v.maintenance,0,v.maintenance.valid,v.maintenance.ratio);
}
/* Last applied pose is owned by the renderer, including an interrupted
 * Settings animation. Call before another scene overwrites this frame. */
void SpeedHome_GetScene(ScenePose *pose){if(pose)*pose=v.pose;}
