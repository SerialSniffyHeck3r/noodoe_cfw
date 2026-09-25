#include "Product_AuxAssets.h"
#include "Product_StatusIcons.h"
#include "Product_ModeIcons.h"
#include "Product_TripIcons.h"
#include "Product_MusicIcons.h"
#include "Settings_Icons.h"
#include "Product_Icons.h"
#include "Resources.h"
#include "lvgl.h"
static uint32_t missing,bad_size,assertions;
#define CHECK(v) do{++assertions;if(!(v))return __LINE__;}while(0)
uint32_t Resources_Get(uint32_t id,ResourceView *out)
{
    static const uint32_t sizes[7]={8832,2304,1152,1728,1728,404,4448};
    if(id<13||id>19||missing==id)return 0;
    out->data=(const uint8_t *)(0xC0010000U+(id-13U)*16384U);
    out->bytes=sizes[id-13U]-(bad_size==id);return 1;
}
const void *lv_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t *d,lv_draw_buf_t *b)
{(void)d;(void)b;return 0;}
bool lv_font_get_glyph_dsc_fmt_txt(const lv_font_t *f,lv_font_glyph_dsc_t *d,uint32_t c,uint32_t n)
{(void)f;(void)d;(void)c;(void)n;return false;}
uint32_t test_bind(void)
{
    for(uint32_t id=13;id<=19;++id){
        /* Montserrat remains byte-identical in the package, but Product's
         * default style now shares Lato. Exercise its optional binder on its
         * own; do not make an unused face a mandatory runtime dependency. */
        missing=id;CHECK(!(id==13?Product_MontserratBind():Product_AuxAssetsBind()));missing=0;
        bad_size=id;CHECK(!(id==13?Product_MontserratBind():Product_AuxAssetsBind()));bad_size=0;
    }
    missing=13;CHECK(Product_AuxAssetsBind());missing=0;
    CHECK(Product_MontserratBind());
    for(uint32_t n=0;n<1000;++n)CHECK(Product_AuxAssetsBind());
    CHECK(((const lv_font_fmt_txt_dsc_t *)lv_font_montserrat_14.dsc)->glyph_bitmap==(void*)0xC0010000U);
    CHECK(lv_font_montserrat_14.get_glyph_bitmap==lv_font_get_bitmap_fmt_txt);
    CHECK(lv_font_montserrat_14.get_glyph_dsc==lv_font_get_glyph_dsc_fmt_txt);
    CHECK(lv_font_montserrat_14.line_height==16&&lv_font_montserrat_14.base_line==3);
    for(uint32_t i=0;i<8;++i)CHECK(Product_ModeIcon(i)->data==(void*)(0xC0014000U+i*288U));
    for(uint32_t i=0;i<4;++i)CHECK(Product_TripIcon(i)->data==(void*)(0xC0018000U+i*288U));
    for(uint32_t i=0;i<6;++i)CHECK(Product_MusicIcon(i)->data==(void*)(0xC001C000U+i*288U));
    for(uint32_t i=0;i<6;++i)CHECK(Settings_Icon(i)->data==(void*)(0xC0020000U+i*288U));
    CHECK(((const lv_font_fmt_txt_dsc_t *)Product_IconFont(24)->dsc)->glyph_bitmap==(void*)0xC0024000U);
    CHECK(!Product_ModeIcon(8)&&!Product_TripIcon(4)&&!Product_MusicIcon(6)&&!Settings_Icon(6));
    CHECK(Product_StatusIcon(0)->data==(void*)0xC0028000U);
    CHECK(Product_StatusIcon(1)->data==(void*)(0xC0028000U+3872));
    CHECK(Product_StatusIcon(2)->data==(void*)(0xC0028000U+4160));
    CHECK(!Product_IconFont(23));return 0;
}
uint32_t get_assertions(void){return assertions;}
