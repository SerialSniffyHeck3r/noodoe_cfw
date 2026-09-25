#include "Dashboard_PagesView.h"
#include "Graphics.h"
#include "Page_Transition.h"
#include "Product_Fonts.h"
#include "Graphics_Viewport.h"
#include "Product_ModeStrip.h"
#include "Trip_Graphic.h"
#include "Music_Graphic.h"
#include "Button_Hints.h"
#include "Product_CallIcon.h"
#include "PhoneVisual.h"
#include "Trip_Layout.h"
#include "SpeedHome_Layout.h"
#include "Gps_GridFade.h"
#include "Gps_LineBatch.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* Current pages use IDs0..10. The old unused ID11 allocated two labels and
 * 136bytes of pointers/text without ever drawing content. */
#define LABELS 11U
#define WHITE 0xF2F5F7U
#define MUTED 0x93A6ADU
#define ACCENT 0x48B6D0U
typedef struct {
    lv_obj_t *root,*labels[LABELS],*plot_obj;
    char text[LABELS][64];uint32_t used,alpha,grey,kind,selection;char speed[4];TripGraphic trip;MusicGraphic music;
    int16_t plot[PHONE_TRAIL_POINTS][2];uint32_t plot_count,quick_ratio;int32_t heading_east,heading_north;uint64_t plot_gaps;
    int16_t grid[PHONE_TRAIL_GRID_LINES][4];uint32_t grid_count;
    uint32_t retired,retired_frame;
} PageSlot;
static struct {PageSlot slots[2];PageTransition transition;DashboardPage last_bound;
    PageSlot *bound_slot;lv_obj_t *body;uint32_t front,ready;PageTransitionAxis axis;int32_t direction;} view;
volatile DashboardViewDiagnostics g_dashboard_view;

/* Fixed boxes use natural text advances and separate tabular digits. No
 * automatic content dimensions or object allocation occurs during a frame. */
static void Label(PageSlot *s,uint32_t id,const char *text,int x,int y,int w,int h,uint32_t px,uint32_t numeric,uint32_t color,lv_text_align_t align)
{
    if(id>=LABELS)return;
    lv_obj_t *o=s->labels[id];lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(o,x,y-40);lv_obj_set_size(o,w,h);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o,numeric?Product_NumberFont(px):Product_TextFont(px),0);
    lv_obj_set_style_text_color(o,lv_color_hex(s->grey?0x647077U:color),0);
    lv_obj_set_style_text_align(o,align,0);
    if(strcmp(s->text[id],text)){snprintf(s->text[id],sizeof(s->text[id]),"%s",text);lv_label_set_text_static(o,s->text[id]);}
    s->used|=1U<<id;
}
static void Center(PageSlot *s,uint32_t id,const char *text,int y,uint32_t px)
{Label(s,id,text,12,y,312,px+8,px,0,WHITE,LV_TEXT_ALIGN_CENTER);}
/* Align font families at a fixed numeric baseline, never the ink bounds of
 * the current value. Rounded6/8 extend1px below7: adjusting for each string
 * made97.6 ->97.7 ->97.8 move the whole row348 ->349 ->348. The full numeric
 * vocabulary gives every value the same Y while retaining natural overshoot.
 * Static text/unit labels still account for Moving/Stopped/mph descenders.
 * bottom is absolute screen Y; Label takes coordinates relative to the shell. */
static void TripLabel(PageSlot *s,uint32_t id,const char *text,int x,int bottom,int w,uint32_t px,uint32_t numeric,uint32_t color,lv_text_align_t align)
{
    const lv_font_t *font=numeric?Product_NumberFont(px):Product_TextFont(px);
    const char *vocabulary=numeric?"0123456789.:-#":text;
    int ink_bottom=-100;
    for(const char *c=vocabulary;*c;++c){lv_font_glyph_dsc_t g;
        if(lv_font_get_glyph_dsc(font,&g,(uint8_t)*c,0U)&&g.box_h){
            int b=-g.ofs_y-1;if(b>ink_bottom)ink_bottom=b;
        }
    }
    if(ink_bottom==-100)ink_bottom=0;
    int y=bottom-ink_bottom-font->line_height+font->base_line-SPEED_HOME_CONTENT_Y;
    Label(s,id,text,x,y,w,font->line_height,px,numeric,color,align);
}
/* Format only the presentation: real trip time remains H:MM in the model.
 * A one-digit hour gets one gray padding0 in a separate, non-overlapping
 * fixed cell. Minutes retain their normal two-digit clock notation. Both
 * pieces use the same fixed baseline and fade with the whole page section.
 * Larger hours and unknown values retain the existing132px centered slot. */
