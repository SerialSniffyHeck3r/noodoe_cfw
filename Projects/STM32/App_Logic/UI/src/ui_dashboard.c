#include "ui_state_internal.h"
#include "Ui_Home.h"

/* Bounded cycle inside this child's enums, independent of labels/glyphs. */
static uint32_t Cycle(uint32_t value,uint32_t count,uint32_t mask,uint32_t down)
{
    for(uint32_t n=0;n<count;++n){value=down?(value+1U)%count:(value+count-1U)%count;
        if(mask&UI_BIT(value))return value;}
    return 0U;
}

/* Child owns preferences and local selection. Parent supplies availability;
 * the renderer keeps object pointers, never a second copy of this state. */
void UiDashboard_Init(UiDashboardState *d,uint32_t cards,uint32_t footers,
    uint32_t card,uint32_t footer,uint32_t reserve)
{
    if(!d)return;
    memset(d,0,sizeof(*d));cards&=UI_CARD_MASK;
    if(!cards)cards=UI_BIT(UI_BLANK);
    d->card=card<UI_CARD_COUNT?card:UI_BLANK;
    if(!(cards&UI_BIT(d->card)))d->card=Cycle(UI_CARD_COUNT-1U,UI_CARD_COUNT,cards,1U);
    d->footer=footer<UI_FOOTER_COUNT?footer:UI_ODO;
    if(!(footers&UI_BIT(d->footer))||d->footer==UI_RESV)d->footer=UI_ODO;
    d->gps_zoom=2U;d->footer_before_reserve=d->footer;if(reserve)d->footer=UI_RESV;
}

/* TRIP F is the displayed name of the RESV latch. A label change must not
 * turn reserve distance into an ordinary manually resettable trip. */
const char *UiDashboard_CardTitle(uint32_t card)
{
    static const char *const titles[]={"","Trip computer","Phone","Music",
        "","","Phone GPS","Settings","Calls"};
    return card<UI_CARD_COUNT?titles[card]:"";
}
const char *UiDashboard_FooterTitle(uint32_t footer)
{
    static const char *const titles[]={"ODO","TRIP 1","TRIP 2","TRIP F","OIL","BELT","SERV"};
    return footer<UI_FOOTER_COUNT?titles[footer]:"ODO";
}

/* Parent first handles power, warnings, modal/menu interception and press
 * classification. Child emits through the same parent epoch/outbox and cannot
 * touch Bluetooth, persistent storage or power directly. */
void Ui_DashboardNavigate(UiState *s,uint32_t b,uint32_t hold)
{
    UiDashboardState *d=&s->dashboard;
    if(UiCalls_Navigate(s,b,hold))return;
    extern uint32_t Ui_PhoneNavigate(UiState*,uint32_t,uint32_t);
    if(Ui_PhoneNavigate(s,b,hold))return;
    /* Music owns the direction keys: one release chooses one command. Its
     * long UP must not also change the distance footer. Short ENTER retains
     * the global forward category step; long ENTER has no phone-switch action. */
    if(d->card==UI_MUSIC&&(b!=UI_ENTER||hold)){
        if(b==UI_ENTER || (b==UI_DOWN&&hold))return;
        uint32_t action=b==UI_DOWN?UI_MEDIA_NEXT:hold?UI_MEDIA_PREVIOUS:UI_MEDIA_TOGGLE;
        (void)Ui_Emit(s,UI_FX_MEDIA,action,0);return;
    }
    /* GPS keys own zoom/orientation; they never reset trips or move footers. */
    if(d->card==UI_PHONE_GPS&&(b!=UI_ENTER||hold)){
        if(b==UI_ENTER)d->gps_heading_up^=1U;
        else if(b==UI_UP&&d->gps_zoom)d->gps_zoom--;
        else if(b==UI_DOWN&&d->gps_zoom<5)d->gps_zoom++;
        return;
    }
    if(b==UI_ENTER&&!hold){
        /* Stable persisted IDs are independent from the visible order. */
        static const uint8_t order[]={UI_BLANK,UI_TRIP,UI_NOTIFICATIONS,UI_MUSIC,UI_CALLS,UI_PHONE_GPS,UI_SYSTEM};
        uint32_t at=0;while(at<sizeof(order)&&order[at]!=d->card)at++;
        uint32_t available=Ui_AvailableCards(s);
        for(uint32_t i=0;i<sizeof(order);i++){at=(at+1)%sizeof(order);if(available&UI_BIT(order[at])){d->card=order[at];break;}}
        d->selection=0U;s->notification_open=d->card==UI_NOTIFICATIONS;s->notification_reply=0;Ui_ContextChanged(s);return;
    }
    if(b!=UI_ENTER){
        /* Hold UP/DOWN selects the persistent distance footer; short presses
         * stay inside the current page. Reserve still owns its footer lock. */
        if(hold&&d->card!=UI_BLANK&&!s->reserve_active){
            d->footer=Cycle(d->footer,UI_FOOTER_COUNT,s->config.footer_mask&~UI_BIT(UI_RESV),b==UI_DOWN);return;
        }
        if(d->card==UI_BLANK)d->selection=Cycle(d->selection,UI_HOME_VIEWS,7U,b==UI_DOWN);
        else if(d->card==UI_TRIP)d->selection=Cycle(d->selection,4U,15U,b==UI_DOWN);
        else if(d->card==UI_NOTIFICATIONS){
            if(!s->speed_valid||s->speed_kph>50U){d->selection=0U;return;}
            if(b==UI_DOWN&&d->selection+1U<s->notification_count)++d->selection;
            else if(b==UI_UP&&d->selection)--d->selection;
        }
        return;
    }
    if(!hold)return;
    switch(d->card){
    case UI_BLANK:if(!s->reserve_active)d->footer=Cycle(d->footer,UI_FOOTER_COUNT,s->config.footer_mask&~UI_BIT(UI_RESV),1U);break;
    case UI_SYSTEM:break; /* SettingsUI owns its stationary hold gate. */
    case UI_NOTIFICATIONS:break;
    case UI_REMOTE:break; /* Retired ID, never a navigation destination. */
    case UI_TRIP:
        if(d->selection>1U)break;
        if(s->pending_id)break;
        s->pending_id=Ui_Emit(s,UI_FX_RESET_TRIP,d->selection==0U?UI_TRIP1:UI_TRIP2,0);
        Ui_ContextChanged(s);break;
    default:break;
    }
}
