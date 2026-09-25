#include "ui_state_internal.h"
#include "Ui_OffStages.h"
#define IGN_RING_TRANSITION_MS 400U
/* Physical IGN is the only power epoch source. Preserve the selected page and
 * trip/maintenance domains while cancelling stale held inputs and effects. */
static void Ignition(UiState *s,const UiEvent *e)
{
    uint32_t on=!!e->a,was_on=s->ign_known&&s->ign_on;
    if(s->ign_known&&s->ign_on==on)return;
    /* A warm OFF/summary reversal never replays Welcome. Cold boot or an
     * acknowledged sleeping panel may show it once while returning to ON. */
    s->welcome_active=on&&(!s->ign_known||s->display_asleep);
    s->startup_sweep=on&&(s->welcome_active||Ui_OffStages_IsOff(s->power));
    /* An unlit but scanning standby panel needs a fresh ON frame too, but
     * does not qualify as a hardware screen-off Welcome transition. */
    s->display_wait=s->welcome_active||(on&&s->power==IGN_OFF_AWAKE);
    /* Only first boot chooses Home. Every retained IGN return keeps the riding
     * card, its item and footer; the ring self-test must not select a page.
     * Full Settings restores its saved riding context in SettingsUI_Process. */
    if(on&&!s->ign_known){s->dashboard.card=UI_BLANK;s->dashboard.selection=0;s->dashboard.item_direction=0;}
    s->ign_known=1;s->ign_on=on;++s->epoch;
    s->effect_head=s->effect_count=0;s->pending_id=s->save_id=s->off_id=0;
    s->menu=UI_MENU_NONE;s->modal=UI_MODAL_NONE;s->dashboard.remote_active=0;
    s->warning=UI_WARN_NONE;s->warning_phase=UI_WARN_IDLE;s->pending_warnings=0;
    s->entered_ms=e->now_ms;s->maintenance_shown=0;s->last_error=0;Ui_ContextChanged(s);
    if(on){s->power=IGN_STARTING;
        if(!s->session_open){s->session_open=1;Ui_Emit(s,UI_FX_SESSION_START,0,0);}}
    else{s->off_ms=e->now_ms;s->power=was_on?IGN_STOPPING:Ui_OffStages_First(&s->config);
        s->off_phase=UI_OFF_DELAY;s->off_phase_ms=e->now_ms;}
    Ui_Emit(s,UI_FX_SCREEN,s->power,on||was_on?25U:0U);
}
/* Phone links and buttons never wake the glass or extend either OFF timer. */
static void Links(UiState *s,const UiEvent *e)
{
    uint32_t before=s->links;s->links=e->a&UI_LINK_SUPPORTED;
    if((before&UI_LINK_PHONES)!=(s->links&UI_LINK_PHONES)&&s->dashboard.remote_active){
        s->dashboard.remote_active=0;Ui_ContextChanged(s);}
}
/* Preserve ID/epoch matching for ordinary asynchronous UI commands. Sleep is
 * not a power-cut command, and no SAVE success is fabricated. */
static void Complete(UiState *s,const UiEvent *e)
{
    if(e->b!=s->epoch){++s->ignored_completions;return;}
    if(e->a&&e->a==s->pending_id){s->pending_id=0;s->last_error=e->c;
        if(!e->c){s->modal=UI_MODAL_NONE;Ui_ContextChanged(s);}return;}
    ++s->ignored_completions;
}
/* The uninterrupted1s delay is the shutdown commit point. End the ride once,
 * then start the ring exit, background fade and summary together. Scanout
 * completion is a rendering concern and must not postpone the session end. */
static void Tick(UiState *s,uint32_t now)
{
    if(s->pending_id&&now-s->pending_since>=5000U){s->pending_id=0;s->last_error=9;}
    uint32_t next=s->power;
    if(s->power==IGN_STARTING&&!s->display_wait&&now-s->entered_ms>=(s->welcome_active?s->config.welcome_ms:0U)+IGN_RING_TRANSITION_MS){
        next=IGN_ON;s->pending_warnings=s->maintenance_due<<UI_WARN_OIL;
        /* Debounced FuelPolicy owns the per-session fuel reminder. */
    }else if(s->power==IGN_STOPPING){
        uint32_t elapsed=now-s->off_phase_ms;
        if(s->off_phase==UI_OFF_DELAY&&elapsed>=UI_IGN_OFF_DELAY_MS){
            s->off_phase=UI_OFF_SUMMARY;s->off_phase_ms=now;
            if(s->session_open){s->session_open=0;Ui_Emit(s,UI_FX_SESSION_END,0,0);}
        }else if(s->off_phase==UI_OFF_SUMMARY&&elapsed>=s->config.summary_ms)next=Ui_OffStages_First(&s->config);
    }
    else if(Ui_OffStages_IsOff(s->power))next=Ui_OffStages_Next(&s->config,s->power,now-s->entered_ms);
    if(next!=s->power){s->power=next;s->entered_ms=now;Ui_ContextChanged(s);Ui_Emit(s,UI_FX_SCREEN,next,next==IGN_ON?25U:0U);}
}
void Ui_PowerEvent(UiState *s,const UiEvent *e)
{
    switch(e->kind){case UI_EVT_IGN:Ignition(s,e);break;case UI_EVT_LINKS:Links(s,e);break;
    case UI_EVT_DISPLAY_READY:
        /* A stale frame from an earlier IGN epoch cannot start this wake. */
        if(s->power==IGN_STARTING&&s->display_wait&&e->a==s->epoch){s->display_wait=0;s->entered_ms=e->now_ms;}
        break;
    case UI_EVT_RING_HIDDEN:break; /* Legacy notification cannot commit a ride. */
    case UI_EVT_EFFECT_DONE:Complete(s,e);break;case UI_EVT_TICK:Tick(s,e->now_ms);break;default:break;}
}
