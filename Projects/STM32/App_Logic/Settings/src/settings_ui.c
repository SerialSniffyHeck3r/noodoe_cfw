#include "Settings_UI.h"
#include "BSP_Buttons.h"
#include <string.h>
#include <stdio.h>
SettingsUI *g_settings_ui;
/* Existing parent context invalidation cancels held remote/navigation intent. */
void Ui_ContextChanged(UiState *s);
static void Context(UiState *p)
{g_settings_ui->pressed=g_settings_ui->repeated=0;Ui_ContextChanged(p);}
static void Message(const char *text,uint32_t now)
{snprintf(g_settings_ui->message,sizeof(g_settings_ui->message),"%s",text);g_settings_ui->message_until=now+3000;}
void SettingsUI_Init(SettingsUI *s,uint32_t held)
{if(!s)return;memset(s,0,sizeof(*s));g_settings_ui=s;s->magic=0x534D5531;s->version=2;s->blocked=held;s->return_card=UI_BLANK;s->last_card=UI_BLANK;}
uint32_t SettingsUI_Quick(const UiState *p)
{return p&&g_settings_ui&&!g_settings_ui->open&&p->power==UI_RUNNING&&p->dashboard.card==UI_SYSTEM;}
/* Restore navigation only. Live clock/ODO/trips and changed preferences keep
 * progressing; reserve lock overrides the old footer selection. */
void SettingsUI_Close(UiState *p,uint32_t now)
{
    SettingsUI *s=g_settings_ui;if(!s||!s->open||s->leaving)return;
    s->leaving=1;s->mode=0;SceneTransition_Request(&s->transition,0,now);
    p->dashboard.card=s->return_card;p->dashboard.selection=s->return_selection;
    if(!p->reserve_active)p->dashboard.footer=s->return_footer;
    Context(p);
}
static void Enter(UiState *p,uint32_t now)
{
    SettingsUI *s=g_settings_ui;s->open=1;s->leaving=0;s->menu=s->row=s->mode=0;s->category=0;s->message[0]=0;
    p->menu=UI_MENU_SETTINGS;SceneTransition_Request(&s->transition,1,now);Context(p);
}
/* Month lengths follow Gregorian leap rules in the actual RTC range. */
static uint32_t MonthDays(uint32_t year,uint32_t month)
{static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};return month==2?28U+(year%4==0&&(year%100!=0||year%400==0)):days[month-1];}
uint32_t SettingsUI_FieldCount(uint32_t key)
{return key==SK_DATE?3:key==SK_TIME?2:1;}
static void Adjust(UiState *p,uint32_t key,int32_t direction,uint32_t accelerated,uint32_t now)
{
    SettingsUI *s=g_settings_ui;
    if(SettingsUI_Quick(p)){
        uint32_t k=AppSettings_Value(SK_MODE)?SK_BIAS:SK_BRIGHTNESS;const SettingItem *d=SettingsCatalog_Find(k);
        int32_t value=AppSettings_Value(k)+direction*d->step;if(value<d->min)value=d->min;if(value>d->max)value=d->max;
        uint32_t id;uint32_t result=AppSettings_RequestChange(k,value,&id);
        if(result&&result!=APP_SETTINGS_BUSY)Message("Brightness unavailable",now);
        return;
    }
    if(!s->open||s->leaving||s->motion.latched||s->request_id)return;
    if(s->mode==4)return;
    if(s->mode==2){s->confirm=!s->confirm;return;}
    if(s->mode==0){uint32_t count;SettingsCatalog_Items(s->menu,&count);++count; /* Virtual final Back row. */
        s->row=(s->row+count+(direction>0?-1:1))%count;if(!s->menu&&s->row<count-1)s->category=s->row;return;}
    if(s->mode==1){uint32_t count=SettingsUI_FieldCount(s->edit_key)+2;
        s->field=(s->field+count+(direction>0?-1:1))%count;return;}
    const SettingItem *item=SettingsCatalog_Find(s->edit_key);if(!item)return;
    if(item->kind==SETTING_DATE){
        uint32_t parts[3]={(uint32_t)s->edit/10000,(uint32_t)s->edit/100%100,(uint32_t)s->edit%100};
        int32_t min=s->field?1:2000,max=s->field==0?2099:s->field==1?12:(int32_t)MonthDays(parts[0],parts[1]);
        int32_t value=(int32_t)parts[s->field]+direction;if(value<min)value=min;if(value>max)value=max;parts[s->field]=value;
        if(parts[2]>MonthDays(parts[0],parts[1]))parts[2]=MonthDays(parts[0],parts[1]);
        s->edit=parts[0]*10000+parts[1]*100+parts[2];return;
    }
    if(item->kind==SETTING_TIME){int32_t hour=s->edit/100,minute=s->edit%100;
        if(s->field)minute=(minute+60+direction)%60;else hour=(hour+24+direction)%24;s->edit=hour*100+minute;return;}
    int32_t step=item->step*(accelerated&&item->kind==SETTING_NUMBER&&s->edit_key!=SK_BIAS?10:1);
    int32_t value=s->edit+direction*step;if(value<item->min)value=item->min;if(value>item->max)value=item->max;s->edit=value;
    (void)key;
}
static void Apply(uint32_t now)
{
    SettingsUI *s=g_settings_ui;uint32_t id,result=AppSettings_RequestChange(s->edit_key,s->edit,&id);
    if(!result)s->request_id=id;else Message(result==APP_SETTINGS_BUSY?"Please wait":result==APP_SETTINGS_LAST_STAGE?"Keep one stage enabled":"Not available",now);
}
/* Root/menu/field controls operate on one retained draft. Back never applies
 * it. Submenu selections are retained; no growing navigation stack exists. */
