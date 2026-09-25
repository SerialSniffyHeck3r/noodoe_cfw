#include "Graphics_Image.h"
#include "Product_Theme.h"
#include "src/draw/eve/lv_draw_eve_private.h"
#include "src/draw/eve/lv_draw_eve_ram_g.h"
#include "src/draw/eve/lv_eve.h"
void Graphics_ImageChanged(const lv_image_dsc_t *im)
{
    if(!im||!lv_draw_eve_unit_g||(im->header.cf!=LV_COLOR_FORMAT_RGB565&&im->header.cf!=LV_COLOR_FORMAT_A4))return;
    /* get_addr reserves a block on its first call too, so always upload it.
     * Only renderer-owned static buffers are passed; no task races on pixels. */
    uint32_t address;
    (void)lv_draw_eve_ramg_get_addr(&address,(uintptr_t)im->data,im->data_size,2U);
    if(address!=LV_DRAW_EVE_RAMG_OUT_OF_RAMG)EVE_memWrite_flash_buffer(address,im->data,im->data_size);
}

void __real_lv_draw_eve_image(lv_draw_task_t *,const lv_draw_image_dsc_t *,const lv_area_t *);
/* Native byte-aligned A4 navigation masks map exactly to EVE's L4 format,
 * already used by the font driver. Upload a ROM mask once, outside the burst;
 * no CPU expansion, temporary heap buffer, or per-frame texture growth. */
static uint32_t UploadMask(const lv_image_dsc_t *im)
{
    uint32_t address;
    uint32_t present=lv_draw_eve_ramg_get_addr(&address,(uintptr_t)im->data,im->data_size,1U);
    if(!present&&address!=LV_DRAW_EVE_RAMG_OUT_OF_RAMG){
        EVE_end_cmd_burst();EVE_memWrite_flash_buffer(address,im->data,im->data_size);EVE_start_cmd_burst();
    }return address;
}
/* The pinned EVE transform branch places BITMAPS at the parent's clip origin
 * and leaves BITMAP_SIZE at the unscaled source size. An offset thumbnail then
 * samples entirely outside its texture. Interpose only our explicit2x/zero
 * pivot RGB565 case: bitmap coverage64x64, source layout32x32, local scale and
 * vertex at image coordinates. Other EVE image behavior stays upstream. */
void __wrap_lv_draw_eve_image(lv_draw_task_t *t,const lv_draw_image_dsc_t *d,const lv_area_t *coords)
{
    if(d->clip_radius||d->tile||d->bitmap_mask_src||
       d->rotation||d->scale_x!=d->scale_y||d->pivot.x||d->pivot.y||
       lv_image_src_get_type(d->src)!=LV_IMAGE_SRC_VARIABLE){
        __real_lv_draw_eve_image(t,d,coords);return;
    }
    const lv_image_dsc_t *im=d->src;
    /* Navigation uses the same corrected image-origin transform for24px
     * A4 masks,12..72px including the64px settings warning. Match this narrow format/range; retain upstream for all other
     * image operations, including unsupported pivots/rotations/masks. */
    uint32_t nav=im->header.cf==LV_COLOR_FORMAT_A4&&im->header.w==24&&im->header.h==24&&im->header.stride==12&&im->data_size==288&&d->scale_x>=128&&d->scale_x<=768;
    uint32_t warning=im->header.cf==LV_COLOR_FORMAT_A4&&im->header.w==88&&im->header.h==88&&im->header.stride==44&&im->data_size==3872&&d->scale_x>=256&&d->scale_x<=1260;
    uint32_t text=im->header.cf==LV_COLOR_FORMAT_A4&&((im->header.w==304&&im->header.h==72&&im->header.stride==152)||(im->header.w==288&&(im->header.h==128||im->header.h==144)&&im->header.stride==144)||(im->header.w==256&&im->header.h==162&&im->header.stride==128))&&im->data_size==20736&&d->scale_x==256;
    uint32_t art=im->header.cf==LV_COLOR_FORMAT_RGB565&&im->header.w==32&&im->header.h==32&&d->scale_x==512&&!d->recolor_opa;
    if(!nav&&!art&&!text&&!warning){
        __real_lv_draw_eve_image(t,d,coords);return;
    }
    uint32_t address=(nav||text||warning)?UploadMask(im):lv_draw_eve_image_upload_image(true,im);
    if(address==LV_DRAW_EVE_RAMG_OUT_OF_RAMG)return;
    lv_eve_scissor(t->clip_area.x1,t->clip_area.y1,t->clip_area.x2,t->clip_area.y2);
    lv_eve_save_context();lv_eve_color_opa(d->opa);
    if(nav||text||warning)lv_eve_color(lv_color_mix(d->recolor,lv_color_white(),d->recolor_opa));else Graphics_ColorRaw(lv_color_white());
    lv_eve_primitive(LV_EVE_PRIMITIVE_BITMAPS);lv_eve_bitmap_source(address);
    uint32_t coverage=(nav||warning)?(im->header.w*d->scale_x+255U)/256U:64U;
    lv_eve_bitmap_layout((nav||text||warning)?EVE_L4:EVE_RGB565,(text||warning)?im->header.stride:nav?12:64,(text||warning)?im->header.h:nav?24:32);
    lv_eve_bitmap_size((nav||warning)?EVE_BILINEAR:EVE_NEAREST,EVE_BORDER,EVE_BORDER,text?im->header.w:coverage,text?im->header.h:coverage);
    EVE_cmd_dl_burst(CMD_LOADIDENTITY);EVE_cmd_scale_burst(d->scale_x*256U,d->scale_y*256U);
    EVE_cmd_dl_burst(CMD_SETMATRIX);
    lv_eve_vertex_2f(coords->x1,coords->y1);lv_eve_restore_context();
}
