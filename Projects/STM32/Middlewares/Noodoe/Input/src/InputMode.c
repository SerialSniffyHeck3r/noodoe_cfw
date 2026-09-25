#include "InputMode.h"
#include "BSP_Buttons.h"
InputModeState g_input_mode;
void InputMode_Init(uint32_t held)
{g_input_mode=(InputModeState){.allowed=1,.inhibited=held&7U,.pressed=held&7U};}
uint32_t InputMode_Sample(uint32_t high,uint32_t ign,uint32_t held,uint32_t now)
{
    InputModeState *s=&g_input_mode;s->raw_high=!!high;s->ign_on=!!ign;
    /* Mechanical selector chatter must not toggle either the badge or the
     * permission gate. Both edges require100ms continuously stable input.
     * Seed once at boot; elapsed subtraction also works across tick wrap. */
    if(!s->initialized){s->stable_high=s->candidate=!!high;s->since=now;s->initialized=1;}
    if(s->candidate!=!!high){s->candidate=!!high;s->since=now;}
    if(now-s->since>=100U)s->stable_high=s->candidate;
    s->allowed=!ign||s->stable_high;
    if(s->allowed)return 0;
    uint32_t cancel=s->accepted;
    s->inhibited|=(held|s->pressed)&7U;s->accepted=0;
    return cancel;
}
uint32_t InputMode_Event(uint32_t button,uint32_t event)
{
    if(button>=3)return INPUT_MODE_DROP;
    InputModeState *s=&g_input_mode;uint32_t bit=1U<<button;
    if(event==BSP_BUTTON_EVENT_PRESS){
        if(s->pressed&bit)return INPUT_MODE_DROP;
        s->pressed|=bit;
        if(!s->allowed){s->inhibited|=bit;++s->blocked_presses;return INPUT_MODE_NOTIFY;}
        if(s->inhibited&bit)return INPUT_MODE_DROP;
        s->accepted|=bit;return INPUT_MODE_ACCEPT;
    }
    uint32_t accepted=!!(s->accepted&bit)&&s->allowed&&!(s->inhibited&bit);
    if(event==BSP_BUTTON_EVENT_RELEASE){s->pressed&=~bit;s->inhibited&=~bit;s->accepted&=~bit;}
    return accepted?INPUT_MODE_ACCEPT:INPUT_MODE_DROP;
}
