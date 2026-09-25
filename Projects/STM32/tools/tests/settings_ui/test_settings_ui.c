#include "Settings_UI.h"
#include "Power_Scene.h"
#include "BSP_Buttons.h"
#include "ClockService.h"
#include <string.h>
#include "support_radio.h"
#include "InputMode.h"
InputModeState g_input_mode;
volatile uint32_t g_assertions,g_failure_line;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
static AppSettings app;static SettingsUI ui;static UiState parent;
static uint32_t pwm,device_error,clock_pending,clock_done,clock_status;static BSP_Clock_Time clock_value;
uint32_t SettingsDevice_Lock(void){return 0;}
void SettingsDevice_Unlock(uint32_t n){(void)n;}
uint32_t Graphics_SetBrightnessPercent(uint32_t p){if(device_error)return 1;pwm=p;return 0;}
void DashService_SetLightBias(int32_t value){dash.light_bias=value;}
uint32_t Wallpaper_SetEnabled(uint32_t n){return n<=1;}
uint32_t Wallpaper_SelectPhoto(uint32_t n){return n<3;}
uint32_t Wallpaper_SetOffPhoto(uint32_t n){return n<=3;}
uint32_t Wallpaper_SetCenterBrightness(uint32_t n){return n<=100;}
uint32_t ProductUI_SelectPhone(uint32_t n){return n<2;}
uint32_t ClockService_Request(const BSP_Clock_Time *v,uint32_t *id)
{clock_value=*v;clock_pending=1;clock_done=0;*id=1;return 0;}
uint32_t ClockService_Result(uint32_t id,uint32_t *r)
{if(!id||!clock_done)return 0;*r=clock_status;return 1;}
void Ui_ContextChanged(UiState *p){++p->context_token;p->pressed_buttons=0;}
static void Init(void)
{
    AppSettings_Init(&app);SettingsUI_Init(&ui,0);memset(&parent,0,sizeof(parent));
    parent.power=UI_RUNNING;parent.dashboard.card=UI_TRIP;parent.dashboard.selection=1;parent.dashboard.footer=UI_TRIP2;
    parent.context_token=1;pwm=25;device_error=clock_pending=clock_done=clock_status=0;
}
static void Tick(uint32_t now,uint32_t valid,uint32_t speed)
{AppSettings_Process(NULL,now);SettingsUI_Process(&parent,now,valid,speed);}
static uint32_t Key(uint32_t key,uint32_t duration,uint32_t now)
{SettingsUI_Button(&parent,key,BSP_BUTTON_EVENT_PRESS,0,now);return SettingsUI_Button(&parent,key,BSP_BUTTON_EVENT_RELEASE,duration,now+duration);}
static uint32_t Motion(void)
{
    SettingsMotion m={0};SettingsMotion_Tick(&m,0,1,3,0);CHECK(!m.ready);
    SettingsMotion_Tick(&m,4999,1,3,0);CHECK(!m.ready);
    SettingsMotion_Tick(&m,5000,1,3,0);CHECK(m.ready);
    SettingsMotion_Tick(&m,5001,1,4,0);CHECK(!m.ready);
    SettingsMotion_Tick(&m,5002,0,0,1);CHECK(m.latched&&m.unknown&&m.started_ms==5002);
    SettingsMotion_Tick(&m,9000,1,9,1);CHECK(m.started_ms==5002&&m.elapsed_ms==3998);
    SettingsMotion_Tick(&m,10000,1,3,1);SettingsMotion_Tick(&m,15000,1,3,1);CHECK(m.latched&&!m.recovery_tracking);
    SettingsMotion_Tick(&m,15001,1,2,1);SettingsMotion_Tick(&m,18000,1,2,1);CHECK(m.latched);
    SettingsMotion_Tick(&m,18001,1,2,1);CHECK(!m.latched&&!m.expired);
    SettingsMotion_Tick(&m,20000,1,4,1);SettingsMotion_Tick(&m,49999,1,3,1);CHECK(m.latched&&!m.expired);
    SettingsMotion_Tick(&m,50000,1,3,1);CHECK(m.expired&&m.elapsed_ms==30000);
    memset(&m,0,sizeof(m));SettingsMotion_Tick(&m,0,1,4,1);SettingsMotion_Tick(&m,27000,1,2,1);
    SettingsMotion_Tick(&m,30000,1,2,1);CHECK(!m.latched&&!m.expired);
    memset(&m,0,sizeof(m));SettingsMotion_Tick(&m,0xFFFFF000U,1,0,0);SettingsMotion_Tick(&m,904,1,0,0);CHECK(m.ready);
    SettingsMotion_Tick(&m,905,1,401,1);CHECK(m.latched&&m.unknown);
    return 0;
}
static uint32_t Scene(void)
{
    SceneTransition s={0};ScenePose p;SceneTransition_Request(&s,1,100);
    SceneTransition_Step(&s,100,&p);CHECK(p.progress==0&&p.ring_radius==240);
    SceneTransition_Step(&s,300,&p);CHECK(p.progress==512&&p.footer_y==50&&p.clock_y==-50);
    SceneTransition_Step(&s,500,&p);CHECK(!s.active&&p.progress==1024&&p.settings_alpha==255&&p.footer_y==100);
    SceneTransition_Request(&s,0,500);SceneTransition_Step(&s,700,&p);CHECK(p.progress==512);
    SceneTransition_Request(&s,1,700);SceneTransition_Step(&s,700,&p);CHECK(p.progress==512);
    SceneTransition_Step(&s,1100,&p);CHECK(p.progress==1024);
    SceneTransition_Request(&s,0,1100);SceneTransition_Step(&s,1500,&p);CHECK(!p.progress&&p.ring_alpha==255&&!p.settings_alpha);
    return 0;
}
static uint32_t Preferences(void)
{
    Init();uint32_t id,result;CHECK(AppSettings_Value(SK_POWER)==60&&AppSettings_Value(SK_BT_HOLD)==60);CHECK(AppSettings_RequestChange(SK_BRIGHTNESS,30,&id)==0);CHECK(pwm==25);
    CHECK(AppSettings_RequestChange(SK_BRIGHTNESS,35,&id)==APP_SETTINGS_BUSY);AppSettings_Process(NULL,0);
    CHECK(AppSettings_GetResult(id,&result)&&!result&&pwm==30);
    CHECK(AppSettings_Value(SK_TRIP_STOP_SPEED)==5);
    CHECK(AppSettings_RequestChange(SK_TRIP_STOP_SPEED,-1,&id)==APP_SETTINGS_ARGUMENT);
    CHECK(AppSettings_RequestChange(SK_TRIP_STOP_SPEED,11,&id)==APP_SETTINGS_ARGUMENT);
    for(int32_t speed=0;speed<=10;++speed){
        CHECK(AppSettings_RequestChange(SK_TRIP_STOP_SPEED,speed,&id)==0);
        AppSettings_Process(NULL,0);CHECK(AppSettings_GetResult(id,&result)&&!result);
        CHECK(AppSettings_Value(SK_TRIP_STOP_SPEED)==speed);
    }
    char stop_label[32];AppSettings_FormatValue(SK_TRIP_STOP_SPEED,5,stop_label,sizeof(stop_label));CHECK(!strcmp(stop_label,"< 5 km/h"));
    AppSettings_Format(SK_BT_HOLD,stop_label,sizeof(stop_label));CHECK(!strcmp(stop_label,"60 min"));
    CHECK(AppSettings_RequestChange(SK_BT_HOLD,0,&id)==0);AppSettings_Process(NULL,0);
    AppSettings_FormatValue(SK_TRIP_STOP_SPEED,0,stop_label,sizeof(stop_label));CHECK(!strcmp(stop_label,"0 km/h only"));
    CHECK(AppSettings_RequestChange(SK_MODE,1,&id)==0);AppSettings_Process(NULL,1);CHECK(pwm==30);
    CHECK(AppSettings_RequestChange(SK_BIAS,2,&id)==0);AppSettings_Process(NULL,2);CHECK(pwm==30&&app.values[SK_BIAS]==2);
    CHECK(AppSettings_RequestChange(SK_BRIGHTNESS,60,&id)==0);AppSettings_Process(NULL,3);CHECK(pwm==30&&app.values[SK_BRIGHTNESS]==60);
    CHECK(AppSettings_RequestChange(SK_MODE,0,&id)==0);AppSettings_Process(NULL,4);CHECK(pwm==60);
    device_error=1;CHECK(AppSettings_RequestChange(SK_BRIGHTNESS,65,&id)==0);AppSettings_Process(NULL,5);
    CHECK(AppSettings_GetResult(id,&result)&&result==APP_SETTINGS_DEVICE_ERROR&&app.values[SK_BRIGHTNESS]==60);device_error=0;
    CHECK(AppSettings_RequestChange(SK_BRIGHTNESS,0,&id)==APP_SETTINGS_ARGUMENT);
    CHECK(AppSettings_RequestChange(SK_PHOTO,1,&id)==APP_SETTINGS_UNAVAILABLE);
    CHECK(AppSettings_RequestChange(SK_AUTO_STATUS,1,&id)==APP_SETTINGS_UNAVAILABLE);
    CHECK(AppSettings_RequestChange(SK_OIL_RESET,0,&id)==APP_SETTINGS_ARGUMENT);
    app.facts.clock_valid=1;app.facts.utc=(BSP_CalendarDateTime){2024,2,28,3,18,15,0};
    CHECK(AppSettings_Value(SK_DATE)==20240229&&AppSettings_Value(SK_TIME)==315);
    CHECK(AppSettings_RequestChange(SK_TIME,30,&id)==0);AppSettings_Process(NULL,10);
    CHECK(clock_pending&&clock_value.year==2024&&clock_value.month==2&&clock_value.day==28&&clock_value.hour==15&&clock_value.minute==30);
    CHECK(!AppSettings_GetResult(id,&result));clock_done=1;AppSettings_Process(NULL,11);CHECK(AppSettings_GetResult(id,&result)&&!result);
    CHECK(AppSettings_RequestChange(SK_DATE,20230229,&id)==0);AppSettings_Process(NULL,20);CHECK(AppSettings_GetResult(id,&result)&&result==APP_SETTINGS_ARGUMENT);
    CHECK(AppSettings_RequestChange(SK_TIME,9999,&id)==APP_SETTINGS_ARGUMENT);
    app.facts.odo_valid=app.facts.on_valid=1;app.facts.odo_km=1000;app.facts.on_ms=3600000;
    CHECK(AppSettings_RequestChange(SK_OIL_RESET,1,&id)==0);AppSettings_Process(NULL,21);
    app.values[SK_OIL_DISTANCE]=2000;app.values[SK_OIL_HOURS]=100;app.values[SK_OIL_DAYS]=365;
    app.facts.odo_km=2000;app.facts.on_ms+=100*3600000ULL;
    UiDashboardDistances d;UiDashboardMaintenance m;uint32_t due;AppSettings_Maintenance(&d,&m,&due);
    CHECK((due&1)&&m.remaining_permille[0]==0&&m.oil_ignition_ms==100*3600000ULL&&d.distance_mm[UI_OIL]==1000000000ULL);
    app.facts.on_valid=0;AppSettings_Maintenance(&d,&m,&due);CHECK(!(m.valid_mask&1)&&!(due&1));
    app.facts.odo_km=3000;AppSettings_Maintenance(&d,&m,&due);CHECK((due&1)&&!(m.valid_mask&1));
    return 0;
}
static uint32_t Navigation(void)
{
    Init();Tick(0,1,0);parent.dashboard.card=UI_SYSTEM;parent.dashboard.selection=0;Tick(100,1,0);
    CHECK(ui.return_card==UI_TRIP&&ui.return_selection==1&&ui.return_footer==UI_TRIP2);
    CHECK(Key(BSP_BUTTON_ENTER,100,100)==0); /* Short remains the original carousel. */
    CHECK(Key(BSP_BUTTON_ENTER,2001,100)==1&&!ui.open);
    Tick(5000,1,3);CHECK(ui.motion.ready);Key(BSP_BUTTON_ENTER,2001,5000);Tick(7500,1,3);
    CHECK(ui.open&&!ui.transition.active&&parent.menu==UI_MENU_SETTINGS);
    Key(BSP_BUTTON_ENTER,100,7500);CHECK(ui.menu==1);
    Key(BSP_BUTTON_DOWN,100,7700);CHECK(ui.row==1);Key(BSP_BUTTON_ENTER,100,7900);CHECK(ui.mode==1&&ui.edit_key==SK_BRIGHTNESS);
    Key(BSP_BUTTON_ENTER,100,8100);CHECK(ui.mode==3);
    Key(BSP_BUTTON_UP,100,8300);CHECK(ui.edit==30&&app.values[SK_BRIGHTNESS]==25);
    Tick(9000,1,4);CHECK(ui.motion.latched);Key(BSP_BUTTON_UP,100,9100);CHECK(ui.edit==30);
    Tick(10000,1,2);Tick(12999,1,2);CHECK(ui.motion.latched);Tick(13000,1,2);CHECK(!ui.motion.latched&&ui.mode==3&&ui.edit==30);
    Key(BSP_BUTTON_ENTER,100,13100);CHECK(ui.mode==1);
    Key(BSP_BUTTON_DOWN,100,13300);CHECK(ui.field==1);
    Key(BSP_BUTTON_ENTER,100,13500);Tick(13700,1,2);CHECK(!ui.request_id&&!ui.mode&&app.values[SK_BRIGHTNESS]==30);
    Tick(14000,1,4);Tick(43999,1,4);CHECK(ui.open&&!ui.leaving);Tick(44000,1,4);CHECK(ui.leaving);
    Tick(44400,1,4);CHECK(!ui.open&&!parent.menu&&parent.dashboard.card==UI_TRIP&&parent.dashboard.selection==1&&parent.dashboard.footer==UI_TRIP2);
    /* Quick long-repeat changes brightness, never footer or two short actions. */
    Init();parent.dashboard.card=UI_SYSTEM;Tick(0,1,0);uint32_t footer=parent.dashboard.footer;
    SettingsUI_Button(&parent,BSP_BUTTON_DOWN,BSP_BUTTON_EVENT_PRESS,0,100);
    Tick(600,1,0);Tick(700,1,0);Tick(800,1,0);
    SettingsUI_Button(&parent,BSP_BUTTON_DOWN,BSP_BUTTON_EVENT_RELEASE,750,850);Tick(900,1,0);
    CHECK(parent.dashboard.footer==footer&&app.values[SK_BRIGHTNESS]==10);
    /* Boot-held candidate stays suppressed until the matching release. */
    SettingsUI_Init(&ui,1);parent.blocked_buttons=1;Key(BSP_BUTTON_UP,100,1000);CHECK(!(ui.blocked&1)&&!(parent.blocked_buttons&1));
    return 0;
}
/* All Back operations below use ordinary100ms taps. A field may be adjusted,
 * abandoned or applied without relying on any long-press escape path. */
