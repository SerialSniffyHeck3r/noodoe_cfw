#include "Product_Overlay.h"
#include "Product_Fonts.h"
/* Keep common object/style setup in one body under LTO. Called once at boot;
 * runtime rendering and the EVE display list are unchanged. */
__attribute__((noinline)) uint32_t ProductOverlay_Create(lv_obj_t *screen,lv_obj_t **panel,lv_obj_t **label,const ProductOverlayLayout *p)
{
 if(*panel)return *label!=NULL;
 *panel=lv_obj_create(screen);if(!*panel)return 0;
 lv_obj_remove_style_all(*panel);lv_obj_set_pos(*panel,p->x,p->y);lv_obj_set_size(*panel,p->w,p->h);
 lv_obj_set_style_bg_color(*panel,lv_color_hex(p->color),0);lv_obj_set_style_bg_opa(*panel,p->alpha,0);
 lv_obj_set_style_radius(*panel,p->radius,0);lv_obj_remove_flag(*panel,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
 *label=lv_label_create(*panel);if(!*label)return 0;
 lv_obj_remove_style_all(*label);lv_obj_set_pos(*label,p->lx,p->ly);lv_obj_set_size(*label,p->lw,p->lh);
 lv_obj_set_style_text_font(*label,Product_TextFont(24),0);lv_obj_set_style_text_color(*label,lv_color_white(),0);
 lv_obj_set_style_text_align(*label,LV_TEXT_ALIGN_CENTER,0);lv_label_set_long_mode(*label,LV_LABEL_LONG_CLIP);
 lv_obj_add_flag(*panel,LV_OBJ_FLAG_HIDDEN);return 1;
}
