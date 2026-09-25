#include "Fuel_Policy.h"
uint32_t FuelPolicy_Step(FuelPolicy *s,uint32_t session,uint32_t now,
 uint32_t valid,uint32_t sample,uint32_t raw,uint32_t reserve)
{
    if(s->session!=session){*s=(FuelPolicy){.session=session};}
    uint32_t kind=raw==0?FUEL_ERROR:raw==0x50?FUEL_CRITICAL:
        raw==0x51?FUEL_LOW:raw>=0x52&&raw<=0x55?FUEL_NORMAL:FUEL_UNKNOWN;
    if(!valid||!session||now-sample>1000U||!kind){s->candidate=0;s->have_sample=0;return 0;}
    if(s->have_sample&&sample==s->last_sample)return 0;
    if(!s->have_sample||sample-s->last_sample>1000U||kind!=s->candidate){s->candidate=kind;s->since=sample;}
    s->last_sample=sample;s->have_sample=1;
    if(sample-s->since<(kind==FUEL_NORMAL?3000U:1000U))return 0;
    s->stable=kind;
    uint32_t result=0;
    if(kind==FUEL_CRITICAL&&!reserve)result|=FUEL_START_RESERVE;
    if(kind==FUEL_NORMAL&&reserve)result|=FUEL_CLEAR_RESERVE;
    if(s->announced!=kind){s->announced=kind;if(kind!=FUEL_NORMAL)result|=kind;}
    return result;
}
