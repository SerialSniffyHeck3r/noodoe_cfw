#include "Settings_View.h"
#include "Settings_Icons.h"
#include "Product_Fonts.h"
#include "Product_MusicIcons.h"
#include "Product_TripIcons.h"
#include "Product_ModeIcons.h"
#include "Button_Hints.h"
#include "Page_Transition.h"
#include "FreeRTOS.h"
#include "BSP_Buttons.h"
#include <string.h>
#include <stdio.h>

typedef struct {lv_obj_t *root;const SettingsUI *state;uint32_t quick,now,alpha,text_index;
    uint32_t menu,row,category,start,from_row,to_row,from_category,to_category,row_q,category_q;
    char text[20][72];} SettingsView;
static SettingsView *view;
/* Power has priority over a still-finishing Settings exit. */
void SettingsView_Hide(void){if(view&&view->root)lv_obj_add_flag(view->root,LV_OBJ_FLAG_HIDDEN);}
#define WHITE 0xF2F5F7U
#define MUTED 0xA1ADB6U
#define BLUE 0x42A5F5U

/* Fixed text slots outlive all deferred draw tasks. Font metrics and widths
 * are independent of values; no content-sized objects or frame buffers. */
static void Text(lv_layer_t *layer,const char *text,int x,int baseline,int width,uint32_t pixels,uint32_t number,uint32_t rgb,uint32_t alpha)
{
    if(view->text_index>=20)return;
    char *slot=view->text[view->text_index++];snprintf(slot,72,"%s",text);
    const lv_font_t *font=number?Product_NumberFont(pixels):Product_TextFont(pixels);if(!font)return;
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.font=font;d.text=slot;d.color=lv_color_hex(rgb);d.opa=alpha;d.align=LV_TEXT_ALIGN_CENTER;
    lv_area_t area={x,baseline-font->line_height+font->base_line,x+width-1,baseline+font->base_line};
    lv_draw_label(layer,&d,&area);
}
static void Rect(lv_layer_t *layer,int x,int y,int width,int height,uint32_t radius,uint32_t rgb,uint32_t alpha)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.radius=radius;d.bg_color=lv_color_hex(rgb);d.bg_opa=alpha;
    lv_area_t a={x,y,x+width-1,y+height-1};lv_draw_rect(layer,&d,&a);
}
static void Icon(lv_layer_t *layer,uint32_t icon,int x,int y,uint32_t size,uint32_t rgb,uint32_t alpha)
{
    lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=Settings_Icon(icon);d.opa=alpha;d.recolor_opa=255;d.recolor=lv_color_hex(rgb);
    d.pivot.x=d.pivot.y=0;d.scale_x=d.scale_y=size*256U/24U;lv_area_t a={x,y,x+23,y+23};lv_draw_image(layer,&d,&a);
}
/* The footer is one fixed, centered navigation legend. Material arrow masks
 * avoid depending on Unicode glyphs absent from the ASCII font subset. */
static void Navigation(lv_layer_t *layer,uint32_t a,uint32_t rgb)
{
    Text(layer,"O to select,",78,391,128,20,0,rgb,a);
    const lv_image_dsc_t *arrows[]={Product_TripIcon(PRODUCT_TRIP_MAXIMUM),Product_MusicIcon(MUSIC_ICON_DOWN)};
    for(uint32_t i=0;i<2;++i){
        lv_draw_image_dsc_t d;lv_draw_image_dsc_init(&d);d.src=arrows[i];d.opa=a;
        d.recolor_opa=255;d.recolor=lv_color_hex(rgb);d.pivot.x=d.pivot.y=0;d.scale_x=d.scale_y=20*256/24;
        lv_area_t area={211+(int)i*23,373,234+(int)i*23,396};lv_draw_image(layer,&d,&area);
    }
    Text(layer,"to navigate",258,391,140,20,0,rgb,a);
}
/* An editor is a small menu of fields, Apply and Back. O toggles field
 * adjustment; a second O returns to this list, without applying anything.
 * Every text slot and selection rectangle is centered at the screen's X240. */
