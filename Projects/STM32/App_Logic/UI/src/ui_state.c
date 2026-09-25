#include "ui_state_internal.h"

/* No heap/clock/peripheral side effects. Every requested interval is <2^31ms;
 * wrap-safe unsigned subtraction is used by the power/warning submachines. */
UiConfig Ui_DefaultConfig(void)
{
    UiConfig c={0};
    c.card_mask=UI_CARD_MASK;
    c.footer_mask=(1U<<UI_FOOTER_COUNT)-1U;
    c.welcome_ms=2000U;c.summary_ms=5000U;c.standby_ms=3600000U;
    c.bt_retention_ms=3600000U;c.off_stage_mask=UI_OFF_STAGE_ALL;
    c.preferred_card=UI_BLANK;c.preferred_footer=UI_ODO;
    return c;
}

/* UI navigation changes invalidate a held press's old context. Release still
 * clears it, but that press cannot operate the newly opened page. */
void Ui_ContextChanged(UiState *s)
{ ++s->context_token; s->consumed_buttons|=s->pressed_buttons; ++s->revision; }

/* Bounded outbox, never synchronous I/O. Overflow is visible and stops normal
 * navigation; silently losing SAVE or DISCONNECT would be a power-state bug. */
uint32_t Ui_Emit(UiState *s,uint32_t kind,uint32_t arg,uint32_t value)
{
    if(s->effect_count==UI_EFFECT_CAPACITY){
        ++s->effect_overflows;s->last_error=1U;s->power=UI_FAULT;
        s->dashboard.remote_active=0U;Ui_ContextChanged(s);return 0U;
    }
    uint32_t id=++s->next_id;if(!id)id=++s->next_id;
    if(kind==UI_FX_SAVE)s->save_since=s->now_ms;
    else if(kind==UI_FX_POWER_OFF)s->off_since=s->now_ms;
    else if(kind==UI_FX_SETTING||kind==UI_FX_PAIR||kind==UI_FX_MEDIA||
        kind==UI_FX_RESET_TRIP||kind==UI_FX_MAINT_RESET||kind==UI_FX_OPEN_EDITOR)s->pending_since=s->now_ms;
    UiEffect *f=&s->effects[(s->effect_head+s->effect_count)%UI_EFFECT_CAPACITY];
    *f=(UiEffect){kind,arg,value,id,s->epoch};++s->effect_count;return id;
}

/* Only the owner drains the outbox. The adapter must check epoch before
 * beginning an effect, and return that same epoch in EFFECT_DONE. */
uint32_t Ui_TakeEffect(UiState *s,UiEffect *out)
{
    if(!s||!out||!s->effect_count)return 0U;
    *out=s->effects[s->effect_head];s->effect_head=(s->effect_head+1U)%UI_EFFECT_CAPACITY;
    --s->effect_count;return 1U;
}

/* Card policy is shared by initialization, cycling and disconnect fallback.
 * Keep BLANK available as a safe fallback even for an OBD-only preference. */
uint32_t Ui_AvailableCards(const UiState *s)
{
    if(!s)return 0U;
    uint32_t mask=s->config.card_mask&UI_CARD_MASK;
    /* Removed-card preferences fall back without renumbering saved IDs. */
    return mask?mask:UI_BIT(UI_BLANK);
}
uint32_t Ui_CardAvailable(const UiState *s,uint32_t card)
{return card<UI_CARD_COUNT && (Ui_AvailableCards(s)&UI_BIT(card))!=0U;}

/* Preferences are sanitized against current facts. Connecting OBD later never
 * switches the user's card; losing it returns only the center to TRIP/BLANK. */
void Ui_Init(UiState *s,const UiConfig *config,uint32_t now)
{
    if(!s)return;
    UiConfig c=config?*config:Ui_DefaultConfig();memset(s,0,sizeof(*s));
    c.card_mask&=UI_CARD_MASK;if(!c.card_mask)c.card_mask=UI_BIT(UI_BLANK);
    c.footer_mask=(c.footer_mask&((1U<<UI_FOOTER_COUNT)-1U))|UI_BIT(UI_ODO);
    if(c.welcome_ms>10000U)c.welcome_ms=2000U;
    if(c.summary_ms>30000U)c.summary_ms=5000U;
    if(c.standby_ms>86400000U)c.standby_ms=3600000U;
    if(c.bt_retention_ms>86400000U)c.bt_retention_ms=3600000U;
    c.off_stage_mask&=UI_OFF_STAGE_ALL;if(!c.off_stage_mask)c.off_stage_mask=UI_OFF_STAGE_DISPLAY;
    s->config=c;s->entered_ms=s->now_ms=now;s->power=UI_BOOT;
    s->reserve_active=!!c.reserve_active;
    UiDashboard_Init(&s->dashboard,Ui_AvailableCards(s),c.footer_mask,c.preferred_card,c.preferred_footer,s->reserve_active);
    s->blocked_buttons=c.boot_held_mask&7U;s->context_token=1U;
}

/* Retired compatibility API: SettingsUI/AppSettings own editing and commit.
 * Do not start an invisible second editor or mutate any caller state. */
uint32_t Ui_BeginEdit(UiState *s,uint32_t key,uint32_t value,uint32_t min,uint32_t max,uint32_t step)
{
    (void)s;(void)key;(void)value;(void)min;(void)max;(void)step;return 0U;
}

/* Total event ordering: explicit fault and ignition first, then connection and
 * asynchronous completion, then warnings, finally input. Warning/state clocks
 * use event time; no timer callback mutates the UI from another task. */
void Ui_Dispatch(UiState *s,const UiEvent *e)
{
    if(!s||!e||e->kind>UI_EVT_DISPLAY_READY)return;
    s->now_ms=e->now_ms;
    ++s->revision;
    if(e->kind==UI_EVT_FAULT){s->last_error=e->a;s->power=UI_FAULT;s->dashboard.remote_active=0U;Ui_ContextChanged(s);return;}
    if(s->power==UI_FAULT&&e->kind!=UI_EVT_IGN){
        if(e->kind==UI_EVT_EFFECT_DONE)++s->ignored_completions;
        return;
    }
    if(e->kind==UI_EVT_SPEED){
        uint32_t was_locked=!s->speed_valid||s->speed_kph>50U;
        s->speed_valid=!!e->a&&e->b<=400U;s->speed_kph=e->b;
        if((!s->speed_valid||s->speed_kph>50U)&&s->dashboard.card==UI_NOTIFICATIONS){
            s->notification_reply=0;s->dashboard.selection=0U;if(!was_locked)Ui_ContextChanged(s);
        }
    }
    if(e->kind==UI_EVT_PHONE_COUNT){s->notification_count=e->a;
        if(s->dashboard.card==UI_NOTIFICATIONS&&s->dashboard.selection>=e->a)s->dashboard.selection=0;}
    Ui_PowerEvent(s,e);
    Ui_WarningEvent(s,e);
    if(e->kind==UI_EVT_BUTTON)Ui_InputEvent(s,e);
}
