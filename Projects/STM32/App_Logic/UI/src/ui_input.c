#include "ui_state_internal.h"

/* Exactly one semantic action per physical press. BSP emits LONG at800ms,
 * VERY_LONG at3000ms, RELEASE and sometimes SHORT; RELEASE duration is the
 * authority. Retired remote mode never intercepts a press.
 * UP/DOWN never require a chord. PRESS captured before navigation is consumed. */
void Ui_InputEvent(UiState *s,const UiEvent *e)
{
    uint32_t b=e->a,t=e->b;if(b>UI_ENTER||t<UI_PRESS||t>UI_VERY_LONG)return;
    uint32_t bit=UI_BIT(b);
    if(t==UI_RELEASE && (s->blocked_buttons&bit)){
        s->blocked_buttons&=~bit;s->pressed_buttons&=~bit;return;
    }
    if(s->blocked_buttons&bit)return;
    if(t==UI_PRESS){
        if(s->pressed_buttons&bit)return;
        s->pressed_buttons|=bit;s->consumed_buttons&=~bit;s->press_ms[b]=e->now_ms;
        s->remote_token[b]=s->context_token;
        if(s->power!=UI_RUNNING||s->warning||s->pending_id)s->consumed_buttons|=bit;
        return;
    }
    if(!(s->pressed_buttons&bit))return;
    /* Manual A/B reset fires at the hold threshold, not on a later release.
     * The context/consumed latch prevents LONG, VERY_LONG and RELEASE from
     * resetting twice or navigating after the asynchronous result arrives. */
    if(t==UI_LONG&&b==UI_ENTER&&e->c>=UI_LONG_MS&&
       s->power==UI_RUNNING&&!s->warning&&!s->menu&&!s->modal&&!s->pending_id&&
       (s->dashboard.card==UI_PHONE_GPS||s->dashboard.card==UI_BLANK||(s->dashboard.card==UI_TRIP&&s->dashboard.selection<2U))&&
       !(s->consumed_buttons&bit)&&s->remote_token[b]==s->context_token){
        s->consumed_buttons|=bit;Ui_Navigate(s,b,1U);return;
    }
    if(t!=UI_RELEASE)return;
    s->pressed_buttons&=~bit;
    if(s->consumed_buttons&bit||s->remote_token[b]!=s->context_token||
       s->power!=UI_RUNNING||s->warning||s->pending_id)return;
    s->consumed_buttons|=bit;
    /* Direction belongs to the accepted input, including cyclic A<->REFUEL.
     * Blocked/duplicate/remote presses above cannot restart a page movement. */
    if(b!=UI_ENTER)s->dashboard.item_direction=b==UI_UP?2U:1U;
    Ui_Navigate(s,b,e->c>=UI_LONG_MS);
}
