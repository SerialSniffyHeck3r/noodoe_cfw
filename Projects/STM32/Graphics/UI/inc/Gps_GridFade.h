#ifndef GPS_GRID_FADE_H
#define GPS_GRID_FADE_H
#include <stdint.h>
/* Shared projection/render bounds. Widen by12px per side while staying
 * inside the existing336px central viewport. Camera/scale are unchanged. */
#define GPS_PLOT_WIDTH 336
#define GPS_PLOT_HEIGHT 236
/* Grid ink fades using three nested clips in the view. Route clipping shares
 * these bounds but does not change route opacity, wallpaper or the arrow. */
/* Clip the segment, not its endpoints independently: a route wholly outside
 * the window must not become a false line along its edge. EVE's signed15-bit
 * vertices cannot represent the model's +/-30000px long-distance points.
 * Scissoring pixels afterwards cannot repair vertices that already wrapped.
 * M4F divisions avoid overflow from multiplying two large int16 deltas. */
static inline uint32_t GpsLine_Clip(const int16_t line[4],int inset,int16_t out[4])
{
    float enter=0.0f,leave=1.0f;
    const int limits[2]={GPS_PLOT_WIDTH-1-inset,GPS_PLOT_HEIGHT-1-inset};
    for(int axis=0;axis<2;++axis){
        float origin=line[axis],delta=(int)line[axis+2]-line[axis];
        if(delta==0.0f){if(origin<inset||origin>limits[axis])return 0;}
        else{
            float lo=(inset-origin)/delta,hi=(limits[axis]-origin)/delta;
            if(lo>hi){float swap=lo;lo=hi;hi=swap;}
            if(lo>enter)enter=lo;
            if(hi<leave)leave=hi;
            if(enter>leave)return 0;
        }
    }
    for(int i=0;i<4;++i){
        int axis=i&1;
        int value=(int)(line[axis]+((int)line[axis+2]-line[axis])*(i<2?enter:leave));
        out[i]=value<inset?inset:value>limits[axis]?limits[axis]:value;
    }
    return 1; /* An in-window point is valid; the line renderer skips it. */
}
#endif