static void Editor(lv_layer_t *layer,uint32_t a,uint32_t ink,uint32_t muted)
{
    const SettingsUI *s=view->state;const SettingItem *item=SettingsCatalog_Find(s->edit_key);
    uint32_t locked=s->motion.latched;char value[64];
    Text(layer,item?item->name:"Value",60,163,360,24,0,ink,a);
    if(s->mode==4){Text(layer,"this is easter egg!",40,248,400,32,0,ink,a);return;}
    if(s->mode==2){
        Text(layer,s->edit_key==SK_BT_REPAIR?"Clear all saved phone pairings?":
            s->edit_key==SK_DEFAULTS?"Reset display preferences?":s->edit_key==SK_REBOOT?"Restart Noodoe?":"Use current service readings?",58,201,364,20,0,muted,a);
        if(s->edit_key==SK_BT_REPAIR)Text(layer,"Forget Noodoe on your phone too.",58,228,364,20,0,muted,a);
        for(uint32_t i=0;i<2;++i){int y=276+(int)i*48;
            if(i==s->confirm)Rect(layer,100,y-29,280,40,8,locked?0x20262B:0x17303F,a);
            Text(layer,i?"Apply":"Back",104,y,272,24,0,ink,a);
        }
        return;
    }
    AppSettings_FormatValue(s->edit_key,s->edit,value,sizeof(value));
    uint32_t civil=item&&(item->kind==SETTING_DATE||item->kind==SETTING_TIME);
    uint32_t numeric=item&&item->kind==SETTING_NUMBER;
    if(numeric){
        const char *unit="";
        if(s->edit_key==SK_BRIGHTNESS||s->edit_key==SK_CENTER)unit="%";
        else if(s->edit_key==SK_SCALE)unit="km/h";
        else if(s->edit_key==SK_POWER||s->edit_key==SK_BT_HOLD)unit="min";
        else if(s->edit_key>=SK_OIL_DISTANCE&&s->edit_key<=SK_SERV_DAYS){uint32_t c=(s->edit_key-SK_OIL_DISTANCE)%4;unit=c==0?"km":c==1?"h":"days";}
        if(s->edit_key>=SK_OIL_DISTANCE&&s->edit_key<=SK_SERV_DAYS&&!s->edit)Text(layer,"Off",60,225,360,32,0,ink,a);
        else{snprintf(value,sizeof(value),"%s%ld",s->edit_key==SK_BIAS&&s->edit>0?"+":"",(long)s->edit);
            Text(layer,value,128,229,224,40,1,s->mode==3&&!locked?BLUE:ink,a);
            Text(layer,unit,354,229,48,20,0,muted,a);}
    }else Text(layer,value,60,225,360,civil?32:24,civil,s->mode==3&&!locked?BLUE:ink,a);
    uint32_t fields=SettingsUI_FieldCount(s->edit_key);
    for(uint32_t i=0;i<fields+2;++i){
        int y=300+((int)i-(int)s->field)*40;if(y<260||y>340)continue;
        const char *name=i==fields?"Apply":i==fields+1?"Back":s->edit_key==SK_DATE?(i==0?"Year":i==1?"Month":"Day"):s->edit_key==SK_TIME?(i?"Minute":"Hour"):"Value";
        if(i==s->field)Rect(layer,100,y-27,280,37,8,locked?0x20262B:s->mode==3?0x21455C:0x17303F,a);
        Text(layer,name,104,y,272,24,0,i==s->field?ink:muted,a);
    }
}
/* Three visible rows with fixed64px pitch. A virtual final Back row is shared
 * by every catalog, including root. It never becomes a configuration key. */
