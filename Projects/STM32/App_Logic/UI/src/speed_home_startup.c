#include "SpeedHome_Startup.h"
#include "Page_Transition.h"
#include <string.h>

void SpeedHomeStartup_Init(SpeedHomeStartup *s)
{
    memset(s,0,sizeof(*s));s->magic=0x53575031U;s->version=1;s->epoch=UINT32_MAX;
}
void SpeedHomeStartup_Ignition(SpeedHomeStartup *s,uint32_t epoch,uint32_t on,uint32_t cold)
{
    if(s->epoch==epoch)return;
    s->epoch=epoch;
    if(!on){
        if(s->phase!=SPEED_START_LIVE)++s->cancelled;
        s->phase=SPEED_START_LIVE;return;
    }
    if(cold){
        s->phase=SPEED_START_ARMED;s->value=0;s->cutoff_ms=0;
        s->accepted_telemetry_ms=0;s->up_ms=s->down_ms=0;++s->starts;
    }
}
uint32_t SpeedHomeStartup_WaitsForFrame(const SpeedHomeStartup *s)
{
    return s->phase==SPEED_START_HOME_FRAME||s->phase==SPEED_START_PEAK_FRAME||
        s->phase==SPEED_START_ZERO_FRAME;
}
/* Reuse the global cubic timing curve. Elapsed is bounded before multiplying,
 * avoiding overflow and keeping each leg monotonic with zero endpoint speed. */
static uint32_t Sweep(uint32_t elapsed)
{
    if(elapsed>=SPEED_HOME_STARTUP_LEG_MS)return 10000U;
    return 10000U*PageTransition_Ease(elapsed*PAGE_TRANSITION_MS/SPEED_HOME_STARTUP_LEG_MS)/1024U;
}
uint32_t SpeedHomeStartup_Step(SpeedHomeStartup *s,uint32_t now,uint32_t home_ready,
    uint32_t frame,uint32_t presented,uint32_t speed_valid,uint32_t telemetry_ms)
{
    switch(s->phase){
    case SPEED_START_LIVE:return 0;
    case SPEED_START_ARMED:
        s->value=0;
        if(home_ready){s->frame_before=frame;s->phase=SPEED_START_HOME_FRAME;}
        break;
    case SPEED_START_HOME_FRAME:
        if(presented&&frame!=s->frame_before){s->start_ms=s->up_ms=now;s->phase=SPEED_START_UP;}
        break;
    case SPEED_START_UP:
        s->value=Sweep(now-s->start_ms);
        if(s->value==10000U){s->phase=SPEED_START_PEAK_FRAME;s->frame_before=frame;}
        break;
    case SPEED_START_PEAK_FRAME:
        if(presented&&frame!=s->frame_before){s->start_ms=s->down_ms=now;s->phase=SPEED_START_DOWN;}
        break;
    case SPEED_START_DOWN:
        s->value=10000U-Sweep(now-s->start_ms);
        if(!s->value){s->phase=SPEED_START_ZERO_FRAME;s->frame_before=frame;}
        break;
    case SPEED_START_ZERO_FRAME:
        if(presented&&frame!=s->frame_before){
            s->cutoff_ms=now;s->phase=SPEED_START_PACKET;++s->completed;
        }
        break;
    case SPEED_START_PACKET:
        /* sequence covers non-telemetry frames too. Use telemetry_ms instead;
         * signed modular difference also accepts a fresh sample across wrap. */
        if(speed_valid&&(int32_t)(telemetry_ms-s->cutoff_ms)>0){
            s->accepted_telemetry_ms=telemetry_ms;s->phase=SPEED_START_LIVE;return 0;
        }
        break;
    default:s->value=0;s->phase=SPEED_START_ARMED;break;
    }
    return 1;
}