static void Select(UiState *p,uint32_t hold,uint32_t now)
{
    SettingsUI *s=g_settings_ui;if(s->leaving)return;
    if(s->motion.latched){if(hold)SettingsUI_Close(p,now);return;}
    if(s->request_id)return;
    if(hold){
        if(s->mode){s->mode=0;Context(p);return;}
        if(!s->menu){SettingsUI_Close(p,now);return;}
        s->rows[s->menu]=s->row;s->menu=SettingsCatalog_Parent(s->menu);s->row=s->rows[s->menu];Context(p);return;
    }
    if(s->mode==4){s->mode=0;Context(p);return;}
    if(s->mode==2){if(s->confirm)Apply(now);else s->mode=0;return;}
    if(s->mode==3){s->mode=1;Context(p);return;}
    if(s->mode==1){uint32_t fields=SettingsUI_FieldCount(s->edit_key);
        if(s->field<fields)s->mode=3;
        else if(s->field==fields)Apply(now);
        else s->mode=0; /* Back discards the draft without an apply request. */
        Context(p);return;}
    uint32_t count;const SettingItem *items=SettingsCatalog_Items(s->menu,&count);if(s->row>count)return;
    if(s->row==count){ /* Every menu, including root, has an ordinary Back item. */
        Select(p,1,now);return;}
    const SettingItem *item=&items[s->row];
    if(item->kind==SETTING_SUBMENU){s->rows[s->menu]=s->row;if(!s->menu)s->category=s->row;
        s->menu=item->key;if(s->menu==6)SettingsSystem_Enter();s->row=s->rows[s->menu];Context(p);return;}
    if(item->key==SK_PHOTO&&!g_app_settings->facts.photo_mask){Message("No photos installed",now);return;}
    if(item->kind==SETTING_DISABLED){Message("Not available",now);return;}
    if(item->kind==SETTING_READONLY)return;
    if(item->kind==SETTING_MESSAGE){s->edit_key=item->key;s->mode=4;Context(p);return;}
    s->edit_key=item->key;s->edit=AppSettings_Value(item->key);s->field=0;s->confirm=0;
    if(item->kind==SETTING_ACTION){s->edit=1;Apply(now);Context(p);return;}
    s->mode=item->kind==SETTING_CONFIRM?2:1;if(s->mode==2)s->edit=1;Context(p);
}
uint32_t SettingsUI_Button(UiState *p,uint32_t key,uint32_t type,uint32_t duration,uint32_t now)
{
    SettingsUI *s=g_settings_ui;if(!s||!p||key>=3)return 0;uint32_t bit=1U<<key;
    if(s->blocked&bit){if(type==BSP_BUTTON_EVENT_RELEASE){s->blocked&=~bit;p->blocked_buttons&=~bit;}return s->open||SettingsUI_Quick(p);}
    uint32_t quick=SettingsUI_Quick(p);if(!s->open&&!quick)return 0;
    if(type==BSP_BUTTON_EVENT_PRESS){
        if(!(s->pressed&bit)){s->pressed|=bit;s->repeated&=~bit;s->press_ms[key]=s->repeat_ms[key]=now;s->tokens[key]=p->context_token;}
        return !(quick&&key==BSP_BUTTON_ENTER);
    }
    if(type!=BSP_BUTTON_EVENT_RELEASE)return 1;
    uint32_t accepted=(s->pressed&bit)&&s->tokens[key]==p->context_token;s->pressed&=~bit;
    if(!accepted)return 1;
    if(p->power!=UI_RUNNING||p->warning)return 1;
    uint32_t hold=duration>=BSP_BUTTONS_LONG_PRESS_MS;
    if(quick){
        if(key!=BSP_BUTTON_ENTER){if(!(s->repeated&bit))Adjust(p,key,key==BSP_BUTTON_UP?1:-1,0,now);return 1;}
        if(!hold)return 0; /* Parent received PRESS; keep its normal one-way page cycle. */
        if(s->motion.ready)Enter(p,now);else{Message("Stop for 5 seconds",now);Context(p);}return 1;
    }
    if(s->transition.active)return 1;
    if(key==BSP_BUTTON_UP&&!hold&&s->menu==6&&!s->mode&&!s->motion.latched)SettingsSystem_Up();
    if(key==BSP_BUTTON_ENTER)Select(p,hold,now);
    else if(!(s->repeated&bit))Adjust(p,key,key==BSP_BUTTON_UP?1:-1,0,now);
    return 1;
}
void SettingsUI_Process(UiState *p,uint32_t now,uint32_t valid,uint32_t speed)
{
    SettingsUI *s=g_settings_ui;if(!s||!p)return;
    if(p->power!=UI_RUNNING){
        if(s->open){
            p->dashboard.card=s->return_card;p->dashboard.selection=s->return_selection;
            if(!p->reserve_active)p->dashboard.footer=s->return_footer;
            s->open=s->leaving=s->mode=0;p->menu=UI_MENU_NONE;Context(p);
        }
        memset(&s->transition,0,sizeof(s->transition));
        SceneTransition_Step(&s->transition,now,&s->pose);
        s->pressed=s->repeated=0;return;
    }
    /* Capture the last non-quick page before its selection is reset by the
     * carousel. Settings restoration never takes a DATA_DEBUG navigation path. */
    if(!s->open){
        if(p->dashboard.card==UI_SYSTEM&&s->last_card!=UI_SYSTEM){s->return_card=s->last_card;s->return_selection=s->last_selection;s->return_footer=s->last_footer;}
        if(p->dashboard.card!=UI_SYSTEM){s->last_selection=p->dashboard.selection;s->last_footer=p->dashboard.footer;}
        s->last_card=p->dashboard.card;
    }
    uint32_t was_locked=s->motion.latched;
    SettingsMotion_Tick(&s->motion,now,valid,speed,s->open&&!s->leaving);
    if(was_locked!=s->motion.latched)Context(p);
    if(s->open&&(p->power!=UI_RUNNING||p->warning||s->motion.expired))SettingsUI_Close(p,now);
    if(s->request_id){uint32_t result;if(AppSettings_GetResult(s->request_id,&result)){
        s->request_id=0;if(!result)s->mode=0;
        const char *text=result?"Apply failed":"Applied";
        if(!result){
            if(s->edit_key==SK_PAIR||s->edit_key==SK_BT_REPAIR)text="Pairing open for 2 min";
            else if(s->edit_key==SK_BT_CLOSE)text="Pairing closed";
            else if(s->edit_key==SK_BT_DISCONNECT)text="Phone disconnected";
            else if(s->edit_key==SK_BT_RESTART)text="Bluetooth ready";
            else if(s->edit_key==SK_SENSOR_RETRY)text="Sensor ready";
        }
        Message(text,now);}}
    if(s->request_id)Message("Working...",now);
    else if((int32_t)(now-s->message_until)>=0)s->message[0]=0;
    SceneTransition_Step(&s->transition,now,&s->pose);
    if(s->leaving&&!s->transition.active){s->open=s->leaving=0;p->menu=UI_MENU_NONE;Context(p);}
    /* Repeat only adjustable values, never confirmation or menu selection.
     * Expired/recontextualized held inputs cannot leak across a scene change. */
    if((SettingsUI_Quick(p)||s->mode==3)&&!s->motion.latched&&!s->transition.active&&!s->request_id&&p->power==UI_RUNNING&&!p->warning){
        for(uint32_t key=0;key<2;++key)if((s->pressed&(1U<<key))&&s->tokens[key]==p->context_token&&
            now-s->press_ms[key]>=500U&&now-s->repeat_ms[key]>=100U){
            s->repeat_ms[key]=now;s->repeated|=1U<<key;Adjust(p,key,key==BSP_BUTTON_UP?1:-1,now-s->press_ms[key]>=2001U,now);}
    }
}