static void Menu(lv_layer_t *layer,uint32_t a)
{
    const SettingsUI *s=view->state;uint32_t count;const SettingItem *items=SettingsCatalog_Items(s->menu,&count);
    uint32_t locked=s->motion.latched,ink=locked?0x50565B:WHITE,muted=locked?0x43494E:MUTED;
    Text(layer,SettingsCatalog_Title(s->menu),60,101,360,32,0,ink,a);
    if(s->mode)Editor(layer,a,ink,muted);
    else for(uint32_t i=0;i<=count;++i){
        int y=248+((int)i*1024-(int)view->row_q)*64/1024;
        if(y<168||y>328)continue;
        uint32_t back=i==count,selected=i==s->row,disabled=!back&&(items[i].kind==SETTING_DISABLED||
            (items[i].key==SK_PHOTO&&items[i].kind!=SETTING_SUBMENU&&!g_app_settings->facts.photo_mask));
        uint32_t has_value=!back&&items[i].kind!=SETTING_SUBMENU;
        if(selected)Rect(layer,80,y-33,320,57,10,locked?0x20262B:0x17303F,a);
        Text(layer,back?"Back":items[i].name,84,y+(has_value?-5:5),312,24,0,disabled?muted:ink,a);
        if(has_value){char value[64];AppSettings_Format(items[i].key,value,sizeof(value));Text(layer,value,84,y+19,312,16,0,muted,a);}
    }
    Navigation(layer,a,muted);
    if(locked){
        /* The movement gate is unchanged; visual simplification must never
         * make a disabled edit actionable or restart its existing deadline. */
        Rect(layer,91,142,298,216,16,0x131B21,a);
        Icon(layer,SETTINGS_ICON_SPEED,208,158,64,WHITE,a);
        Icon(layer,SETTINGS_ICON_WARNING,197,201,32,0xFFD166,a);
        Text(layer,s->motion.unknown?"Speed unavailable":"Settings paused",90,267,300,24,0,WHITE,a);
        char remaining[40];snprintf(remaining,sizeof(remaining),"Return in %lu s",(unsigned long)((30000-s->motion.elapsed_ms+999)/1000));
        Text(layer,remaining,100,303,280,24,0,0xFFD166,a);
        Text(layer,"Stop below 3 km/h to resume",92,334,296,16,0,MUTED,a);
        SettingsMotion_Draw(layer,s->motion.elapsed_ms,a);
    }else SettingsPosition_Draw(layer,view->category_q,view->row_q,count+1,a,s->menu!=0);
}
static void Draw(lv_event_t *event)
{
    if(!view->state)return;
    view->text_index=0;lv_layer_t *layer=lv_event_get_layer(event);
    if(view->alpha)Menu(layer,view->alpha);
    if(view->state->message[0]&&(!view->state->open||view->alpha>200)&&!view->state->motion.latched){
        Rect(layer,98,332,272,40,12,0x223641,255);
        Text(layer,view->state->message,104,359,260,20,0,WHITE,255);
    }
}
uint32_t SettingsView_Create(lv_obj_t *screen)
{
    if(!screen)return 0;
    if(!view){view=pvPortMalloc(sizeof(*view));if(!view)return 0;memset(view,0,sizeof(*view));}
    view->root=lv_obj_create(screen);if(!view->root)return 0;
    lv_obj_remove_style_all(view->root);lv_obj_set_size(view->root,480,480);lv_obj_set_pos(view->root,0,0);
    lv_obj_remove_flag(view->root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view->root,Draw,LV_EVENT_DRAW_MAIN,NULL);lv_obj_add_flag(view->root,LV_OBJ_FLAG_HIDDEN);
    ButtonHints_Reparent(screen);return 1;
}
/* Navigation stores no history in the renderer. Every retarget starts at the
 * currently interpolated Q1024 position, including repeated rapid presses. */
void SettingsView_Render(const SettingsUI *s,uint32_t quick,uint32_t now)
{
    if(!view||!view->root||!s)return;
    uint32_t age=now-view->start,ease=PageTransition_Ease(age>=240?240:age);
    view->row_q=view->from_row+((int32_t)view->to_row-(int32_t)view->from_row)*(int32_t)ease/1024;
    view->category_q=view->from_category+((int32_t)view->to_category-(int32_t)view->from_category)*(int32_t)ease/1024;
    if(view->menu!=s->menu){view->row_q=s->row*1024;view->menu=s->menu;}
    if(view->to_row!=s->row*1024||view->to_category!=s->category*1024){
        view->from_row=view->row_q;view->from_category=view->category_q;view->to_row=s->row*1024;view->to_category=s->category*1024;view->start=now;
    }
    view->state=s;view->quick=quick;view->now=now;view->alpha=s->pose.settings_alpha;
    lv_obj_set_flag(view->root,LV_OBJ_FLAG_HIDDEN,!s->open);lv_obj_invalidate(view->root);
    if(quick||s->open)ButtonHints_ShowKeys(quick?UI_SYSTEM+1:100+s->menu,quick?255:s->pose.settings_alpha,now);
}
