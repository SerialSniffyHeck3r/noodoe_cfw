#include "Toast_View.h"
#include "Product_Fonts.h"
#include "Page_Transition.h"
#include "SpeedHome_Layout.h"
#include <string.h>
static struct {lv_obj_t *root,*label;char display[TOAST_MESSAGE_CAPACITY];uint32_t alpha,revision;} toast;

/* Pinned LVGL cannot apply LONG_DOT to static text. Fit the accepted ASCII
 * message once per change, using the actual font metrics and our own bounded
 * static display buffer. The model owns the original text and revision; do
 * not duplicate its64-byte buffer in this SRAM-constrained target. */
static void FitMessage(const char *text)
{
    memcpy(toast.display,text,sizeof(toast.display));toast.display[sizeof(toast.display)-1]=0;
    lv_point_t size;const lv_font_t *font=Product_TextFont(24);
    lv_text_get_size(&size,toast.display,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(size.x>248){
        uint32_t n=strlen(toast.display);if(n>TOAST_MESSAGE_CAPACITY-4)n=TOAST_MESSAGE_CAPACITY-4;
        for(;;){
            toast.display[n]='.';toast.display[n+1]='.';toast.display[n+2]='.';toast.display[n+3]=0;
            lv_text_get_size(&size,toast.display,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            if(size.x<=248||n==0)break;
            --n;
        }
    }
    lv_label_set_text_static(toast.label,toast.display);
}

/* Fixed272x48 panel atX104/Y330. One24px line uses ellipsis for a long message;
 * the full accepted message remains in the model. No per-toast LVGL objects. */
uint32_t ToastView_Create(lv_obj_t *content)
{
    if(!content)return 0;
    memset(&toast,0,sizeof(toast));
    toast.root=lv_obj_create(content);if(!toast.root)return 0;
    lv_obj_remove_style_all(toast.root);lv_obj_set_pos(toast.root,104-SPEED_HOME_CONTENT_X,330-SPEED_HOME_CONTENT_Y);
    lv_obj_set_size(toast.root,272,48);lv_obj_set_style_radius(toast.root,10,0);
    lv_obj_set_style_bg_color(toast.root,lv_color_hex(0x142B36),0);
    lv_obj_set_style_border_color(toast.root,lv_color_hex(0x8BA4AE),0);lv_obj_set_style_border_width(toast.root,1,0);
    lv_obj_remove_flag(toast.root,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    toast.label=lv_label_create(toast.root);if(!toast.label)return 0;
    lv_obj_remove_style_all(toast.label);lv_obj_set_pos(toast.label,12,10);lv_obj_set_size(toast.label,248,30);
    lv_obj_set_style_text_font(toast.label,Product_TextFont(24),0);lv_obj_set_style_text_color(toast.label,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_align(toast.label,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(toast.label,LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(toast.label,toast.display);toast.alpha=UINT32_MAX;lv_obj_add_flag(toast.root,LV_OBJ_FLAG_HIDDEN);return 1;
}
void ToastView_Render(const PopupNotificationSnapshot *s)
{
    if(!toast.root||!s)return;
    lv_obj_set_flag(toast.root,LV_OBJ_FLAG_HIDDEN,!s->visible);if(!s->visible)return;
    if(toast.revision!=s->revision){FitMessage(s->text);toast.revision=s->revision;}
    /* Share the global Slow-Fast-Slow curve for the first/last240ms. Duration
     * includes both fades and is driven by real time even if page preview freezes. */
    uint32_t remaining=s->duration_ms>s->elapsed_ms?s->duration_ms-s->elapsed_ms:0;
    uint32_t edge=s->elapsed_ms<remaining?s->elapsed_ms:remaining;
    uint32_t alpha=PageTransition_Ease(edge)*255U/1024U;
    if(alpha!=toast.alpha){toast.alpha=alpha;lv_obj_set_style_bg_opa(toast.root,alpha*240U/255U,0);
        lv_obj_set_style_border_opa(toast.root,alpha,0);lv_obj_set_style_text_opa(toast.label,alpha,0);}
}
