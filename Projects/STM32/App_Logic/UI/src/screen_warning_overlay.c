#include "ScreenWarningOverlay.h"
#include <string.h>
#include <stdio.h>
static ScreenWarningState state;
static uint32_t clock_now,blocked;
void ScreenWarningOverlay(uint32_t icon,uint32_t color,uint32_t a,uint32_t b,const char *message)
{
    if(!message||a>30||b>30)return;
    if(state.active&&state.icon==icon&&state.color==color&&!strcmp(state.message,message))return;
    state=(ScreenWarningState){.active=1,.size=432,.y=24,.opacity=255,.icon=icon,.color=color,.started=clock_now,.blink_ms=a*1000U,.text_ms=b*1000U};
    snprintf(state.message,sizeof(state.message),"%s",message);
}
void ScreenWarningOverlay_Close(void){state.active=0;}
void ScreenWarningOverlay_Get(ScreenWarningState *out){if(out)*out=state;}
/* A press consumed by the overlay never leaks RELEASE/SHORT/LONG into the
 * underlying page, including when the text timeout expires while held. */
uint32_t ScreenWarningOverlay_Button(uint32_t button,uint32_t event)
{
    if(button>2)return 0;uint32_t bit=1U<<button;
    /* RELEASE may be followed by SHORT. Retire the quarantine only at the
     * next PRESS, otherwise that trailing SHORT could select a setting. */
    if(blocked&bit){if(event!=1)return 1;blocked&=~bit;}
    if(!state.active)return 0;
    if(event==1){blocked|=bit;if(state.phase==2)state.active=0;}
    return 1;
}
void ScreenWarningOverlay_Process(uint32_t now,uint32_t allowed)
{
    clock_now=now;if(!allowed){state.active=0;return;}if(!state.active)return;
    uint32_t elapsed=now-state.started;
    if(elapsed>=state.blink_ms+400U+state.text_ms){state.active=0;return;}
    state.phase=elapsed<state.blink_ms?0:elapsed<state.blink_ms+400U?1:2;
    state.opacity=state.phase==0?(elapsed%1000U<500U?255:0):255;
    uint32_t t=state.phase==0?0:state.phase==2?1000:(elapsed-state.blink_ms)*1000U/400U;
    uint32_t ease=t*t/1000U*(3000U-2U*t)/1000U;
    state.size=432U-96U*ease/1000U;state.y=24U-18U*ease/1000U;
}
