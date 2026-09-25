#include "Product_Theme.h"
#include "Scalar_Transition.h"
static ScalarTransition transition;
static uint32_t amount;
/* One clock and one easing curve govern every primitive, including CJK
 * masks, so outgoing page banks cannot retain colors from the old theme. */
void Theme_SetLight(uint32_t light,uint32_t now)
{ScalarTransition_Request(&transition,light?255:0,240,now);}
void Theme_Tick(uint32_t now){amount=ScalarTransition_Value(&transition,now);}
uint32_t Theme_LightAmount(void){return amount;}
uint32_t Theme_Color(uint32_t color)
{
    if(!amount)return color;
    uint32_t r=(color>>16)&255,g=(color>>8)&255,b=color&255;
    uint32_t hi=r>g?r:g;if(b>hi)hi=b;
    uint32_t lo=r<g?r:g;if(b<lo)lo=b;
    uint32_t out=0;
    for(int shift=16;shift>=0;shift-=8){
        uint32_t c=(color>>shift)&255;
        /* Neutral ink/surfaces invert luminance; chromatic status colors
         * retain hue with stronger contrast against a white surface. */
        /* Shell accents have semantic light counterparts: inverting the
         * dark blue ring/green-gray separator would turn them peach/purple. */
        uint32_t light=color==0x183440U?0xD8U:
            color==0x71867CU?((0x7D8C84U>>shift)&255U):
            hi-lo<80U?255U-c:(c*3U)/4U;
        out|=((c*(255U-amount)+light*amount+127U)/255U)<<shift;
    }
    return out;
}
