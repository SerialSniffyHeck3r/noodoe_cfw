#include "Product_Theme.h"
#include "Graphics_Background.h"
#include "Graphics_BackgroundDraw.h"
#include "Graphics_BackgroundClip.h"
#include "Graphics.h"
#include "BSP_Display.h"
#include "Scalar_Transition.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/eve/lv_eve.h"
#include <string.h>
static const uint8_t background_tag;
static BackgroundImage selected,cached[3],loading_image;
uint32_t GraphicsBackground_SourceInUse(const void *pixels)
{return pixels&&(selected.pixels==pixels||(g_background.loading&&loading_image.pixels==pixels));}
static uint32_t shade_dirty,upload_bank,base_bank,overlay_bank,last_reference[3];
static ScalarTransition fade,brightness,mix;
static uint32_t held,last_step;
enum {BG_IDLE,BG_FADE_OUT,BG_RETIRE,BG_LOAD,BG_FADE_IN,BG_BLEND};
static uint8_t shade[480],upload_buffer[BACKGROUND_UPLOAD_CHUNK];
volatile BackgroundDiagnostics g_background;
_Static_assert(BACKGROUND_IMAGE_RAM+BACKGROUND_IMAGE_BYTES==BACKGROUND_IMAGE_B_RAM,"photo A overlap");
_Static_assert(BACKGROUND_IMAGE_B_RAM+BACKGROUND_IMAGE_BYTES==BSP_DISPLAY_CAPTURE_RAM_G,"photo B overlap");
static uint32_t Same(const BackgroundImage *a,const BackgroundImage *b)
{return a->pixels==b->pixels&&a->width==b->width&&a->height==b->height&&a->bytes==b->bytes&&a->revision==b->revision&&a->read==b->read&&a->context==b->context;}
uint32_t GraphicsBackground_Settled(void)
{
    if(g_background.read_error)return 1; /* Exposed failure, never spin asleep forever. */
    return !g_background.loading&&g_background.phase==BG_IDLE&&!shade_dirty&&
        !fade.duration_ms&&!mix.duration_ms&&!brightness.duration_ms&&
        (!selected.bytes||(base_bank<3&&Same(&selected,&cached[base_bank])));
}
static uint32_t BankAddress(uint32_t bank)
{return bank==2?BACKGROUND_SMALL_RAM:bank?BACKGROUND_IMAGE_B_RAM:BACKGROUND_IMAGE_RAM;}
void GraphicsBackground_Init(void)
{
    memset(&selected,0,sizeof(selected));memset(cached,0,sizeof(cached));memset(last_reference,0,sizeof(last_reference));
    memset(&loading_image,0,sizeof(loading_image));memset((void*)&g_background,0,sizeof(g_background));
    g_background.magic=0x42474E31;g_background.version=3;g_background.center_percent=g_background.display_percent=100;
    fade=(ScalarTransition){0};mix=(ScalarTransition){0};brightness=(ScalarTransition){.from=100,.target=100,.value=100};
    base_bank=overlay_bank=upload_bank=3;shade_dirty=1;
    held=0;last_step=lv_tick_get();
}
/* Request stores the latest descriptor without reading pixels or invalidating
 * either currently visible bank. A reversal reuses the sampled blend position. */
uint32_t GraphicsBackground_SetImage(const BackgroundImage *image)
{
    if(held)return 0;
    if(image&&!BackgroundImage_Valid(image))return 0;
    BackgroundImage next={0};if(image)next=*image;if(Same(&selected,&next))return 1;
    selected=next;++g_background.requests;g_background.read_error=0;
    g_background.total=next.bytes;g_background.width=next.width;g_background.height=next.height;
    uint32_t now=lv_tick_get();(void)ScalarTransition_Value(&fade,now);(void)ScalarTransition_Value(&mix,now);
    ScalarTransition_Request(&fade,image?255:0,BACKGROUND_FADE_MS,now);
    if(g_background.phase==BG_BLEND&&image){
        if(base_bank<3&&Same(&selected,&cached[base_bank]))ScalarTransition_Request(&mix,0,BACKGROUND_FADE_MS,now);
        else if(overlay_bank<3&&Same(&selected,&cached[overlay_bank]))ScalarTransition_Request(&mix,255,BACKGROUND_FADE_MS,now);
    }
    return 1;
}
uint32_t GraphicsBackground_SetCenterBrightness(uint32_t percent)
{
    if(held||percent>100)return 0;
    g_background.center_percent=percent;
    ScalarTransition_Request(&brightness,percent,BACKGROUND_FADE_MS,lv_tick_get());return 1;
}
/* The retained display list samples this L8 texture directly. Updating only
 * its480bytes changes shade without rebuilding text, ring or photo banks. */