static uint32_t TapNavigation(void)
{
    for(uint32_t menu=0;menu<12;++menu){
        Init();ui.open=1;ui.menu=menu;parent.menu=UI_MENU_SETTINGS;
        uint32_t n;SettingsCatalog_Items(menu,&n);ui.row=n;
        Key(BSP_BUTTON_ENTER,100,100);
        CHECK(menu?(ui.menu==SettingsCatalog_Parent(menu)&&!ui.leaving):ui.leaving);
    }
    Init();ui.open=1;ui.menu=1;ui.row=1;parent.menu=UI_MENU_SETTINGS;
    Key(BSP_BUTTON_ENTER,100,100);CHECK(ui.mode==1&&ui.field==0);
    Key(BSP_BUTTON_ENTER,100,300);CHECK(ui.mode==3);
    Key(BSP_BUTTON_UP,100,500);CHECK(ui.edit==30&&pwm==25);
    Key(BSP_BUTTON_ENTER,100,700);CHECK(ui.mode==1);
    Key(BSP_BUTTON_UP,100,900);CHECK(ui.field==2);
    Key(BSP_BUTTON_ENTER,100,1100);Tick(1300,1,0);
    CHECK(ui.mode==0&&pwm==25&&!ui.request_id);
    /* Date has three selectable fields, Apply and Back. Time has two. */
    CHECK(SettingsUI_FieldCount(SK_DATE)==3&&SettingsUI_FieldCount(SK_TIME)==2);
    app.facts.clock_valid=1;app.facts.utc=(BSP_CalendarDateTime){2024,2,29,4,0,0,0};
    ui.menu=2;ui.row=0;Key(BSP_BUTTON_ENTER,100,1400);CHECK(ui.mode==1&&ui.edit_key==SK_DATE);
    Key(BSP_BUTTON_ENTER,100,1600);Key(BSP_BUTTON_UP,100,1800);CHECK(ui.edit==20250228);
    Key(BSP_BUTTON_ENTER,100,2000);Key(BSP_BUTTON_UP,100,2200);CHECK(ui.field==4);
    Key(BSP_BUTTON_ENTER,100,2400);CHECK(!ui.mode&&!clock_pending);
    /* Cancel-first service confirmation also has a short-select Back row. */
    ui.menu=7;ui.row=3;Key(BSP_BUTTON_ENTER,100,2600);CHECK(ui.mode==2&&!ui.confirm);
    Key(BSP_BUTTON_ENTER,100,2800);CHECK(!ui.mode&&!ui.request_id&&!app.origins[0].valid);
    /* Root Back is not a seventh category; its hidden top arc never overflows. */
    ui.menu=0;ui.row=0;ui.category=0;Key(BSP_BUTTON_UP,100,3000);CHECK(ui.row==7&&ui.category==0);
    return 0;
}
static uint32_t PowerPreemptsSettings(void)
{
    Init();ui.open=1;ui.mode=3;ui.edit=777;ui.return_card=UI_MUSIC;ui.return_selection=1;
    parent.menu=UI_MENU_SETTINGS;parent.power=IGN_STOPPING;
    SettingsUI_Process(&parent,1000,1,0);
    CHECK(!ui.open&&!ui.leaving&&!ui.mode&&!parent.menu);
    CHECK(parent.dashboard.card==UI_MUSIC&&parent.dashboard.selection==1);
    CHECK(!ui.transition.active&&!ui.pressed);
    return 0;
}
static uint32_t PowerReversal(void)
{
    PowerScene a;ScenePose p={0};PowerScene_Seed(&a,&p);
    PowerScene_Request(&a,1,0,1000);PowerScene_Step(&a,1200,&p);
    CHECK(p.progress==512&&p.footer_y==0&&p.clock_y==0);
    PowerScene_Request(&a,0,0,1200);PowerScene_Step(&a,1200,&p);CHECK(p.progress==512);
    PowerScene_Step(&a,1300,&p);uint32_t q=p.progress;CHECK(q>0&&q<512);
    PowerScene_Request(&a,1,0,1300);PowerScene_Step(&a,1300,&p);CHECK(p.progress==q);
    PowerScene_Step(&a,1340,&p);CHECK(p.progress>q&&p.progress<1024);
    for(uint32_t i=0;i<30;++i){uint32_t now=1400+i*17;
        PowerScene_Step(&a,now,&p);q=p.progress;
        PowerScene_Request(&a,i&1,i&1,now);PowerScene_Step(&a,now,&p);
        CHECK(p.progress==q&&p.footer_y>=0&&p.footer_y<=100&&p.clock_y<=0);
    }
    PowerScene_Request(&a,0,0,3000);PowerScene_Step(&a,3400,&p);
    CHECK(p.progress==0&&p.footer_y==0&&p.clock_y==0&&p.ring_alpha==255);
    p=(ScenePose){.progress=512,.footer_y=50,.clock_y=-50};PowerScene_Seed(&a,&p);
    PowerScene_Request(&a,1,0,4000);PowerScene_Step(&a,4200,&p);
    CHECK(p.progress==768&&p.footer_y==25&&p.clock_y==-25);
    PowerScene_HoldRing(&a);q=p.progress;
    PowerScene_Step(&a,4699,&p);CHECK(p.progress==q&&p.footer_y==0);
    PowerScene_Request(&a,1,0,4700);PowerScene_Step(&a,4700,&p);CHECK(p.progress==q);
    PowerScene_Step(&a,5100,&p);CHECK(p.progress==1024&&p.ring_alpha==0);
    return 0;
}
/* The real settings request queue validates masks; editing remains a draft.
 * Verify ordinary taps reach the new subtree and return to System. */
