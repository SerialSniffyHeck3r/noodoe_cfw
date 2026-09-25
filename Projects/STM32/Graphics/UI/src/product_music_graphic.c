#include "Music_Graphic.h"
#include "src/misc/lv_area_private.h"
#include <string.h>
#include "Product_MusicIcons.h"
#include "Product_ModeIcons.h"
#include "Product_TripIcons.h"
#include "SpeedHome_Layout.h"
#include "Button_Hints.h"
#include "BSP_Buttons.h"
#include "BSP_RAM.h"
#include "PhoneVisual.h"
#include "Graphics_Image.h"
#include "Graphics.h"
#include "Number_Raster.h"
#include "Product_Fonts.h"
#include "Product_CallIcon.h"

/* Music owns only which immutable assets represent its current actions. */
void MusicGraphic_Hints(uint32_t playing,uint32_t scope,uint32_t alpha,uint32_t now)
{
    /* Music supplies only bindings. Geometry, sizes, colors and release timers
     * belong to the same public renderer used by future pages. */
    const ButtonHintBinding rows[3]={
        {BSP_BUTTON_UP,Product_TripIcon(PRODUCT_TRIP_MAXIMUM),Product_MusicIcon(playing?MUSIC_ICON_PAUSE:MUSIC_ICON_PLAY),Product_MusicIcon(MUSIC_ICON_PREVIOUS)},
        {BSP_BUTTON_ENTER,Product_MusicIcon(MUSIC_ICON_CIRCLE),NULL,NULL},
        {BSP_BUTTON_DOWN,Product_MusicIcon(MUSIC_ICON_DOWN),Product_MusicIcon(MUSIC_ICON_NEXT),NULL}};
    ButtonHints_Update(rows,scope,alpha,now);
}
/* Progress belongs to the moving page body; key hints are physical-edge
 * anchors and only fade, avoiding a slide into the fixed speed ring. */