static void UploadShade(void)
{
    if(!shade_dirty)return;
    for(uint32_t y=0;y<480;++y)shade[y]=(uint8_t)BackgroundShade_Alpha(y,g_background.display_percent);
    EVE_memWrite_flash_buffer(BACKGROUND_SHADE_RAM,shade,sizeof(shade));
    shade_dirty=0;++g_background.shade_uploads;
}
/* Freeze every background property at the last processed frame. This API
 * performs NO GPU writes: brightness, image and blend all remain unchanged.
 * Resuming excludes the held interval from all three animation clocks. */
void GraphicsBackground_HoldFrame(uint32_t hold)
{
    if(!hold){
        if(held){uint32_t now=lv_tick_get(),elapsed=now-last_step;
            fade.start_ms+=elapsed;mix.start_ms+=elapsed;brightness.start_ms+=elapsed;
            held=0;last_step=now;}
        return;
    }
    held=1;
}
/* Start the same240ms animation only after every byte has reached RAM_G.
 * No cold bank can be exposed, and no third texture interrupts a two-way mix. */
static void Activate(uint32_t bank,uint32_t now)
{
    g_background.loading=0;g_background.ready=1;g_background.resident_width=cached[bank].width;g_background.resident_height=cached[bank].height;
    if(base_bank==3){fade=(ScalarTransition){0};g_background.alpha=0;}
    ScalarTransition_Request(&fade,255,BACKGROUND_FADE_MS,now);
    if(base_bank==3||!fade.value){base_bank=bank;overlay_bank=3;g_background.phase=BG_IDLE;return;}
    overlay_bank=bank;mix=(ScalarTransition){0};ScalarTransition_Request(&mix,255,BACKGROUND_FADE_MS,now);g_background.phase=BG_BLEND;
}
void GraphicsBackground_Process(void)
{
    if(g_graphics.render_busy||BSP_Display_CaptureBusy())return;
    if(held)return;
    uint32_t now=lv_tick_get();g_background.alpha=ScalarTransition_Value(&fade,now);(void)ScalarTransition_Value(&mix,now);
    last_step=now;
    uint32_t light=ScalarTransition_Value(&brightness,now);
    if(light!=g_background.display_percent){g_background.display_percent=light;shade_dirty=1;}
    UploadShade();
    if(g_background.phase==BG_BLEND){
        if(mix.duration_ms)return;
        if(mix.value==255)base_bank=overlay_bank;
        if(base_bank<3){g_background.resident_width=cached[base_bank].width;g_background.resident_height=cached[base_bank].height;}
        overlay_bank=3;mix=(ScalarTransition){0};g_background.phase=BG_IDLE;
    }
    if(g_background.read_error)return;
    if(!selected.bytes){g_background.ready=!!g_background.alpha;g_background.loading=0;g_background.phase=BG_IDLE;return;}
    if(base_bank<3&&Same(&selected,&cached[base_bank])){g_background.ready=1;g_background.loading=0;g_background.phase=BG_IDLE;return;}
    for(uint32_t bank=0;bank<3;++bank)if(cached[bank].bytes&&Same(&selected,&cached[bank])){Activate(bank,now);return;}
    if(g_background.phase!=BG_LOAD||!Same(&loading_image,&selected)){
        upload_bank=selected.bytes<=BACKGROUND_SMALL_BYTES?2:base_bank==0?1:0;
        /* If new small artwork would overwrite the visible small bank, stage
         * it in the unused full bank; the2KiB slot is only a preferred cache. */
        if(upload_bank==base_bank)upload_bank=base_bank==0?1:0;
        if(last_reference[upload_bank]&&!Graphics_FramePresentedAfter(last_reference[upload_bank]))return;
        loading_image=selected;cached[upload_bank]=(BackgroundImage){0};
        g_background.uploaded=0;g_background.loading=1;g_background.phase=BG_LOAD;
    }
    uint32_t left=loading_image.bytes-g_background.uploaded;
    uint32_t limit=loading_image.pixels?BACKGROUND_DIRECT_UPLOAD_CHUNK:BACKGROUND_UPLOAD_CHUNK,n=left>limit?limit:left;
    const uint8_t *data=loading_image.pixels?loading_image.pixels+g_background.uploaded:upload_buffer;
    if(loading_image.read){int32_t result=loading_image.read(loading_image.context,g_background.uploaded,upload_buffer,n);
        if(!result)return;
        if(result!=1){g_background.read_error=1;g_background.loading=0;g_background.phase=BG_IDLE;return;}}
    EVE_memWrite_flash_buffer(BankAddress(upload_bank)+g_background.uploaded,data,n);g_background.uploaded+=n;
    if(g_background.uploaded==loading_image.bytes){cached[upload_bank]=loading_image;++g_background.image_uploads;Activate(upload_bank,lv_tick_get());}
}

