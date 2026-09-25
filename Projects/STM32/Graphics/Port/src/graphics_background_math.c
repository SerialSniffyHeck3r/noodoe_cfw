#include "Graphics_Background.h"

/* Validate byte bounds before any upload or division; keep multiplication
 * bounded by checking dimensions first. Padded/compressed inputs need a
 * separate decoder, not reinterpretation as raw pixels. */
uint32_t BackgroundImage_Valid(const BackgroundImage *i)
{
    return i&&(!!i->pixels!=!!i->read)&&i->width&&i->height&&i->width<=480U&&i->height<=480U&&
        i->bytes==i->width*i->height*2U;
}

/* No vertical gradient in the central160..320 band. Smoothstep fades into
 * clock shading above160 (fully dark by60) and ODO shading below320 (by430).
 * The default100% leaves center pixels untouched. Lower center brightness
 * raises a uniform central black alpha; edges never become brighter than it.
 * This480-byte L8 alpha mask changes only when brightness changes. */
uint32_t BackgroundShade_Alpha(uint32_t y,uint32_t percent)
{
    if(percent>100U)percent=100U;
    uint32_t t=y<160U?(y<=60U?1000U:(160U-y)*10U):
        y>320U?(y>=430U?1000U:(y-320U)*1000U/110U):0U;
    uint32_t weight=(t*t/1000U)*(3000U-2U*t)/1000U;
    uint32_t center=255U-(percent*255U+50U)/100U;
    uint32_t edge=center>238U?center:238U;
    return center+(edge-center)*weight/1000U;
}

