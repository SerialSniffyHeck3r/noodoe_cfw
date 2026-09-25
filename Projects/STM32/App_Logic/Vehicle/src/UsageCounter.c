#include "UsageCounter.h"
/* Unsigned tick subtraction handles the49.7day wrap when regularly polled.
 * The64bit accumulated duration saturates rather than wrapping to zero. */
void UsageCounter_Sample(UsageCounter *c,uint32_t now,uint32_t valid,uint32_t on)
{
    if(!c)return;
    uint32_t delta=now-c->last_ms;
    if(c->primed){
        if(delta>1000U)++c->gaps;
        else if(c->known&&c->on)c->on_ms=UINT64_MAX-c->on_ms<delta?UINT64_MAX:c->on_ms+delta;
    }
    c->last_ms=now;c->known=!!valid;c->on=!!on;c->primed=1U;
}
