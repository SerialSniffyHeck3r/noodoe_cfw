#include "Settings_View.h"
#include "Graphics_SubpixelArc.h"
#include "SpeedHome_Layout.h"
/* Arc width is exactly half of the18px speed ring. Q1024 positions animate
 * continuously; the selected segment and its rounded ends stay in the track. */
void SettingsPosition_Draw(lv_layer_t *layer,uint32_t category_q,uint32_t row_q,uint32_t count,uint32_t alpha,uint32_t category_visible)
{
    if(!count)return;
    const uint32_t starts[]={25500,33000},spans[]={3000,6000},counts[]={7,count};
    uint32_t positions[]={category_q,row_q};
    for(uint32_t i=category_visible?0:1;i<2;++i){
        Graphics_DrawSubpixelArcOpacity(layer,240,240,240,9,starts[i],starts[i]+spans[i],0x344049,alpha);
        uint32_t width=spans[i]/counts[i],offset=positions[i]*width/1024;
        if(offset>spans[i]-width)offset=spans[i]-width;
        Graphics_DrawSubpixelArcOpacity(layer,240,240,240,9,starts[i]+offset,starts[i]+offset+width,0xFFFFFF,alpha);
    }
}
/* One elapsed timeline. Further motion never changes its origin; the App
 * handles cancellation and navigation, this function only draws a fraction. */
void SettingsMotion_Draw(lv_layer_t *layer,uint32_t elapsed,uint32_t alpha)
{
    if(elapsed>30000)elapsed=30000;
    Graphics_DrawSubpixelArcOpacity(layer,240,240,240,SPEED_HOME_RING_STROKE,27000,63000,0x344049,alpha);
    if(elapsed)Graphics_DrawSubpixelArcOpacity(layer,240,240,240,SPEED_HOME_RING_STROKE,27000,27000+elapsed*36000U/30000U,0xFFD166,alpha);
}