static void TripTime(PageSlot *s,const char *text)
{
    if(text[0]>='0'&&text[0]<='9'&&text[1]==':'){
        TripLabel(s,4,"0",TRIP_TIME_PAD_X,215,TRIP_TIME_DIGIT_WIDTH,40,1,0x647077U,LV_TEXT_ALIGN_LEFT);
        TripLabel(s,2,text,TRIP_TIME_SUFFIX_X,215,TRIP_TIME_SUFFIX_WIDTH,40,1,WHITE,LV_TEXT_ALIGN_LEFT);
    }else TripLabel(s,2,text,TRIP_TIME_X,215,TRIP_TIME_WIDTH,40,1,WHITE,LV_TEXT_ALIGN_CENTER);
}
/* Geometry is bounded before LVGL creates a task or EVE packs a vertex.
 * Both the grid and long routes share this window; page translation is added
 * only after clipping. No framebuffer or GPU cache allocation. */
/* Clip before batching. The separator makes gaps explicit even if successive
 * clipped segments happen to touch. Coordinates are copied by LVGL. */
static void PlotLine(lv_point_precise_t *points,uint32_t *count,const lv_area_t *a,const int16_t *line,int inset)
{
 int16_t clipped[4];if(!GpsLine_Clip(line,inset,clipped))return;
 points[(*count)++]=(lv_point_precise_t){a->x1+clipped[0],a->y1+clipped[1]};
 points[(*count)++]=(lv_point_precise_t){a->x1+clipped[2],a->y1+clipped[3]};
 points[(*count)++]=(lv_point_precise_t){LV_DRAW_LINE_POINT_NONE,LV_DRAW_LINE_POINT_NONE};
}
static void Plot(lv_event_t *e)
{
    PageSlot *s=lv_event_get_user_data(e);
    if(!s)return;
    lv_area_t a;lv_obj_get_coords(s->plot_obj,&a);lv_layer_t *layer=lv_event_get_layer(e);
    if(s->kind==UI_BLANK){
        if(s->selection==1){
            lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=14;
            d.bg_color=lv_color_black();d.bg_opa=s->alpha*180U/255U;
            lv_area_t plate={a.x1+28,a.y1+191,a.x1+307,a.y1+233};lv_draw_rect(layer,&d,&plate);
        }
        return;
    }
    /* The quick bar belongs to its moving page bank, including opacity.
     * Reuse the existing custom draw surface; no extra per-frame allocation. */
    if(s->kind==UI_SYSTEM){
        lv_draw_rect_dsc_t r;lv_draw_rect_dsc_init(&r);r.radius=4;r.bg_opa=s->alpha;
        r.bg_color=lv_color_hex(0x344049U);lv_draw_rect(layer,&r,&a);
        uint32_t width=224U*s->quick_ratio/1000U;
        if(width){a.x2=a.x1+(int)width-1;r.bg_color=lv_color_hex(0x42A5F5U);lv_draw_rect(layer,&r,&a);}
        return;
    }
    if(!s->plot_count)return;
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.width=3;d.opa=s->alpha;
    /* Translucent solid lines share the route's world-space transform. The
     * project-owned batch reuses EVE LINE_STRIP with one context per style. */
    /* Rounded ends select the existing LINE_STRIP path even at north-up,
     * where grid segments become exactly horizontal/vertical. Its explicit
     * line width avoids inheriting the previous primitive's width. */
    d.width=1;d.opa=s->alpha*120/255;d.color=lv_color_hex(WHITE);
    d.round_start=d.round_end=1;
    lv_point_precise_t points[PHONE_TRAIL_POINTS*3];uint32_t count,previous=0;
    for(uint32_t band=0;band<3;band++){
        uint32_t target=(band+1U)*40U*s->alpha/255U;
        d.opa=(target-previous)*255U/(255U-previous);previous=target;count=0;
        for(uint32_t i=0;i<s->grid_count&&i<PHONE_TRAIL_GRID_LINES;i++)PlotLine(points,&count,&a,s->grid[i],4+(int)band*14);
        GpsLineBatch_Draw(layer,&d,points,count);
    }
    d.width=3;d.opa=s->alpha;d.color=lv_color_hex(ACCENT);d.round_start=d.round_end=1;count=0;
    for(uint32_t i=1;i<s->plot_count&&i<PHONE_TRAIL_POINTS;++i){
        if(s->plot_gaps&(1ULL<<i))continue;
        int16_t line[4]={s->plot[i-1U][0],s->plot[i-1U][1],s->plot[i][0],s->plot[i][1]};
        PlotLine(points,&count,&a,line,2);
    }
    GpsLineBatch_Draw(layer,&d,points,count);
    int cx=a.x1+GPS_PLOT_WIDTH/2,cy=a.y1+GPS_PLOT_HEIGHT/2,dx=s->heading_east*12/1024,dy=-s->heading_north*12/1024;
    /* Arrow uses accepted course, never two decimated/projected points. */
    d.color=lv_color_white();d.width=4;d.p1=(lv_point_precise_t){cx+dx,cy+dy};
    d.p2=(lv_point_precise_t){cx-dx/2+dy/2,cy-dy/2-dx/2};lv_draw_line(layer,&d);
    d.p2=(lv_point_precise_t){cx-dx/2-dy/2,cy-dy/2+dx/2};lv_draw_line(layer,&d);
    d.width=2;d.p1=(lv_point_precise_t){a.x1+24,a.y2-9};d.p2=(lv_point_precise_t){a.x1+74,a.y2-9};lv_draw_line(layer,&d);
    for(int x=24;x<=74;x+=50){d.p1=(lv_point_precise_t){a.x1+x,a.y2-13};d.p2=(lv_point_precise_t){a.x1+x,a.y2-5};lv_draw_line(layer,&d);}

}
static void Alpha(PageSlot *s,uint32_t alpha,int32_t x,int32_t y)
{
    /* Calls keep their category and action/status labels anchored. Only the
     * clipped contact bitmap follows the shared vertical transition curve. */
    int32_t rows=s->kind==UI_CALLS?y:0;
    lv_obj_set_pos(s->root,x,y-rows);
    if(s->music.slide_y!=rows){s->music.slide_y=rows;lv_obj_invalidate(s->music.root);}
    if(s->alpha==alpha)return;
    s->alpha=alpha;TripGraphic_Alpha(&s->trip,alpha);
    MusicGraphic_Alpha(&s->music,alpha);
    for(uint32_t i=0;i<LABELS;++i)lv_obj_set_style_text_opa(s->labels[i],alpha,0);
    lv_obj_invalidate(s->plot_obj);
}

