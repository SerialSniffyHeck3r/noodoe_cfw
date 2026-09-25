#include "Scalar_Transition.h"
#include "Page_Transition.h"
/* One cubic is shared with page/carousel/scene animations. Normalize elapsed
 * time before evaluating it, keeping all products within32-bit bounds. */
uint32_t ScalarTransition_Value(ScalarTransition *s,uint32_t now)
{
    uint32_t dt=now-s->start_ms;
    if(s->duration_ms){
        if(dt>=s->duration_ms){s->value=s->target;s->duration_ms=0;}
        else{s->value=(uint32_t)((int32_t)s->from+((int32_t)s->target-(int32_t)s->from)*
            (int32_t)PageTransition_Ease(dt*PAGE_TRANSITION_MS/s->duration_ms)/1024);}
    }
    return s->value;
}
void ScalarTransition_Request(ScalarTransition *s,uint32_t target,uint32_t duration,uint32_t now)
{
    (void)ScalarTransition_Value(s,now);
    if(s->target==target)return;
    s->from=s->value;s->target=target;s->start_ms=now;s->duration_ms=duration;
    if(!duration)s->value=target;
}
