#include "Ignition_Session.h"
/* Wrap-safe milliseconds, one UI owner. Confirmed end clears only this ride,
 * never odometer/Trip A/B/maintenance. Integrate valid speed <=1s gaps. */
void IgnitionSession_Tick(IgnitionSession *s,uint32_t now,uint32_t on,uint32_t valid,uint32_t kph)
{
    if(!s)return;
    on=!!on;valid=!!valid&&kph<=400U;
    if(on&&!s->on){s->on=1;s->last_ms=now;s->valid=0;}
    if(s->on){uint32_t dt=now-s->last_ms;s->ride_ms+=dt;
        if(dt<=1000U&&s->valid&&valid){
            s->speed_ms2+=(uint64_t)(s->speed+kph)*dt;s->known_ms+=dt;
            /* Carry sub-millimetres across samples; frequent UI ticks must
             * not systematically undercount low-speed riding distance. */
            uint64_t value=(uint64_t)(s->speed+kph)*dt*5U*(s->distance_scale_q16?s->distance_scale_q16:65536U)+s->distance_fraction;
            s->distance_mm+=value/(36U*65536U);s->distance_fraction=value%(36U*65536U);
        }
        else if(dt)s->partial=1;
    }
    if(s->on&&valid){s->have_speed=1;if(kph>s->peak_kph)s->peak_kph=kph;}
    if(s->on&&!on){
        s->finished_distance_mm=s->distance_mm;s->finished_ride_ms=s->ride_ms;
        s->finished_partial=s->partial;++s->completed;
        s->distance_mm=s->ride_ms=0;s->partial=s->distance_fraction=0;
        s->speed_ms2=s->known_ms=0;s->peak_kph=s->have_speed=0;
    }
    s->on=on;s->last_ms=now;s->speed=kph;s->valid=valid;
}
