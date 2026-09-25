#include "Number_Raster.h"
#include <string.h>
/* The generator emits byte-aligned 4bpp glyph rows. A 288x128 page texture
 * reuses the phone/music bank; only the first248px contain the three cells. */
uint32_t NumberRaster_Compose(uint8_t *pixels,uint32_t bytes,const lv_font_t *font,const char *text)
{
    if(!pixels||bytes<18432U||!font||!text)return 0;
    size_t n=strlen(text);if(!n||n>3)return 0;
    const lv_font_fmt_txt_dsc_t *f=font->dsc;
    if(!f||!f->glyph_bitmap||f->bpp!=4||f->bitmap_format||!f->stride)return 0;
    int below=0;
    for(char c='0';c<='9';c++){lv_font_glyph_dsc_t g;
        if(!lv_font_get_glyph_dsc(font,&g,c,0))return 0;
        if(-g.ofs_y>below)below=-g.ofs_y;
    }
    memset(pixels,0,18432U);
    for(uint32_t i=0;i<n;i++){
        if(text[i]<'0'||text[i]>'9')return 0;
        lv_font_glyph_dsc_t g;if(!lv_font_get_glyph_dsc(font,&g,text[i],0))return 0;
        const uint8_t *src=f->glyph_bitmap+f->glyph_dsc[g.gid.index].bitmap_index;
        int x0=(int)((3U-n+i)*165U/2U)+g.ofs_x;
        int y0=128-below-g.box_h-g.ofs_y;
        if(x0<0||y0<0||x0+g.box_w>248||y0+g.box_h>128)return 0;
        for(int y=0;y<g.box_h;y++)for(int x=0;x<g.box_w;x++){
            uint32_t v=(src[y*((g.box_w+1)/2)+x/2]>>(x&1?0:4))&15U;
            uint32_t at=(y0+y)*144U+(x0+x)/2U,shift=(x0+x)&1?0:4;
            pixels[at]=(pixels[at]&~(15U<<shift))|(v<<shift);
        }
    }
    return 1;
}
