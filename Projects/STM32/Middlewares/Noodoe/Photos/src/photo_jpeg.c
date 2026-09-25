/* Compile the pinned upstream decoder without registering an unused LVGL file
 * decoder. Source/version/license remain in Third_Party/LVGL, unmodified. */
#include "Photo_Jpeg.h"
#include "tjpgd.c"

/* The pinned LVGL fork emits B,G,R before its JD_FORMAT=1 packing step.
 * Therefore its native words are BGR565, unlike upstream ChaN RGB565.
 * Convert exactly once at the decoder boundary, for photos AND live art.
 * EVE and callers continue to receive little-endian RGB565. */
void PhotoJpeg_CopyRGB565(void *destination,const void *source,uint32_t pixels)
{
    uint16_t *out=destination;const uint16_t *in=source;
    while(pixels--){uint16_t p=*in++;*out++=(p&0x07e0U)|((p&0x001fU)<<11)|((p&0xf800U)>>11);}
}
