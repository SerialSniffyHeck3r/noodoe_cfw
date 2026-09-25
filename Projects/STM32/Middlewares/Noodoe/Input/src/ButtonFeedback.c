#include "ButtonFeedback.h"
#include "BSP_Buttons.h"
#include <string.h>
ButtonFeedbackState g_button_feedback;
static void Observe(const ButtonEvent *event,void *context)
{(void)context;ButtonFeedback_Handle(event);}
/* One middleware listener observes the same BSP stream as the app. It never
 * emits actions or touches UI/BT. Init follows ButtonEvents_Init on its owner. */
void ButtonFeedback_Init(uint32_t held)
{
    memset(&g_button_feedback,0,sizeof(g_button_feedback));
    g_button_feedback.magic=0x42464631U;g_button_feedback.version=2;
    g_button_feedback.blocked=held&7U;(void)ButtonEvents_Subscribe(Observe,NULL);
}
void ButtonFeedback_SetContext(uint32_t scope,uint32_t generation,uint32_t enabled)
{
    ButtonFeedbackState *s=&g_button_feedback;
    if(s->enabled&&!enabled){
        /* A disabled scene cannot retain a held key/released icon forever.
         * Suppress the physical hold until release instead of inventing one. */
        for(uint32_t i=0;i<3;++i){if(s->keys[i].pressed)s->blocked|=1U<<i;
            s->keys[i].pressed=s->keys[i].long_press=s->keys[i].release_serial=0;}
        s->visibility_alpha=s->visibility_from=s->visibility_target=0;
        s->activity_serial=0;s->visibility_initialized=1;
    }
    s->scope=scope;s->generation=generation;s->enabled=!!enabled;
}
/* PRESS arms once; release duration remains authoritative even if a threshold
 * notification was skipped. Duplicate SHORT/LONG after release cannot pulse.
 * A boot-held key is excluded until its first physical release, like the UI. */
void ButtonFeedback_Handle(const ButtonEvent *e)
{
    if(!e||e->button>=3)return;
    ButtonFeedbackState *s=&g_button_feedback;ButtonFeedbackKey *k=&s->keys[e->button];uint32_t bit=1U<<e->button;
    if(s->blocked&bit){if(e->type==BSP_BUTTON_EVENT_RELEASE)s->blocked&=~bit;return;}
    /* Physical activity is independent of page action availability. Ignore
     * duplicate packets and the broken/boot-held key until its release. */
    if((e->type==BSP_BUTTON_EVENT_PRESS&&!k->pressed)||
       (e->type==BSP_BUTTON_EVENT_RELEASE&&k->pressed)){
        s->activity_ms=e->timestamp_ms;if(!++s->activity_serial)++s->activity_serial;
    }
    if(e->type==BSP_BUTTON_EVENT_PRESS){
        if(!k->pressed){k->pressed=1;k->long_press=0;k->press_scope=s->scope;k->press_generation=s->enabled?s->generation:~s->generation;}
    }else if(e->type==BSP_BUTTON_EVENT_RELEASE){
        if(k->pressed&&s->enabled&&k->press_scope==s->scope&&k->press_generation==s->generation){
            k->released_ms=e->timestamp_ms;k->release_long=e->duration_ms>=BSP_BUTTONS_LONG_PRESS_MS;
            k->release_scope=s->scope;if(!++k->release_serial)++k->release_serial;
        }k->pressed=k->long_press=0;
    }else if(k->pressed&&(e->type==BSP_BUTTON_EVENT_LONG_PRESS||e->type==BSP_BUTTON_EVENT_VERY_LONG_PRESS))k->long_press=1;
}
/* Phase0 white,1 short/blue,2 long/red. Releasing immediately restores only
 * the physical-key symbol; a matching action may retain its own1s pulse. */
uint32_t ButtonFeedback_KeyPhase(uint32_t button,uint32_t scope)
{
    if(button>=3)return 0;
    ButtonFeedbackKey *k=&g_button_feedback.keys[button];
    return k->pressed&&g_button_feedback.enabled&&k->press_scope==scope&&k->press_generation==g_button_feedback.generation?1U+k->long_press:0;
}
uint32_t ButtonFeedback_ActionPhase(uint32_t button,uint32_t scope,uint32_t hold,uint32_t now)
{
    if(button>=3)return 0;
    ButtonFeedbackKey *k=&g_button_feedback.keys[button];
    return k->release_serial&&k->release_scope==scope&&k->release_long==!!hold&&now-k->released_ms<BUTTON_FEEDBACK_RELEASE_MS?1U+k->release_long:0;
}

/* Cubic slow-fast-slow opacity, including continuous reversal mid-fade.
 * Held input stays visible; inactivity starts at its final release. Tick
 * subtraction is unsigned so the five-second window survives wraparound. */
uint32_t ButtonFeedback_Visibility(uint32_t now)
{
    ButtonFeedbackState *s=&g_button_feedback;
    if(!s->enabled||(!s->activity_serial&&s->visibility_initialized))return 0;
    if(!s->visibility_initialized){
        s->visibility_initialized=1;
        if(!s->activity_serial){s->activity_ms=now;s->activity_serial=1;}
    }
    uint32_t age=now-s->visibility_start,q=age>=BUTTON_HINT_FADE_MS?1024:age*1024/BUTTON_HINT_FADE_MS;
    uint32_t ease=q*q*(3072-2*q)/1048576U;
    s->visibility_alpha=s->visibility_from+((int32_t)s->visibility_target-(int32_t)s->visibility_from)*(int32_t)ease/1024;
    uint32_t held=s->keys[0].pressed|s->keys[1].pressed|s->keys[2].pressed;
    uint32_t target=held||now-s->activity_ms<BUTTON_HINT_IDLE_MS?255:0;
    if(target!=s->visibility_target){s->visibility_from=s->visibility_alpha;s->visibility_target=target;s->visibility_start=now;}
    return s->visibility_alpha;
}