/* Bind/update a complete section, including every title, value, graph and
 * image. Outgoing bank is left untouched for the duration of its transition. */
static void Bind(PageSlot *s,const DashboardPage *p)
{
    /* Only one bank receives live updates. Byte equality (not a hash) skips
     * unchanged layout/text at the fast owner-task rate. The outgoing section
     * stays frozen; animation alpha is still advanced independently. */
    /* Phone pixels complete asynchronously, including while paused. The
     * metadata can remain byte-identical, so poll readiness before skipping. */
    if(p->kind==UI_MUSIC)MusicGraphic_Text(&s->music,p->known?p->visual_key:0);
    if(p->kind==UI_NOTIFICATIONS||p->kind==UI_CALLS)MusicGraphic_Panel(&s->music,p->visual_key,p->reply_selection,p->reply_count);
    if(view.bound_slot==s&&!memcmp(&view.last_bound,p,sizeof(*p)))return;
    view.bound_slot=s;view.last_bound=*p;
    s->music.call_color=0;s->used=0;s->grey=p->grey;s->kind=p->kind;s->selection=p->selection;
    MusicGraphic_Update(&s->music,p->kind==UI_MUSIC||p->kind==UI_NOTIFICATIONS||p->kind==UI_CALLS||(p->kind==UI_BLANK&&p->selection==2),p->known,!strcmp(p->lines[2],"Playing"),p->ratio_permille);
    TripGraphic_Update(&s->trip,p->kind==UI_TRIP,p->known&&p->ratio_permille<=1000U,p->ratio_permille);
    lv_obj_add_flag(s->plot_obj,LV_OBJ_FLAG_HIDDEN);
    /* The fixed mode strip owns localY0..35. Page bodies start below it;
     * only inner row spacing changes, never the clock/ring/footer shell. */
    if(p->kind!=UI_BLANK&&p->kind!=UI_MUSIC&&p->kind!=UI_NOTIFICATIONS&&p->kind!=UI_PHONE_GPS)Center(s,0,p->title,40,24U);
    if(p->kind==UI_BLANK){
        if(p->selection){
            TripLabel(s,3,p->note,8,369,320,24,0,WHITE,LV_TEXT_ALIGN_CENTER);
            lv_obj_set_pos(s->plot_obj,0,0);lv_obj_set_size(s->plot_obj,336,240);
            lv_obj_remove_flag(s->plot_obj,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(s->plot_obj);
            if(p->selection==2U){
                /* Stable three-digit field + unit; no content-size centering. */
                if(s->music.panel!=2||strcmp(s->speed,p->numbers[0]))MusicGraphic_Number(&s->music,p->numbers[0]);
                snprintf(s->speed,sizeof(s->speed),"%.3s",p->numbers[0]);
                TripLabel(s,2,p->lines[0],267,302,62,24,0,WHITE,LV_TEXT_ALIGN_LEFT);
            }
        }
    }else if(p->kind==UI_TRIP){
        /* Reclaim the old diagnostic row for the trip itself. Quality/gap
         * flags remain in app data/RAM but are never a footer caption. Each
         * row has one visible bottom; fixed widths cover99999.9 and999.9. */
        TripLabel(s,1,"Moving",8,215,80,20,0,ACCENT,LV_TEXT_ALIGN_LEFT);
        TripTime(s,p->numbers[0]);
        TripLabel(s,3,"Stopped",243,215,85,20,0,0xEEE5BCU,LV_TEXT_ALIGN_RIGHT);
        /* Fixed slots: signpost . . . motorcycle,16px padding, XXXXX.X km.
         * Label4 is now reserved for the gray hour padding, not distance. */
        TripLabel(s,5,p->numbers[4],TRIP_DISTANCE_X,313,TRIP_DISTANCE_WIDTH,40,1,WHITE,LV_TEXT_ALIGN_RIGHT);
        TripLabel(s,6,p->lines[4],TRIP_DISTANCE_UNIT_X,313,TRIP_DISTANCE_UNIT_WIDTH,24,0,WHITE,LV_TEXT_ALIGN_LEFT);
        /* Compact units preserve the common ink bottom without squeezing
         * numeric glyphs. Both groups reserve their complete km/h width. */
        TripLabel(s,7,p->numbers[2],40,376,64,36,1,WHITE,LV_TEXT_ALIGN_RIGHT);
        TripLabel(s,8,p->lines[2],110,376,37,16,0,WHITE,LV_TEXT_ALIGN_LEFT);
        TripLabel(s,9,p->numbers[3],196,376,83,36,1,WHITE,LV_TEXT_ALIGN_RIGHT);
        TripLabel(s,10,p->lines[3],285,376,37,16,0,WHITE,LV_TEXT_ALIGN_LEFT);
    }else if(p->kind==UI_NOTIFICATIONS){
        Center(s,1,p->lines[0],130,32);
        Center(s,6,p->note,242,24);
    }else if(p->kind==UI_CALLS){
        if(!p->visual_key)Center(s,1,p->lines[0],110,24);
        /* Direction is metadata for this exact selected bitmap, never inferred
         * from its place in the list. The phone keeps names/numbers private. */
        if(!p->ratio_permille&&p->art_revision){
            uint32_t type=p->art_revision;
            const char *name=type==1?"Incoming":type==2?"Outgoing":type==3?"Missed":type==4?"Voicemail":type==5?"Rejected":type==6?"Blocked":"Answered";
            uint32_t color=type==1?0x5BCB9AU:type==2?0x5BAEF5U:0xFF796EU;
            Label(s,2,name,114,198,170,30,24,0,color,LV_TEXT_ALIGN_LEFT);
            /* Existing Rounded phone glyph. Tint matches the call category. */
            s->music.call_color=color;
        }else s->music.call_color=0;
        Center(s,6,p->note,238,20);
        if(p->ratio_permille==CALL_ACTIVE||p->ratio_permille==CALL_HELD)Center(s,5,p->numbers[0],214,24);
    }else if(p->kind==UI_MUSIC){
        /* Art belongs to the shell background. Bold title, regular artist
         * and playback times use fixed lower-body boxes; the progress object
         * shares this page's translation/fade. Debug notes remain in RAM. */
        if(!MusicGraphic_Text(&s->music,p->known?p->visual_key:0)){
            Label(s,2,p->visual_key?"":p->lines[0],16,160,304,34,24,0,WHITE,LV_TEXT_ALIGN_CENTER);
            Label(s,3,p->known&&!p->visual_key?p->lines[1]:"",16,194,304,32,24,0,0xDCE5E9U,LV_TEXT_ALIGN_CENTER);
        }
        if(p->known){
            Label(s,5,p->numbers[0],28,261,125,23,20,1,WHITE,LV_TEXT_ALIGN_LEFT);
            Label(s,6,p->numbers[1],184,261,125,23,20,1,WHITE,LV_TEXT_ALIGN_RIGHT);
        }
    }else if(p->kind==UI_SYSTEM){
        TripLabel(s,1,p->lines[0],28,214,280,20,0,MUTED,LV_TEXT_ALIGN_CENTER);
        TripLabel(s,2,p->numbers[0],68,265,200,40,1,WHITE,LV_TEXT_ALIGN_CENTER);
        TripLabel(s,3,p->lines[1],224,265,30,24,0,WHITE,LV_TEXT_ALIGN_CENTER);
        TripLabel(s,4,p->lines[2],2,319,332,20,0,MUTED,LV_TEXT_ALIGN_CENTER);
        TripLabel(s,5,p->note,0,363,336,20,0,WHITE,LV_TEXT_ALIGN_CENTER);
        s->quick_ratio=p->ratio_permille>1000U?1000U:p->ratio_permille;
        lv_obj_set_pos(s->plot_obj,56,138);lv_obj_set_size(s->plot_obj,224,8);
        lv_obj_remove_flag(s->plot_obj,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(s->plot_obj);
    }else if(p->kind==UI_PHONE_GPS){
        lv_obj_set_pos(s->plot_obj,0,4);lv_obj_set_size(s->plot_obj,GPS_PLOT_WIDTH,GPS_PLOT_HEIGHT);
        s->heading_east=p->heading_east;s->heading_north=p->heading_north;s->plot_gaps=p->plot_gaps;
        s->plot_count=p->plot_count;memcpy(s->plot,p->plot,sizeof(s->plot));
        s->grid_count=p->grid_count;memcpy(s->grid,p->grid,sizeof(s->grid));
        lv_obj_set_flag(s->plot_obj,LV_OBJ_FLAG_HIDDEN,!p->plot_count);lv_obj_invalidate(s->plot_obj);
        if(p->plot_count)Label(s,2,p->numbers[0],20,252,88,24,16,0,WHITE,LV_TEXT_ALIGN_LEFT);
        Center(s,1,p->note,244,20);
    }else{
        if(p->kind!=UI_BLANK){Center(s,1,p->lines[0],104,24);
            Label(s,2,p->numbers[0],18,151,300,42,36,1,WHITE,LV_TEXT_ALIGN_CENTER);}
        Center(s,3,p->note,p->kind==UI_BLANK?244:226,20);
    }
    for(uint32_t i=0;i<LABELS;++i)if(!(s->used&(1U<<i)))lv_obj_add_flag(s->labels[i],LV_OBJ_FLAG_HIDDEN);
}

uint32_t DashboardPagesView_Create(lv_obj_t *parent)
{
    if(!parent)return 0;
    memset(&view,0,sizeof(view));memset((void*)&g_dashboard_view,0,sizeof(g_dashboard_view));g_dashboard_view.magic=0x50414731U;
    /* Independent clipped body protects the fixed mode bar during vertical
     * motion. Moving a page upward cannot draw its title over the icons. */
    view.body=lv_obj_create(parent);if(!view.body)return 0;
    lv_obj_remove_style_all(view.body);lv_obj_set_pos(view.body,0,40);lv_obj_set_size(view.body,336,SPEED_HOME_CONTENT_HEIGHT-40);
    lv_obj_remove_flag(view.body,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    for(uint32_t n=0;n<2U;++n){PageSlot *s=&view.slots[n];
        s->root=lv_obj_create(view.body);
    if(!s->root)return 0;
        lv_obj_remove_style_all(s->root);lv_obj_set_size(s->root,336,SPEED_HOME_CONTENT_HEIGHT-40);
        lv_obj_remove_flag(s->root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        for(uint32_t i=0;i<LABELS;++i){s->labels[i]=lv_label_create(s->root);
    if(!s->labels[i])return 0;
            lv_obj_remove_style_all(s->labels[i]);lv_label_set_long_mode(s->labels[i],LV_LABEL_LONG_CLIP);
            lv_label_set_text_static(s->labels[i],s->text[i]);lv_obj_set_style_text_opa(s->labels[i],255,0);
        }
        s->plot_obj=lv_obj_create(s->root);
    if(!s->plot_obj)return 0;
        lv_obj_remove_style_all(s->plot_obj);lv_obj_set_pos(s->plot_obj,24,34);lv_obj_set_size(s->plot_obj,288,168);
        lv_obj_remove_flag(s->plot_obj,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s->plot_obj,Plot,LV_EVENT_DRAW_MAIN,s);lv_obj_move_to_index(s->plot_obj,0);
        if(!TripGraphic_Create(&s->trip,s->root))return 0;
        if(!MusicGraphic_Create(&s->music,s->root))return 0;
        MusicGraphic_Alpha(&s->music,255);
        s->alpha=255;lv_obj_add_flag(s->root,LV_OBJ_FLAG_HIDDEN);
    }
    if(!ButtonHints_Create(lv_obj_get_parent(parent)))return 0;
    view.ready=1;return 1;
}
static void Retire(PageSlot *s)
{
    lv_obj_add_flag(s->root,LV_OBJ_FLAG_HIDDEN);
    s->retired=1;s->retired_frame=g_graphics.render_count;
}
static uint32_t Available(PageSlot *s)
{
    if(s->retired&&!Graphics_FramePresentedAfter(s->retired_frame))return 0;
    s->retired=0;return 1;
}
void DashboardPagesView_Render(const DashboardPage *p,uint32_t now)
{
    if(!p||!view.ready)return;
    ++g_dashboard_view.seq;PageTransitionFrame frame;
    if(PageTransition_Step(&view.transition,now,&frame)){
        MusicGraphic_Alpha(&view.slots[view.front].music,0);
        Retire(&view.slots[view.front]);view.front^=1U;
        Alpha(&view.slots[view.front],255,0,0);
    }
    /* A removed LVGL object can still belong to the currently scanned EVE
     * list. Only a completed physical swap makes that page's texture reusable. */
    uint32_t request=0;
    if(view.transition.active||!view.transition.initialized||p->key==view.transition.current||Available(&view.slots[view.front^1U]))
        request=PageTransition_Request(&view.transition,p->key,now);
    ProductModeStrip_Update(request||!view.transition.active?p->kind:UINT32_MAX,now);
    if(request==2U){
        if(p->kind==UI_MUSIC&&PhoneVisual_MusicRevision(p->visual_key))MusicGraphic_Marquee(&view.slots[view.front].music,p->visual_key);
        Bind(&view.slots[view.front],p);lv_obj_remove_flag(view.slots[view.front].root,LV_OBJ_FLAG_HIDDEN);}
    if(request==1U){
        view.axis=view.slots[view.front].kind==p->kind?PAGE_AXIS_VERTICAL:PAGE_AXIS_HORIZONTAL;
        view.direction=view.axis==PAGE_AXIS_VERTICAL&&p->direction==2U?-1:1;
        ++g_dashboard_view.transitions;
        if(p->kind==UI_MUSIC&&PhoneVisual_MusicRevision(p->visual_key))MusicGraphic_Marquee(&view.slots[view.front^1U].music,p->visual_key);
        Bind(&view.slots[view.front^1U],p);lv_obj_remove_flag(view.slots[view.front^1U].root,LV_OBJ_FLAG_HIDDEN);}
    if(view.transition.active){
        if(p->key==view.transition.target&&!(p->kind==UI_BLANK&&p->selection==2))Bind(&view.slots[view.front^1U],p);
        (void)PageTransition_Step(&view.transition,now,&frame);
        PageTransitionPose pose;PageTransition_Project(&frame,view.axis,view.direction,&pose);
        Alpha(&view.slots[view.front],frame.outgoing_alpha,pose.outgoing_x,pose.outgoing_y);
        Alpha(&view.slots[view.front^1U],frame.incoming_alpha,pose.incoming_x,pose.incoming_y);
        g_dashboard_view.outgoing_x=pose.outgoing_x;g_dashboard_view.outgoing_y=pose.outgoing_y;
        g_dashboard_view.incoming_x=pose.incoming_x;g_dashboard_view.incoming_y=pose.incoming_y;
    }else{
        PageSlot *front=&view.slots[view.front],*back=&view.slots[view.front^1U];
        if(p->key==view.transition.current){
            uint32_t panel_revision=0;
            if(p->kind==UI_NOTIFICATIONS||p->kind==UI_CALLS)PhoneVisual_CopyPanel(p->visual_key,NULL,&panel_revision);
            if(panel_revision&&front->music.text_ready&&front->music.text_revision!=panel_revision){
                if(Available(back)){
                    back->music.text_key=back->music.text_revision=back->music.text_ready=back->music.text_wait=0;
                    Bind(back,p);Retire(front);view.front^=1U;lv_obj_remove_flag(back->root,LV_OBJ_FLAG_HIDDEN);
                }
            }else if(p->kind==UI_BLANK&&p->selection==2&&strcmp(front->speed,p->numbers[0])){
                if(Available(back)){Bind(back,p);Retire(front);view.front^=1U;lv_obj_remove_flag(back->root,LV_OBJ_FLAG_HIDDEN);}
            }else if(p->kind==UI_MUSIC&&PhoneVisual_MusicRevision(p->visual_key)&&
                    (front->music.text_key!=p->visual_key||front->music.text_revision!=PhoneVisual_MusicRevision(p->visual_key)||!front->music.text_ready)){
                if(Available(back)&&MusicGraphic_Marquee(&back->music,p->visual_key)){
                    Bind(back,p);Retire(front);view.front^=1U;lv_obj_remove_flag(back->root,LV_OBJ_FLAG_HIDDEN);
                }else Bind(front,p);
            }else Bind(front,p);
        }
        Alpha(&view.slots[view.front],255,0,0);
    }
    /* Physical-edge hints have one shared renderer, independent of the two
     * sliding page banks. Keep the Music bank's fade while entering/leaving;
     * a phone1/2 transition keeps one feedback pulse, not overlapping copies. */
    PageSlot *hint=&view.slots[view.front];
    if(view.transition.active&&view.slots[view.front^1U].kind==UI_MUSIC)hint=&view.slots[view.front^1U];
    if(hint->kind==UI_MUSIC)MusicGraphic_Hints(hint->music.playing,UI_MUSIC+1U,hint->alpha,now);
    else if(p->kind==UI_CALLS)ProductCall_Hints(p->ratio_permille,UI_CALLS+1U,255,now);
    else ButtonHints_ShowKeys(p->kind+1U,255,now);
    g_dashboard_view.axis=view.axis;g_dashboard_view.direction=view.direction;
    g_dashboard_view.frames++;g_dashboard_view.active=view.transition.active;g_dashboard_view.key=view.transition.current;
    g_dashboard_view.target=view.transition.target;g_dashboard_view.alpha=view.slots[view.front^1U].alpha;
    g_dashboard_view.elapsed_ms=now-view.transition.start_ms;++g_dashboard_view.seq;
}
