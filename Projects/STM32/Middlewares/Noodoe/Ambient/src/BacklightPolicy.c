#include "BacklightPolicy.h"
uint32_t BacklightPolicy_Target(uint32_t index,int32_t bias)
{
    if(bias<-2)bias=-2;
    if(bias>2)bias=2;
    int32_t step=(int32_t)(index>9?9:index)+bias;
    if(step<0)step=0;
    if(step>9)step=9;
    uint32_t control=(uint32_t)(step+1)*10U;
    return control>=100U?99U:control;
}
/* Caller supplies only current, calibrated LIVE samples. UART overrides and
 * stale/invalid readings deliberately fall back to the manual setting. */
uint32_t BacklightPolicy_Step(BacklightPolicy *p,uint32_t now,uint32_t active,
    uint32_t automatic,uint32_t valid,uint32_t index,int32_t bias,uint32_t manual,uint32_t current)
{
    if(!p)return current;
    p->source_valid=valid&&index<10;
    if(!active){p->tick=p->since=now;return current;}
    if(!automatic){p->tick=p->since=now;return manual>100?100:manual;}
    if(now-p->tick<100U)return current;
    p->tick=now;
    uint32_t target=p->source_valid?BacklightPolicy_Target(index,bias):(manual>99?99:manual);
    p->target=target;
    if(p->candidate!=target){p->candidate=target;p->since=now;}
    if(p->source_valid&&now-p->since<1000U)return current;
    if(current<target)return current+(target-current>2?2:target-current);
    return current-(current-target>2?2:current-target);
}