static void Progress(lv_event_t *event)
{
    MusicGraphic *v=lv_event_get_user_data(event);if(!v->known&&!v->panel)return;
    lv_area_t origin;lv_obj_get_coords(v->root,&origin);lv_layer_t *layer=lv_event_get_layer(event);
    if(v->call_color){
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=Product_CallIcon();
        d.opa=v->alpha;d.recolor=lv_color_hex(v->call_color);d.recolor_opa=255;
        lv_area_t a={origin.x1+68,origin.y1+160,origin.x1+91,origin.y1+183};lv_draw_image(layer,&d,&a);
        lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);line.width=2;line.opa=v->alpha;
        line.color=lv_color_hex(v->call_color);line.round_start=line.round_end=1;
        int x=origin.x1+100,y=origin.y1+165;
        /* Outgoing NE arrow, incoming SW arrow, missed/rejected red cross. */
        int sign=v->call_color==0x5BCB9AU?-1:1;
        line.p1=(lv_point_precise_t){x-4,y+4};line.p2=(lv_point_precise_t){x+4,y-4};lv_draw_line(layer,&line);
        if(v->call_color==0xFF796EU){line.p1=(lv_point_precise_t){x-4,y-4};line.p2=(lv_point_precise_t){x+4,y+4};lv_draw_line(layer,&line);}
        else{line.p1=(lv_point_precise_t){x+4*sign,y-4*sign};line.p2=(lv_point_precise_t){x-3*sign,y-4*sign};lv_draw_line(layer,&line);line.p2=(lv_point_precise_t){x+4*sign,y+3*sign};lv_draw_line(layer,&line);}
    }
    if(v->panel==1&&v->reply_count){
        lv_draw_rect_dsc_t r;lv_draw_rect_dsc_init(&r);r.bg_opa=v->alpha;r.bg_color=lv_color_hex(0x174D72);r.radius=4;
        lv_area_t selected={origin.x1+24,origin.y1+6+(int)v->reply_index*(v->text_ready==162?27:21),origin.x1+311,origin.y1+(v->text_ready==162?32:26)+(int)v->reply_index*(v->text_ready==162?27:21)};
        lv_draw_rect(layer,&r,&selected);
    }
    if(v->text_ready){lv_draw_image_dsc_t image;lv_draw_image_dsc_init(&image);image.src=&v->text_image;image.opa=v->alpha;
        image.recolor=lv_color_white();image.recolor_opa=255;image.pivot.x=image.pivot.y=0;
        lv_area_t text={origin.x1+16,origin.y1+120,origin.x1+319,origin.y1+191};
        if(v->panel==1){int x=(336-v->text_image.header.w)/2;text=(lv_area_t){origin.x1+x,origin.y1+6,origin.x1+x+v->text_image.header.w-1,origin.y1+5+(int)v->text_image.header.h};}
        if(v->panel==2)text=(lv_area_t){origin.x1+6,origin.y1+30,origin.x1+293,origin.y1+157};
        /* Contact rows may slide, but their fixed viewport may not cover the
         * category heading or action footer. Restore the layer clip locally. */
        lv_area_t clip=layer->_clip_area;
        uint32_t visible=1;
        if(v->panel==1&&(v->text_ready==192||v->text_ready==144||v->text_ready==145)){
            /* Packed status/app/title/body share one bounded20.25KiB bank.
             * Restore transparent gutters using UI clips rather than storing
             * empty rows in GPU memory. Pixels stay one-to-one. */
            static const uint8_t old_starts[]={0,24,56,88,128},old_y[]={0,34,84,132};
            static const uint8_t new_starts[]={0,30,70,114,144},new_y[]={0,40,90,148};
            static const uint8_t compact_starts[]={0,20,48,84,144},compact_y[]={6,29,60,99};
            const uint8_t *starts=v->text_ready==145?compact_starts:v->text_ready==144?new_starts:old_starts;
            const uint8_t *ys=v->text_ready==145?compact_y:v->text_ready==144?new_y:old_y;
            for(uint32_t i=0;i<4;i++){
                lv_area_t window=text,part=text;
                window.y1+=ys[i];window.y2=window.y1+starts[i+1]-starts[i]-1;
                part.y1+=ys[i]-starts[i];part.y2+=ys[i]-starts[i];
                if(lv_area_intersect(&layer->_clip_area,&clip,&window))lv_draw_image(layer,&image,&part);
            }
        }else{
            if(v->panel==1){lv_area_t window=text;visible=lv_area_intersect(&layer->_clip_area,&clip,&window);text.y1+=v->slide_y;text.y2+=v->slide_y;}
            if(visible)lv_draw_image(layer,&image,&text);
        }
        layer->_clip_area=clip;}
    if(v->panel)return;
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=3;d.bg_opa=v->alpha;d.bg_color=lv_color_hex(0x48545C);
    lv_area_t a={origin.x1+28,origin.y1+209,origin.x1+307,origin.y1+214};lv_draw_rect(layer,&d,&a);
    uint32_t fill=280U*v->ratio/1000U;
    if(fill){a.x2=a.x1+(int)fill-1;d.bg_color=lv_color_hex(0xF2F5F7);lv_draw_rect(layer,&d,&a);}
}
uint32_t MusicGraphic_Create(MusicGraphic *v,lv_obj_t *body)
{
    if(!v||!body)return 0;
    *v=(MusicGraphic){0};
    v->root=lv_obj_create(body);if(!v->root)return 0;
    uint8_t *pixels=BSP_RAM_Allocate(PHONE_PANEL_BYTES);if(!pixels)return 0;
    memset(pixels,0,PHONE_PANEL_BYTES);
    v->text_image=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_A4,.w=304,.h=72,.stride=152},.data_size=PHONE_PANEL_BYTES,.data=pixels};
    lv_obj_remove_style_all(v->root);lv_obj_set_size(v->root,336,SPEED_HOME_CONTENT_HEIGHT-40);
    lv_obj_remove_flag(v->root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v->root,Progress,LV_EVENT_DRAW_MAIN,v);
    lv_obj_add_flag(v->root,LV_OBJ_FLAG_HIDDEN);return 1;
}
/* Owner-only snapshots keep the outgoing page frozen during transitions. */
static uint32_t BindText(MusicGraphic *v,uint32_t key,uint32_t panel)
{
    if(v->text_key!=key||v->panel!=panel){
        v->panel=panel;v->text_image.header.w=panel?288:304;v->text_image.header.h=panel?128:72;v->text_image.header.stride=panel?144:152;
        /* Retire a previously scanned texture before reusing its fixed bank.
         * LVGL object hiding alone is not an EVE display-list swap fence. */
        v->text_key=key;v->text_ready=0;v->text_wait=v->text_revision!=0;
        v->text_fence=g_graphics.render_count;lv_obj_invalidate(v->root);
    }
    if(v->text_wait){if(!Graphics_FramePresentedAfter(v->text_fence))return 0;v->text_wait=0;}
    /* Visible panel bytes are frozen. Revision changes are composed into the
     * other physically retired page bank by DashboardPagesView_Render. */
    if(panel&&v->text_ready)return v->text_ready;
    uint32_t before=v->text_revision,ready=v->text_ready;
    v->text_ready=(panel?PhoneVisual_CopyPanel:PhoneVisual_CopyMask)(key,(void*)v->text_image.data,&v->text_revision);
    if(panel){v->text_image.header.w=v->text_ready==162?256:288;v->text_image.header.h=v->text_ready==162?162:v->text_ready==144||v->text_ready==145?144:128;v->text_image.header.stride=v->text_ready==162?128:144;}
    if(v->text_ready&&before!=v->text_revision)Graphics_ImageChanged(&v->text_image);
    if(ready!=v->text_ready||before!=v->text_revision)lv_obj_invalidate(v->root);
    return v->text_ready;
}
uint32_t MusicGraphic_Text(MusicGraphic *v,uint32_t key)
{
    if(PhoneVisual_MusicRevision(key)){
        if(v->panel||v->text_key!=key){v->text_ready=0;v->text_revision=0;v->text_key=key;v->panel=0;}
        return v->text_ready;
    }
    return BindText(v,key,0);
}
/* Only the invisible, physically retired page bank calls this function.
 * The visible bank retains its complete previous window during a tile gap. */
