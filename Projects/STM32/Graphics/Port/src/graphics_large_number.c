#include "Graphics_LargeNumber.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_draw_eve_ram_g.h"
#include "src/draw/eve/lv_eve.h"
#include "src/draw/lv_draw_private.h"
#include <string.h>
static const uint8_t tag;
/* Upstream deliberately ignores tagged tasks. Claim just our label before
 * scheduling; wrapping its execution alone cannot make an unclaimed task run. */
uint32_t Graphics_LargeNumberEvaluate(lv_draw_task_t *task)
{
    if(task->type!=LV_DRAW_TASK_TYPE_LABEL||
       ((lv_draw_label_dsc_t*)task->draw_dsc)->base.user_data!=&tag)return 0;
    task->preference_score=0;task->preferred_draw_unit_id=9;return 1;
}
void Graphics_DrawLargeNumber(lv_layer_t *layer,const lv_font_t *font,const char *text,int x,int bottom,uint32_t opacity)
{
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);
    d.base.user_data=(void*)&tag;d.font=font;d.text=text;d.text_local=0;
    d.color=lv_color_hex(0xF2F5F7);d.opa=opacity;
    lv_area_t a={x,bottom-149,x+247,bottom};lv_draw_label(layer,&d,&a);
}
void __real_lv_draw_eve_label(lv_draw_task_t*,const lv_draw_label_dsc_t*,const lv_area_t*);
/* Scale the same cached D-DIN64 masks; no new font or duplicate bitmap.
 * Fixed baseline uses the complete digit vocabulary, not the current ink. */
void __wrap_lv_draw_eve_label(lv_draw_task_t *t,const lv_draw_label_dsc_t *d,const lv_area_t *a)
{
    if(d->base.user_data!=&tag){__real_lv_draw_eve_label(t,d,a);return;}
    size_t n=strlen(d->text);if(!n||n>3||!d->opa)return;
    int below=0;
    for(char c='0';c<='9';++c){lv_font_glyph_dsc_t g;if(lv_font_get_glyph_dsc(d->font,&g,c,0)&&-g.ofs_y>below)below=-g.ofs_y;}
    lv_eve_scissor(t->clip_area.x1,t->clip_area.y1,t->clip_area.x2,t->clip_area.y2);
    lv_eve_save_context();lv_eve_color(d->color);lv_eve_color_opa(d->opa);
    lv_eve_primitive(LV_EVE_PRIMITIVE_BITMAPS);
    EVE_cmd_dl_burst(CMD_LOADIDENTITY);EVE_cmd_scale_burst(163840,163840);EVE_cmd_dl_burst(CMD_SETMATRIX);
    for(size_t i=0;i<n;++i){lv_font_glyph_dsc_t g;
        if(!lv_font_get_glyph_dsc(d->font,&g,(uint8_t)d->text[i],0)||!g.box_w)continue;
        uint32_t address=lv_draw_eve_label_upload_glyph(true,d->font->dsc,g.gid.index);
        if(address==LV_DRAW_EVE_RAMG_OUT_OF_RAMG)continue;
        lv_eve_bitmap_source(address);lv_eve_bitmap_layout(EVE_L4,(g.box_w+1)/2,g.box_h);
        lv_eve_bitmap_size(EVE_BILINEAR,EVE_BORDER,EVE_BORDER,(g.box_w*5+1)/2,(g.box_h*5+1)/2);
        int x=a->x1+((3-(int)n+(int)i)*165+g.ofs_x*5)/2;
        int y=a->y2+1-((below+g.box_h+g.ofs_y)*5+1)/2;
        lv_eve_vertex_2f(x,y);
    }
    lv_eve_restore_context();
}
