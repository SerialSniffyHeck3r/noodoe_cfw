#include "Page_Transition.h"
uint32_t PageTransition_Ease(uint32_t dt)
{
    if(dt>=PAGE_TRANSITION_MS)return 1024U;
    uint32_t t=dt*1024U/PAGE_TRANSITION_MS;
    return (t*t*(3072U-2U*t))>>20;
}
void PageTransition_Project(const PageTransitionFrame *f,PageTransitionAxis axis,int32_t direction,PageTransitionPose *p)
{
    if(!f||!p)return;
    int32_t sign=direction<0?-1:1;
    *p=(PageTransitionPose){0};
    if(axis==PAGE_AXIS_VERTICAL){p->outgoing_y=f->outgoing_x*sign;p->incoming_y=f->incoming_x*sign;}
    else{p->outgoing_x=f->outgoing_x*sign;p->incoming_x=f->incoming_x*sign;}
}
uint32_t PageTransition_Request(PageTransition *s,uint32_t key,uint32_t now)
{
    if(!s)return 0;
    s->pending=key;
    if(!s->initialized){s->initialized=1;s->current=s->target=key;return 2;}
    if(s->active||key==s->current)return 0;
    s->target=key;s->start_ms=now;s->active=1;return 1;
}
/* Cubic smoothstep has zero end velocity. Alpha is applied to individual
 * EVE primitives, never LVGL's unsupported offscreen opacity layers. */
uint32_t PageTransition_Step(PageTransition *s,uint32_t now,PageTransitionFrame *f)
{
    if(!s||!f)return 0;
    uint32_t dt=now-s->start_ms;
    if(!s->active){*f=(PageTransitionFrame){0,0,255,0};return 0;}
    if(dt>=PAGE_TRANSITION_MS){s->active=0;s->current=s->target;*f=(PageTransitionFrame){-PAGE_TRANSITION_DISTANCE,0,0,255};return 1;}
    uint32_t q=PageTransition_Ease(dt);
    uint32_t a=q*255U/1024U;
    *f=(PageTransitionFrame){-(int32_t)(q*PAGE_TRANSITION_DISTANCE/1024U),
        (int32_t)((1024U-q)*PAGE_TRANSITION_DISTANCE/1024U),255U-a,a};return 0;
}
