#include "Phone_Indicators.h"
#include <string.h>
static struct {uint32_t epoch,source,received,count,gps;PhoneIndicatorEntry entries[10];} state;
uint32_t PhoneIndicators_Update(uint32_t epoch,uint32_t source,uint32_t now,uint32_t gps,PhoneIndicatorEntry *in,uint32_t count)
{
    if(!epoch||!source||gps>1||count>10)return 0;
    for(uint32_t i=0;i<count;i++){if(!in[i].id||!in[i].revision||in[i].read>1)return 0;
        for(uint32_t j=0;j<i;j++)if(in[i].id==in[j].id)return 0;}
    if(state.source!=source){memset(&state,0,sizeof(state));state.source=source;}state.epoch=epoch;
    for(uint32_t i=0;i<count;i++)for(uint32_t j=0;j<state.count;j++){
        const PhoneIndicatorEntry *old=&state.entries[j];
        if(old->id==in[i].id&&old->revision==in[i].revision){
            in[i].read|=old->read;
            uint64_t age=(uint64_t)old->age_ms+(uint32_t)(now-state.received);
            if(age>86400000U)age=86400000U;if(in[i].age_ms<age)in[i].age_ms=age;
        }
    }
    memcpy(state.entries,in,count*sizeof(*in));state.count=count;state.gps=gps;state.received=now;return 1;
}
void PhoneIndicators_MarkRead(void){for(uint32_t i=0;i<state.count;i++)state.entries[i].read=1;}
void PhoneIndicators_Colors(uint32_t now,uint32_t connected,uint32_t *bt,uint32_t *gps)
{
    *bt=*gps=0x666666U;if(!connected)return;
    *bt=0xDCE5E9U;
    if(state.epoch&&now-state.received<=3000U&&state.gps)*gps=0xF2F5F7U;
    uint32_t youngest=UINT32_MAX;
    for(uint32_t i=0;i<state.count;i++)if(!state.entries[i].read){
        uint64_t age=(uint64_t)state.entries[i].age_ms+(uint32_t)(now-state.received);
        if(age<youngest)youngest=age;
    }
    if(youngest==UINT32_MAX)return;
    *bt=youngest<600000U?0xFF3030U:0xFFCC33U;
    if(youngest<5000U&&youngest%1000U>=500U)*bt=0xDCE5E9U;
}
