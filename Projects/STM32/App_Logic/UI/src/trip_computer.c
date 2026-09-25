#include "Trip_Computer.h"
#include <string.h>
void TripComputer_Init(TripComputer *s)
{if(s){memset(s,0,sizeof(*s));s->stop_speed_kph=TRIP_STOP_SPEED_DEFAULT_KPH;s->distance_q16=65536;}}
uint32_t TripComputer_SetStopSpeed(TripComputer *s,uint32_t kph)
{if(!s||kph>TRIP_STOP_SPEED_MAX_KPH)return 0;s->stop_speed_kph=kph;return 1;}
uint32_t TripComputer_Reset(TripComputer *s,uint32_t n)
{if(!s||n>TRIP_B)return 0;memset(&s->records[n],0,sizeof(TripRecord));s->records[n].valid=1;return 1;}

/* Rectangular integration uses the preceding fresh sample. Integer residue
 * preserves sub-mm distance across30fps ticks, without float or cumulative
 * truncation. An IGN/date edge does not carry distance into a different day. */
void TripComputer_Tick(TripComputer *s,uint32_t now,uint32_t ign,uint32_t valid,uint32_t speed,uint32_t date)
{
    if(!s)return;
    uint32_t dt=now-s->last_ms,day_change=date&&date!=s->date;
    if(day_change){memset(&s->records[TRIP_TODAY],0,sizeof(TripRecord));s->records[TRIP_TODAY].valid=1;s->date=date;}
    valid=valid&&speed<=400U;
    uint32_t measured=s->have_tick&&dt<=1500U&&s->last_valid&&valid;
    uint32_t mm=0;
    /* Fresh samples cap speed at400 and dt at1500ms: numerator<=3000017.
     * Only cumulative totals and the Q16 product need64-bit arithmetic. */
    if(measured&&s->last_ign&&ign){uint32_t n=s->last_speed*dt*5U+s->remainder;mm=n/18U;s->remainder=n%18U;}
    if(!measured||!s->last_ign||!ign)s->odo_clean=0;
    s->odo_raw_mm+=mm;
    uint64_t scaled=(uint64_t)mm*(s->distance_q16?s->distance_q16:65536U)+s->scale_remainder;
    mm=scaled>>16;s->scale_remainder=(uint32_t)scaled&65535U;
    for(uint32_t i=0;i<TRIP_COUNT;++i){
        TripRecord *r=&s->records[i];
        if(i==TRIP_REFUEL&&!r->valid)continue; /* No fabricated previous fill. */
        if(i==TRIP_TODAY&&(!date||day_change))continue;
        if(ign&&valid){r->valid=1;if(speed>r->max_kph)r->max_kph=speed;}
        if(!s->have_tick||!s->last_ign||!ign)continue;
        if(measured){r->distance_mm+=mm;if(s->last_stopped)r->stopped_ms+=dt;else r->moving_ms+=dt;}
        else{r->unknown_ms+=dt;r->partial=1;}
    }
    s->have_tick=1;s->last_ms=now;s->last_speed=speed;s->last_valid=valid;s->last_ign=!!ign;
    /* Match the preceding-sample distance integration. A setting changed now
     * takes effect from now, not across the interval that just elapsed. */
    s->last_stopped=s->stop_speed_kph?speed<s->stop_speed_kph:speed==0U;
}

/* Accept only continuous samples. A gradual1->2->3 rise is compared to1
 * until3 remains stable, so a real fill is not lost to intermediary packets. */
uint32_t TripComputer_Fuel(TripComputer *s,uint32_t now,uint32_t valid,uint32_t bars)
{
    if(!s)return 0;
    if(!valid||bars>5U){s->fuel_pending=0;return 0;}
    if((uint32_t)(now-s->fuel_last_ms)>1500U)s->fuel_pending=0;
    s->fuel_last_ms=now;
    if(!s->fuel_pending||s->fuel_candidate!=bars){s->fuel_candidate=bars;s->fuel_since=now;s->fuel_pending=1;return 0;}
    if((uint32_t)(now-s->fuel_since)<TRIP_REFUEL_DEBOUNCE_MS)return 0;
    if(!s->fuel_known){s->fuel_known=1;s->fuel_base=bars;return 0;}
    if(bars>=s->fuel_base+2U){
        memset(&s->records[TRIP_REFUEL],0,sizeof(TripRecord));s->records[TRIP_REFUEL].valid=1;
        ++s->refuel_count;s->fuel_base=bars;return 1;
    }
    if(bars<s->fuel_base)s->fuel_base=bars;
    return 0;
}

/* Integer ODO is authoritative for complete kilometre intervals, but cannot
 * reveal the fractional distance at first contact. Skip that first partial
 * interval, gaps, reversal and implausible jumps. No learned value rewrites
 * historical trips or modifies the displayed UART speed. */
void TripComputer_Odometer(TripComputer *s,uint32_t valid,uint32_t odo)
{
    if(!s)return;
    if(!valid||!s->last_ign){s->odo_anchor=0;s->odo_clean=0;return;}
    if(!s->odo_anchor){s->odo_last=odo;s->odo_anchor=1;s->odo_raw_mm=0;return;}
    if(odo==s->odo_last)return;
    if(s->odo_anchor==2&&s->odo_clean&&odo==s->odo_last+1U&&
        s->odo_raw_mm>=800000U&&s->odo_raw_mm<=1250000U)
        s->distance_q16=(uint32_t)(65536000000ULL/s->odo_raw_mm);
    s->odo_anchor=odo==s->odo_last+1U?2:1;s->odo_last=odo;
    s->odo_raw_mm=0;s->odo_clean=1;
}