/* A fill task gives LVGL a complete480px dependency/clip rectangle without
 * allocating any software layer. It is claimed before normal fill evaluation. */
void GraphicsBackground_Draw(lv_layer_t *layer)
{
    if(!layer||!g_background.ready||!g_background.alpha)return;
    lv_draw_fill_dsc_t d;lv_draw_fill_dsc_init(&d);d.base.user_data=(void*)&background_tag;
    lv_area_t a={0,0,479,479};lv_draw_fill(layer,&d,&a);
}
uint32_t GraphicsBackground_Evaluate(lv_draw_task_t *task)
{
    if(task->type!=LV_DRAW_TASK_TYPE_FILL||((lv_draw_fill_dsc_t*)task->draw_dsc)->base.user_data!=&background_tag)return 0;
    task->preference_score=0;task->preferred_draw_unit_id=9;return 1;
}

/* FT81x uses8.8 coefficients (the1.15 mode belongs to later BT81x chips).
 * Uniform center-crop fills the circle without changing aspect ratio. L8 is
 * an alpha-only texture: tinting a1x480 strip black supplies the vertical fade
 * in one bitmap, with X transform zero to repeat the same sample everywhere.
 * Reference: FT81X Series Programmer Guide, bitmap formats/transforms.
 * https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf
 */
/* Draw one retained bank; the black shade is drawn once after compositing.
 * The full photo is the opaque base and artwork is an alpha overlay. */
static void DrawBank(uint32_t bank,uint32_t alpha)
{
    const BackgroundImage *image=&cached[bank];
    last_reference[bank]=g_graphics.render_count+1U;
    Graphics_ColorRaw(lv_color_white());lv_eve_color_opa(alpha);
    lv_eve_primitive(LV_EVE_PRIMITIVE_BITMAPS);lv_eve_bitmap_source(BankAddress(bank));
    lv_eve_bitmap_layout(EVE_RGB565,image->width*2U,image->height);
    lv_eve_bitmap_size(EVE_BILINEAR,EVE_BORDER,EVE_BORDER,480,480);
    uint32_t w=image->width,h=image->height,side=w<h?w:h,scale=side*256U/480U;
    EVE_cmd_dl_burst(BITMAP_TRANSFORM_A(scale));EVE_cmd_dl_burst(BITMAP_TRANSFORM_B(0));
    EVE_cmd_dl_burst(BITMAP_TRANSFORM_C((w-side)*128U));EVE_cmd_dl_burst(BITMAP_TRANSFORM_D(0));
    EVE_cmd_dl_burst(BITMAP_TRANSFORM_E(scale));EVE_cmd_dl_burst(BITMAP_TRANSFORM_F((h-side)*128U));
    lv_eve_vertex_2f(0,0);
}
uint32_t GraphicsBackground_Dispatch(lv_draw_task_t *task)
{
    if(task->type!=LV_DRAW_TASK_TYPE_FILL||((lv_draw_fill_dsc_t*)task->draw_dsc)->base.user_data!=&background_tag)return 0;
    lv_eve_scissor(task->clip_area.x1,task->clip_area.y1,task->clip_area.x2,task->clip_area.y2);
    lv_eve_save_context();GraphicsBackground_ClipBegin();
    if(base_bank<3&&cached[base_bank].bytes)DrawBank(base_bank,g_background.alpha);
    if(overlay_bank<3&&cached[overlay_bank].bytes&&mix.value)
        DrawBank(overlay_bank,g_background.alpha*mix.value/255U);
    lv_eve_color_opa(255);lv_eve_color(lv_color_black());lv_eve_bitmap_source(BACKGROUND_SHADE_RAM);
    lv_eve_bitmap_layout(EVE_L8,1,480);lv_eve_bitmap_size(EVE_NEAREST,EVE_BORDER,EVE_BORDER,480,480);
    EVE_cmd_dl_burst(BITMAP_TRANSFORM_A(0));EVE_cmd_dl_burst(BITMAP_TRANSFORM_C(0));
    EVE_cmd_dl_burst(BITMAP_TRANSFORM_E(256));EVE_cmd_dl_burst(BITMAP_TRANSFORM_F(0));
    lv_eve_vertex_2f(0,0);GraphicsBackground_ClipEnd();lv_eve_restore_context();++g_background.draws;return 1;
}