uint32_t MusicGraphic_Marquee(MusicGraphic *v,uint32_t key)
{
    if(v->panel||v->text_key!=key)v->text_revision=0;
    v->panel=0;v->text_key=key;v->text_wait=0;v->text_ready=0;
    v->text_image.header.w=304;v->text_image.header.h=72;v->text_image.header.stride=152;
    if(!PhoneVisual_CopyMusic(key,(void*)v->text_image.data,&v->text_revision))return 0;
    v->text_ready=1;Graphics_ImageChanged(&v->text_image);lv_obj_invalidate(v->root);return 1;
}
/* Called only for a retired/invisible page bank. The view swaps complete
 * pages after upload; no visible texture is changed underneath EVE. */
uint32_t MusicGraphic_Number(MusicGraphic *v,const char *text)
{
    v->panel=2;v->reply_count=0;v->text_wait=0;v->text_key=0;
    v->text_image.header.w=288;v->text_image.header.h=128;v->text_image.header.stride=144;
    v->text_ready=NumberRaster_Compose((void*)v->text_image.data,PHONE_PANEL_BYTES,Product_NumberFont(160),text);
    if(v->text_ready){++v->text_revision;Graphics_ImageChanged(&v->text_image);}
    lv_obj_invalidate(v->root);return v->text_ready;
}
uint32_t MusicGraphic_Panel(MusicGraphic *v,uint32_t key,uint32_t selection,uint32_t count)
{v->reply_index=selection;v->reply_count=count;return BindText(v,key,1);}
void MusicGraphic_Update(MusicGraphic *v,uint32_t show,uint32_t known,uint32_t playing,uint32_t ratio)
{
    lv_obj_set_flag(v->root,LV_OBJ_FLAG_HIDDEN,!show);
    v->known=known;v->playing=playing;v->ratio=ratio>1000?1000:ratio;
    lv_obj_invalidate(v->root);
}
void MusicGraphic_Alpha(MusicGraphic *v,uint32_t alpha)
{if(v->alpha!=alpha){v->alpha=alpha;lv_obj_invalidate(v->root);}}