static uint32_t PowerSettings(void)
{
    Init();uint32_t id,result;char text[40];
    CHECK(SettingsPower_EnabledMask()==7&&app.version==2&&ui.version==2);
    for(uint32_t key=SK_POWER;key<=SK_BT_HOLD;key+=SK_BT_HOLD-SK_POWER){
        CHECK(AppSettings_RequestChange(key,-1,&id)==APP_SETTINGS_ARGUMENT);
        CHECK(AppSettings_RequestChange(key,1441,&id)==APP_SETTINGS_ARGUMENT);
        CHECK(AppSettings_RequestChange(key,1,&id)==0);Tick(0,1,0);
        CHECK(AppSettings_GetResult(id,&result)&&!result&&AppSettings_Value(key)==1);
        AppSettings_Format(key,text,sizeof(text));CHECK(!strcmp(text,"1 min"));
    }
    CHECK(AppSettings_RequestChange(SK_OFF_FINAL_WAIT,0,&id)==APP_SETTINGS_UNAVAILABLE);
    AppSettings_Format(SK_OFF_FINAL_WAIT,text,sizeof(text));CHECK(!strcmp(text,"Until IGN ON"));
    for(uint32_t keep=SK_OFF_DISPLAY;keep<=SK_OFF_DEEP;++keep){
        Init();
        for(uint32_t key=SK_OFF_DISPLAY;key<=SK_OFF_DEEP;++key)if(key!=keep){
            CHECK(AppSettings_RequestChange(key,0,&id)==0);Tick(0,1,0);
            AppSettings_Format(key,text,sizeof(text));CHECK(!strcmp(text,"Skipped"));
        }
        CHECK(SettingsPower_EnabledMask()==(1U<<(keep-SK_OFF_DISPLAY)));
        CHECK(AppSettings_RequestChange(keep,0,&id)==APP_SETTINGS_LAST_STAGE);
        CHECK(!app.pending_id&&AppSettings_Value(keep)==1);
    }
    Init();ui.open=1;ui.menu=0;ui.row=5;parent.menu=UI_MENU_SETTINGS;
    Key(BSP_BUTTON_ENTER,100,100);CHECK(ui.menu==6);
    for(uint32_t n=0;n<4;++n)Key(BSP_BUTTON_DOWN,100,300+n*200);
    CHECK(ui.row==4);Key(BSP_BUTTON_ENTER,100,1100);CHECK(ui.menu==10&&ui.row==0);
    Key(BSP_BUTTON_ENTER,100,1200);CHECK(ui.mode==1&&ui.edit_key==SK_OFF_DISPLAY);
    Key(BSP_BUTTON_ENTER,100,1400);Key(BSP_BUTTON_DOWN,100,1600);
    CHECK(ui.mode==3&&!ui.edit&&SettingsPower_EnabledMask()==7);
    Key(BSP_BUTTON_ENTER,100,1800);Key(BSP_BUTTON_DOWN,100,2000);
    Key(BSP_BUTTON_ENTER,100,2200);Tick(2400,1,0);
    CHECK(!ui.mode&&!ui.request_id&&SettingsPower_EnabledMask()==6);
    /* Back is the seventh ordinary row, and returns to System's saved row. */
    for(uint32_t n=0;n<6;++n)Key(BSP_BUTTON_DOWN,100,2600+n*200);
    Key(BSP_BUTTON_ENTER,100,4000);CHECK(ui.menu==6&&ui.row==4);
    return 0;
}
static uint32_t Connections(void)
{
    Init();uint32_t id,r;char text[64];
    g_bluetooth.state=BLUETOOTH_STATE_READY;g_settings.provisioned=1;bonds=2;
    CHECK(AppSettings_RequestChange(SK_PAIR,0,&id)==APP_SETTINGS_ARGUMENT);
    CHECK(AppSettings_RequestChange(SK_PAIR,1,&id)==0);
    for(uint32_t n=0;n<3;++n)AppSettings_Process(NULL,n);
    CHECK(AppSettings_GetResult(id,&r)&&!r&&pairing==120);
    AppSettings_Format(SK_PAIR,text,sizeof(text));CHECK(!strcmp(text,"Visible for 120 s"));
    CHECK(AppSettings_RequestChange(SK_BT_REPAIR,1,&id)==0);AppSettings_Process(NULL,10);
    CHECK(stops==1&&clears==0);AppSettings_Process(NULL,11);CHECK(clears==0);
    g_bluetooth.state=BLUETOOTH_STATE_OFF;AppSettings_Process(NULL,12);
    CHECK(clears==1&&bonds==0&&starts==0);
    AppSettings_Process(NULL,13);CHECK(!AppSettings_GetResult(id,&r)&&starts==0);
    g_bluetooth.key_persisted_generation=g_bluetooth.key_generation;
    AppSettings_Process(NULL,14);AppSettings_Process(NULL,15);
    CHECK(starts==1);AppSettings_Process(NULL,16);CHECK(starts==1); /* Await ACK, never resend. */
    control.ack_sequence=control.request_sequence;control.phase=2;g_bluetooth.state=BLUETOOTH_STATE_STARTING;
    AppSettings_Process(NULL,17);CHECK(!AppSettings_GetResult(id,&r));
    g_bluetooth.state=BLUETOOTH_STATE_READY;AppSettings_Process(NULL,18);AppSettings_Process(NULL,19);
    CHECK(AppSettings_GetResult(id,&r)&&!r&&pairing==120);
    install_hold=1;CHECK(AppSettings_RequestChange(SK_BT_REPAIR,1,&id)==0);AppSettings_Process(NULL,20);
    CHECK(AppSettings_GetResult(id,&r)&&r==APP_SETTINGS_UNAVAILABLE&&clears==1&&stops==1);install_hold=0;
    CHECK(AppSettings_RequestChange(SK_BT_CLOSE,1,&id)==0);AppSettings_Process(NULL,21);AppSettings_Process(NULL,22);
    CHECK(AppSettings_GetResult(id,&r)&&!r&&!pairing);
    /* Permanent-store failure may not reopen pairing or claim completion. */
    CHECK(AppSettings_RequestChange(SK_BT_REPAIR,1,&id)==0);AppSettings_Process(NULL,100);
    g_bluetooth.state=BLUETOOTH_STATE_OFF;AppSettings_Process(NULL,101);
    AppSettings_Process(NULL,35099);CHECK(!AppSettings_GetResult(id,&r)&&starts==1);
    AppSettings_Process(NULL,35100);CHECK(AppSettings_GetResult(id,&r)&&r==APP_SETTINGS_DEVICE_ERROR&&!pairing);
    /* Explicit retry fails once with controller error; no infinite restart. */
    CHECK(AppSettings_RequestChange(SK_PAIR,1,&id)==0);AppSettings_Process(NULL,40000);AppSettings_Process(NULL,40001);
    control.ack_sequence=control.request_sequence;control.phase=4;control.accept_result=-3;
    AppSettings_Process(NULL,40002);CHECK(AppSettings_GetResult(id,&r)&&r==APP_SETTINGS_DEVICE_ERROR&&starts==2);
    return 0;
}
static uint32_t AutoBrightness(void)
{
    Init();app.values[SK_MODE]=1;dash.calibration_valid=1;dash.light_source=DASH_LIGHT_LIVE;dash.light_index=9;dash.raw_light_index=9;
    for(uint32_t n=1000;n<=3000;n+=100){dash.light_sample_ms=n;SettingsDisplay_Tick(n,1);}
    CHECK(app.effective_brightness>25&&app.effective_brightness<100);
    uint32_t before=app.effective_brightness;
    SettingsDisplay_Tick(3200,0);CHECK(app.effective_brightness==before); /* IGN OFF cannot relight. */
    dash.light_source=DASH_LIGHT_OVERRIDE;SettingsDisplay_Tick(3400,1);CHECK(app.effective_brightness<before);
    for(uint32_t n=3500;n<9000;n+=100)SettingsDisplay_Tick(n,1);
    CHECK(app.effective_brightness==25);
    ambient.driver.valid=1;ambient.driver.millilux=1234000;char text[64];SettingsDisplay_SensorText(text,sizeof(text));
    CHECK(!strcmp(text,"1234 lux"));
    CHECK(SettingsDisplay_Probe(9000)==APP_SETTINGS_BUSY);ambient.completed_id=19;ambient.driver.ready=1;
    CHECK(SettingsDisplay_Probe(9010)==APP_SETTINGS_OK);
    ambient.completed_id=0;CHECK(SettingsDisplay_Probe(10000)==APP_SETTINGS_BUSY);
    CHECK(SettingsDisplay_Probe(15000)==APP_SETTINGS_DEVICE_ERROR);
    return 0;
}
static uint32_t DebugMenu(void)
{
 Init();ui.open=1;ui.menu=11;char text[64];uint32_t count;
 const SettingItem *items=SettingsCatalog_Items(11,&count);CHECK(count==15);
 uint32_t old_starts=starts,old_stops=stops,old_clears=clears;
 for(uint32_t i=0;i<count;++i){ui.row=i;CHECK(items[i].kind==SETTING_READONLY);
  Key(BSP_BUTTON_ENTER,100,1000+i*200);CHECK(!ui.mode&&!app.pending_id);
  uint32_t request=99;CHECK(AppSettings_RequestChange(items[i].key,1,&request)==APP_SETTINGS_UNAVAILABLE&&request==99);
  AppSettings_Format(items[i].key,text,sizeof(text));CHECK(text[0]);}
 CHECK(starts==old_starts&&stops==old_stops&&clears==old_clears);
 ambient.driver.valid=0;ambient.driver.error=3;
 AppSettings_Format(SD_AMBIENT,text,sizeof(text));CHECK(strstr(text,"Unavailable")&&strstr(text,"3"));
 ambient.driver.valid=1;ambient.driver.millilux=12345;ambient.stale=1;
 AppSettings_Format(SD_AMBIENT,text,sizeof(text));CHECK(strstr(text,"12.345")&&strstr(text,"STALE"));
 ui.row=count;Key(BSP_BUTTON_ENTER,100,6000);CHECK(ui.menu==0);
 return 0;
}
static uint32_t RemoteBoundary(void)
{
 Init();uint32_t id=0,result;
 CHECK(AppSettings_RequestRemote(SK_BRIGHTNESS,35,&id)==APP_SETTINGS_BUSY&&!id);
 AppSettings_RemoteAllowed(1);CHECK(!AppSettings_RequestRemote(SK_BRIGHTNESS,35,&id));
 AppSettings_RemoteAllowed(0);AppSettings_Process(NULL,10);
 CHECK(AppSettings_GetResult(id,&result)&&result==APP_SETTINGS_BUSY&&pwm==25);
 AppSettings_RemoteAllowed(1);CHECK(!AppSettings_RequestRemote(SK_DASH_BIAS,-5,&id));AppSettings_Process(NULL,20);
 CHECK(AppSettings_Value(SK_DASH_BIAS)==-5&&pwm==25);CHECK(AppSettings_GetResult(id,&result)&&!result);
 uint32_t prior=id;
 CHECK(!AppSettings_RequestRemote(SK_DASH_BIAS,5,&id));AppSettings_Process(NULL,30);
 CHECK(AppSettings_GetResult(prior,&result)&&!result&&AppSettings_Value(SK_DASH_BIAS)==5);
 CHECK(AppSettings_RequestRemote(SK_DASH_BIAS,6,&id)==APP_SETTINGS_ARGUMENT);
 CHECK(!AppSettings_RequestChange(SK_DEFAULTS,1,&id));AppSettings_Process(NULL,40);CHECK(!AppSettings_Value(SK_DASH_BIAS));
 return 0;
}
uint32_t SettingsUI_TestMain(void)
;
static uint32_t EasterEgg(void)
{
 Init();ui.open=1;ui.menu=6;uint32_t count;
 SettingsSystem_Enter();SettingsCatalog_Items(6,&count);uint32_t initial=count;
 for(uint32_t i=0;i<19;i++)Key(BSP_BUTTON_UP,100,1000+i*200);
 SettingsCatalog_Items(6,&count);CHECK(count==initial);
 Key(BSP_BUTTON_UP,900,6000);SettingsCatalog_Items(6,&count);CHECK(count==initial);
 ui.motion.latched=1;Key(BSP_BUTTON_UP,100,7000);SettingsCatalog_Items(6,&count);CHECK(count==initial);ui.motion.latched=0;
 Key(BSP_BUTTON_UP,100,7300);const SettingItem *items=SettingsCatalog_Items(6,&count);CHECK(count==initial+1&&items[count-1].key==SK_DEV_MESSAGE);
 ui.row=count-1;Key(BSP_BUTTON_ENTER,100,7500);CHECK(ui.mode==4);
 Key(BSP_BUTTON_ENTER,100,7700);CHECK(!ui.mode&&ui.menu==6);
 uint32_t id;CHECK(AppSettings_RequestChange(SK_DEV_MESSAGE,0,&id)==APP_SETTINGS_UNAVAILABLE);
 CHECK(SettingsCatalog_Find(SK_REBOOT)->kind==SETTING_CONFIRM);
 return 0;
}
uint32_t SettingsUI_TestMain(void)
{if(RemoteBoundary()||Motion()||Scene()||PowerPreemptsSettings()||PowerReversal()||Preferences()||PowerSettings()||Navigation()||TapNavigation()||Connections()||AutoBrightness()||DebugMenu()||EasterEgg())return 1;return 0;}
