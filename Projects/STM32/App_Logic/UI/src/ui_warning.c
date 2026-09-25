#include "ui_state_internal.h"

/* Fuel has priority over maintenance. Shared timing is independent of icon,
 * font and view. Domain RESERVE_ENTER means debounce was already satisfied;
 * raw UART bytes must never be delivered as this semantic event. */
void Ui_StartWarning(UiState *s,uint32_t now)
{
    if(s->power!=UI_RUNNING||s->warning||!s->pending_warnings)return;
    for(uint32_t w=UI_WARN_FUEL;w<=UI_WARN_SERV;++w)if(s->pending_warnings&UI_BIT(w)){
        s->pending_warnings&=~UI_BIT(w);s->warning=w;s->warning_phase=UI_WARN_BLINK;s->warning_ms=now;
        if(w>=UI_WARN_OIL)s->maintenance_shown|=UI_BIT(w-UI_WARN_OIL);
        s->dashboard.remote_active=0U;Ui_ContextChanged(s);return;
    }
}

/* Reserve distance starts once at the domain transition, not after animation.
 * A duplicate low-fuel event/reboot reminder never issues another RESET.
 * Footer selection locks at animation end; other navigation remains available. */
void Ui_WarningEvent(UiState *s,const UiEvent *e)
{
    if(e->kind==UI_EVT_RESERVE_ENTER&&!s->reserve_active){
        s->reserve_active=1U;s->dashboard.footer_before_reserve=s->dashboard.footer;
        Ui_Emit(s,UI_FX_RESERVE_START,0,0);s->dashboard.footer=UI_RESV;
        if(s->warning&&s->warning!=UI_WARN_FUEL){
            s->pending_warnings|=UI_BIT(s->warning);s->warning=UI_WARN_NONE;
        }
    }
    if(e->kind==UI_EVT_REFUEL&&s->reserve_active){
        s->reserve_active=0U;s->dashboard.footer=s->dashboard.footer_before_reserve;
        s->pending_warnings&=~UI_BIT(UI_WARN_FUEL);
        if(s->warning==UI_WARN_FUEL){s->warning=UI_WARN_NONE;s->warning_phase=UI_WARN_IDLE;}
        Ui_Emit(s,UI_FX_RESERVE_CLEAR,0,0);Ui_ContextChanged(s);
    }
    if(e->kind==UI_EVT_MAINTENANCE){
        s->maintenance_due=e->a&7U;
        s->pending_warnings&=(UI_BIT(UI_WARN_FUEL)|(s->maintenance_due<<UI_WARN_OIL));
        if(s->power==UI_RUNNING)s->pending_warnings|=(s->maintenance_due&~s->maintenance_shown)<<UI_WARN_OIL;
    }
    if(s->power!=UI_RUNNING)return;
    if(e->kind==UI_EVT_TICK&&s->warning){
        uint32_t elapsed=e->now_ms-s->warning_ms;
        if(elapsed>=8400U){
            if(s->warning==UI_WARN_FUEL&&s->reserve_active)s->dashboard.footer=UI_RESV;
            s->warning=UI_WARN_NONE;s->warning_phase=UI_WARN_IDLE;Ui_ContextChanged(s);
        }else s->warning_phase=elapsed<5000U?UI_WARN_BLINK:elapsed<5400U?UI_WARN_MOVE:UI_WARN_TEXT;
    }
    Ui_StartWarning(s,e->now_ms);
}
