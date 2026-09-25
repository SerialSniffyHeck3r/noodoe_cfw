#include "Power_View.h"
#include "Product_Fonts.h"
#include <string.h>
static lv_obj_t *root;
static PowerViewModel model;
static char welcome_line[POWER_VIEW_NAME_CAPACITY+6U]="Rider";
/* Move the entire summary, including its rise animation, up by24px. Welcome
 * keeps its original positions; shared clipping expands upward only. */
#define SUMMARY_SHIFT_Y (-24)

/* The shipped Lato subset is ASCII. Keep the original UTF-8 in the App/model,
 * but render one '?' per unsupported scalar, never one per encoded byte.
 * Fit the presentation copy once per name change with the real32px metrics;
 * preserve the font/center/line height and ellipsize instead of wrapping. */
static void PrepareWelcome(void)
{
    memcpy(welcome_line,"Rider",6);uint32_t n=5,i=0;
    if(model.rider_name[0])welcome_line[n++]=' ';
    while(i<POWER_VIEW_NAME_CAPACITY&&model.rider_name[i]){
        uint8_t c=(uint8_t)model.rider_name[i++];
        welcome_line[n++]=c>=0x20U&&c<0x7FU?(char)c:'?';
        if(c>=0x80U)while(i<POWER_VIEW_NAME_CAPACITY&&((uint8_t)model.rider_name[i]&0xC0U)==0x80U)++i;
        if(n>=sizeof(welcome_line)-1U)break;
    }
    welcome_line[n]=0;
    const lv_font_t *font=Product_TextFont(32);if(!font)return;
    lv_point_t size;lv_text_get_size(&size,welcome_line,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(size.x<=288)return;
    if(n>sizeof(welcome_line)-4U)n=sizeof(welcome_line)-4U;
    do{welcome_line[n]='.';welcome_line[n+1]='.';welcome_line[n+2]='.';welcome_line[n+3]=0;
        lv_text_get_size(&size,welcome_line,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        if(size.x<=288||!n)break;
        --n;
    }while(1);
}

/* Immutable text slots survive deferred LVGL/EVE draw tasks. Fixed numeric
 * vocabulary determines the ink baseline; changing9/7/6 cannot move the row. */
static void Text(lv_layer_t *layer,const char *text,int x,int bottom,int width,
                 uint32_t pixels,uint32_t number,uint32_t rgb,lv_text_align_t align)
{
    const lv_font_t *font=number?Product_NumberFont(pixels):Product_TextFont(pixels);
    if(!font)return;
    int low=0;
    if(number)for(const char *c="0123456789.:";*c;++c){
        lv_font_glyph_dsc_t g;
        if(lv_font_get_glyph_dsc(font,&g,(uint8_t)*c,0)&&g.ofs_y<low)low=g.ofs_y;
    }
    /* Text unit vocabularies have no descenders. Match their visible bottom
     * to the full numeric envelope rather than aligning nominal font boxes. */
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=font;d.text=text;
    d.color=lv_color_hex(rgb);d.opa=model.alpha;d.align=align;
    int baseline=bottom+1+low+model.offset_y;
    lv_area_t area={x,baseline-font->line_height+font->base_line,
                   x+width-1,baseline+font->base_line};
    lv_draw_label(layer,&d,&area);
}
/* Three aligned rows share prefix/value/unit boxes. Natural digit counts
 * cannot move units or baselines; all rows rise/fade as one scene. */
static void Draw(lv_event_t *event)
{
    lv_layer_t *layer=lv_event_get_layer(event);
    if(model.kind==1){
        Text(layer,"Welcome",96,237,288,32,0,0xF2F5F7,LV_TEXT_ALIGN_CENTER);
        Text(layer,welcome_line,96,283,288,32,0,0xF2F5F7,LV_TEXT_ALIGN_CENTER);
    }else if(model.kind==2){
        Text(layer,"Ride Summary",80,185+SUMMARY_SHIFT_Y,320,32,0,0xF2F5F7,LV_TEXT_ALIGN_CENTER);
        /*252px centered composite:44+10+160+10+28. Time deliberately
         * reserves the same trailing unit column as Dist. and OIL. */
        const char *labels[]={"Dist.","Time","OIL"};
        const char *values[]={model.distance,model.ride,model.oil};
        const char *units[]={model.unit,"","%"};
        for(uint32_t i=0;i<3U;++i){int bottom=246+SUMMARY_SHIFT_Y+(int)i*56;
            Text(layer,labels[i],114,bottom,44,20,0,0xA1ADB6,LV_TEXT_ALIGN_LEFT);
            Text(layer,values[i],168,bottom,160,48,1,0xF2F5F7,LV_TEXT_ALIGN_RIGHT);
            Text(layer,units[i],338,bottom,28,20,0,0xA1ADB6,LV_TEXT_ALIGN_LEFT);
        }
    }
}
/* Allocate one transparent draw surface once, inside clock/footer bounds.
 * Only Graphics owns LVGL objects; the App owns summary values and lifetime. */
uint32_t PowerView_Create(lv_obj_t *parent)
{
    root=lv_obj_create(parent);if(!root)return 0;
    lv_obj_remove_style_all(root);lv_obj_set_pos(root,72,145+SUMMARY_SHIFT_Y);lv_obj_set_size(root,336,240-SUMMARY_SHIFT_Y);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root,Draw,LV_EVENT_DRAW_MAIN,NULL);
    lv_obj_add_flag(root,LV_OBJ_FLAG_HIDDEN);return 1;
}
void PowerView_Render(const PowerViewModel *next)
{
    if(!root||!next)return;
    if(memcmp(&model,next,sizeof(model))){
        uint32_t name_changed=memcmp(model.rider_name,next->rider_name,sizeof(model.rider_name))!=0;
        model=*next;model.rider_name[sizeof(model.rider_name)-1U]=0;
        if(name_changed)PrepareWelcome();
        lv_obj_invalidate(root);
    }
    lv_obj_set_flag(root,LV_OBJ_FLAG_HIDDEN,!model.kind);
}
