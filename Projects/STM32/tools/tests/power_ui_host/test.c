#include "Power_UI.h"
#include "Ignition_Session.h"
#include "platform.h"
#include <string.h>
uint32_t assertions;
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
FakeGraphics g_graphics;
FakeBackground g_background;
static FakeSettings settings={25};FakeSettings *g_app_settings=&settings;
static ScenePose pose={.ring_radius=240,.ring_alpha=255};
static PowerViewModel shown;
uint32_t RiderName_Get(char *out,uint32_t capacity){if(capacity<5)return 0;memcpy(out,"Alex",5);return 1;}
static uint32_t asleep,light,scanout=1,submit=1,starts,ends;
static uint32_t time_now,wake_delay,wake_started,waking;
static uint32_t background_held;
static uint32_t clock_valid,clock_minute;
static uint32_t sleep_requests;
void WallpaperRuntime_HoldShutdown(uint32_t hold){background_held=hold;}
static UiState state;
static SpeedHomeModel model;
static UiDashboardPresentation footer;
static UiDashboardMaintenance maintenance;
uint32_t PowerService_RunRequired(void){return 0;}
void PowerService_Request(uint32_t p){(void)p;}
void PowerService_Acknowledge(uint32_t a,uint32_t b){(void)a;(void)b;}
uint32_t Graphics_SetDisplaySleeping(uint32_t s){
    if(s){++sleep_requests;asleep=1;light=0;waking=0;return 0;}
    if(asleep&&wake_delay){if(!waking){waking=1;wake_started=time_now;}
        if(time_now-wake_started<wake_delay)return 1;}
    asleep=0;waking=0;return 0;}
uint32_t Graphics_DisplaySleeping(void){return asleep;}
uint32_t Graphics_SetContinuousRendering(uint32_t a){(void)a;return 0;}
uint32_t Graphics_Invalidate(void){return 0;}
uint32_t Graphics_GetBrightnessPercent(void){return light;}
uint32_t Graphics_SetBrightnessPercent(uint32_t p){light=p;return 0;}
uint32_t Graphics_FramePresentedAfter(uint32_t f){return scanout&&g_graphics.render_count!=f;}
void *Graphics_GetScreen(void){return 0;}
void SpeedHome_GetScene(ScenePose *p){*p=pose;}
void SpeedHome_ApplyScene(const ScenePose *p,uint32_t q){pose=*p;(void)q;}
void SpeedHome_Render(const SpeedHomeModel *m,const UiDashboardPresentation *p,uint32_t n){(void)m;(void)p;(void)n;}
void ButtonHints_Update(const void *p,uint32_t s,uint32_t a,uint32_t n){(void)p;(void)s;(void)a;(void)n;}
void SettingsView_Hide(void){}
uint32_t GraphicsBackground_SetImage(const void *p){(void)p;return 1;}
uint32_t GraphicsBackground_SetCenterBrightness(uint32_t value){return value==100U;}
void GraphicsBackground_Process(void){}
uint32_t GraphicsBackground_Settled(void){return !g_background.loading&&!g_background.phase&&g_background.center_percent==g_background.display_percent;}
uint32_t PowerView_Create(lv_obj_t *p){(void)p;return 1;}
void PowerView_Render(const PowerViewModel *p){shown=*p;}
/* Same ordering as ProductUI/default task, with scanout independently delayed
 * from a successful CPU submission. No test mutates a private UiState phase. */
static void Step(uint32_t now,uint32_t on)
{
    time_now=now;
    NoodoeSystemSnapshot sys={.ign_valid=1,.ign_on=on,.clock_valid=clock_valid,.minute=clock_minute};
    VehicleSnapshot vehicle={.valid_fields=3,.speed_kph=36,.odometer_km=36475};
    state.display_asleep=asleep;
    UiEvent ign={UI_EVT_IGN,now,on,0,0,0},tick={UI_EVT_TICK,now,0,0,0,0};
    Ui_Dispatch(&state,&ign);Ui_Dispatch(&state,&tick);
    UiEffect effect;while(Ui_TakeEffect(&state,&effect)){starts+=effect.kind==UI_FX_SESSION_START;ends+=effect.kind==UI_FX_SESSION_END;}
    if(PowerUI_Update(&state,&sys,&vehicle,now)){
        if(state.power==IGN_ON)pose=(ScenePose){.ring_radius=240,.ring_alpha=255};
        PowerUI_Render(&state,&model,&footer,&maintenance,now);
        if(submit)++g_graphics.render_count;
    }
    PowerUI_AfterGraphics(now);
}
uint32_t TestMain(void)
{
    /* The ride summary uses the same learned Q16 scale as trip records.
     * Exact36km/h for60s at0.9375 must retain562.5m, independent of tick rate. */
    IgnitionSession calibrated={.distance_scale_q16=61440};
    for(uint32_t t=0;t<=60000;t+=10)IgnitionSession_Tick(&calibrated,t,1,1,36);
    CHECK(calibrated.distance_mm==562500);
    IgnitionSession_Tick(&calibrated,60000,0,1,36);
    CHECK(calibrated.finished_distance_mm==562500&&calibrated.finished_ride_ms==60000);
    CHECK(!calibrated.distance_mm&&calibrated.distance_scale_q16==61440);
    Ui_Init(&state,0,0);PowerUI_Init();
    Step(0,1);CHECK(shown.kind==1&&!strcmp(shown.rider_name,"Alex"));
    for(uint32_t t=10;t<=3000;t+=10)Step(t,1);
    CHECK(state.power==IGN_ON&&starts==1&&!ends);
    uint32_t frames=g_graphics.render_count;
    state.dashboard.card=UI_MUSIC;state.dashboard.selection=1;
    Step(3100,0);CHECK(pose.ring_alpha==255&&!shown.kind&&!g_power_ui.compose&&background_held);
    Step(4099,0);CHECK(pose.ring_alpha==255&&state.off_phase==UI_OFF_DELAY&&!g_power_ui.compose&&background_held);
    CHECK(g_graphics.render_count==frames&&state.session_open&&!ends);
    CHECK(g_power_ui.policy==POWER_RUN);
    /* Cancellation at999ms keeps the same ride and visible pose. */
    Step(4099,1);CHECK(state.session_open&&!ends&&starts==1&&!background_held&&!shown.kind);
    Step(4500,1);CHECK(state.power==IGN_ON);
    Step(4600,0);frames=g_graphics.render_count;Step(5599,0);
    CHECK(g_graphics.render_count==frames&&state.session_open&&!ends&&background_held);
    /* Even a stalled renderer cannot delay the official1s session end. */
    submit=0;scanout=0;Step(5600,0);
    CHECK(state.off_phase==UI_OFF_SUMMARY&&state.off_phase_ms==5600&&!state.session_open);
    CHECK(ends==1&&g_power_ui.completed==1&&shown.kind==2&&!background_held&&g_power_ui.policy==POWER_ECONOMY);
    CHECK(shown.alpha==0&&shown.offset_y==24);
    CHECK(!g_power_ui.ride_ms&&!g_power_ui.distance_mm&&g_power_ui.finished_ride_ms==5600);
    CHECK(!strcmp(shown.ride,"0:00")&&!strcmp(shown.unit,"km")&&strchr(shown.distance,'.'));
    CHECK(!strcmp(shown.oil,"--"));
    maintenance.valid_mask=1;maintenance.remaining_permille[0]=870;Step(5601,0);CHECK(!strcmp(shown.oil,"87"));
    Step(5720,0);CHECK(shown.alpha>100&&shown.alpha<155&&shown.offset_y==12);
    Step(5800,0);uint32_t partial=g_power_ui.ring_progress;
    CHECK(partial==512&&shown.alpha>0); /* Ring exit and summary overlap. */
    /* ON after commitment starts a new ride while preserving the ring pose. */
    Step(5800,1);CHECK(g_power_ui.ring_progress==partial&&!shown.kind&&starts==2&&ends==1);
    Step(5900,1);partial=g_power_ui.ring_progress;CHECK(partial>0&&partial<512);
    Step(5900,0);Step(6899,0);CHECK(g_power_ui.ring_progress==partial&&ends==1&&background_held);
    Step(6900,0);CHECK(g_power_ui.ring_progress==partial&&ends==2&&g_power_ui.completed==2);
    CHECK(shown.kind==2&&shown.alpha==0&&g_power_ui.finished_ride_ms==1100);
    uint64_t frozen=g_power_ui.finished_distance_mm;
    submit=1;scanout=1;Step(7020,0);CHECK(shown.alpha>100&&shown.alpha<155&&shown.offset_y==12);
    Step(7140,0);CHECK(shown.alpha==255&&shown.offset_y==0);
    Step(7300,0);CHECK(g_power_ui.ring_progress==1024&&g_power_ui.finished_distance_mm==frozen&&g_power_ui.completed==2);
    Step(11899,0);CHECK(shown.kind==2);
    Step(11900,0);CHECK(state.power==IGN_OFF_AWAKE&&!shown.kind&&!light&&!asleep&&!sleep_requests);
    CHECK(g_power_ui.policy==POWER_DISPLAY_SLEEP&&!background_held&&PowerUI_WaitMs()==5);
    frames=g_graphics.render_count;
    g_background.center_percent=20;g_background.display_percent=30;submit=0;
    Step(11901,0);CHECK(g_power_ui.compose&&!asleep&&!light);
    g_background.display_percent=20;Step(11902,0);CHECK(g_power_ui.compose);
    submit=1;Step(11903,0);Step(11904,0);CHECK(!g_power_ui.compose&&frames+1==g_graphics.render_count&&background_held);
    /* A changed minute submits once, waits for actual swap, keeps scanning.
     * Stalled scanout and active background work cannot create extra frames. */
    frames=g_graphics.render_count;clock_valid=1;clock_minute=1;scanout=0;
    Step(11905,0);CHECK(!light&&!asleep&&g_graphics.render_count==frames+1);
    Step(11906,0);Step(11907,0);CHECK(!g_power_ui.compose&&g_graphics.render_count==frames+1);
    scanout=1;Step(11908,0);CHECK(!asleep&&!light&&!sleep_requests);
    Step(11909,0);CHECK(g_graphics.render_count==frames+1);
    Step(11910,1);CHECK(starts==3&&!state.welcome_active&&!shown.kind&&g_power_ui.ride_ms==0);
    CHECK(state.startup_sweep&&state.dashboard.card==UI_MUSIC&&state.dashboard.selection==1);
    Step(12310,1);Step(12400,0);Step(13400,0);Step(18400,0);
    CHECK(state.power==IGN_OFF_AWAKE&&!light&&!asleep);
    state.config.standby_ms=100;state.config.bt_retention_ms=100;
    Step(18499,0);CHECK(state.power==IGN_OFF_AWAKE&&!light&&!asleep&&!sleep_requests);
    Step(18500,0);CHECK(state.power==IGN_OFF_SLEEPING&&asleep&&!light&&g_power_ui.policy==POWER_DISPLAY_SLEEP);
    Step(18501,0);Step(18601,0);CHECK(state.power==OFF_DEEP_SLEEP&&g_power_ui.policy==POWER_DEEP&&asleep&&!light);
    /* Slow hardware and delayed scanout must not consume Welcome behind a
     * dark panel, nor illuminate the old scene after two CPU submissions. */
    wake_delay=3000;scanout=0;Step(19000,1);Step(21999,1);
    CHECK(state.display_wait&&state.power==IGN_STARTING&&!light&&asleep);
    Step(22000,1);Step(22010,1);Step(23000,1);
    CHECK(!asleep&&!light&&state.display_wait&&shown.kind==1&&shown.alpha==255);
    scanout=1;Step(23001,1);CHECK(!state.display_wait&&light==25&&state.entered_ms==23001);
    Step(25400,1);CHECK(state.power==IGN_STARTING);Step(25401,1);CHECK(state.power==IGN_ON);
    /* Repeated sleep/wake and an OFF reversal during wake cannot leak an old
     * frame acknowledgement into the new epoch. */
    for(uint32_t i=0;i<100;++i){uint32_t t=30000+i*20000;
        Step(t,0);Step(t+1000,0);Step(t+6000,0);Step(t+6100,0);CHECK(asleep&&!light);
        Step(t+7000,1);CHECK(asleep&&state.display_wait);
        Step(t+7050,0);Step(t+8050,0);CHECK(!light);
        Step(t+8100,1);CHECK(state.display_wait&&state.welcome_active);
        Step(t+10000,1);CHECK(!state.display_wait&&!asleep&&light==25);
        Step(t+12400,1);CHECK(state.power==IGN_ON);
    }
    /*100 distinct clock minutes: one frame each, no panel sleep, brightness
     * leak or animation/upload-driven redraw between them. */
    state.config.standby_ms=7200000;wake_delay=150;
    Step(2100000,0);Step(2101000,0);Step(2106000,0);Step(2106001,0);CHECK(!asleep&&!light);
    uint32_t prior_sleep_requests=sleep_requests;
    for(uint32_t i=0;i<100;++i){uint32_t t=2160000+i*60000;
        clock_minute=(i+2)%60;frames=g_graphics.render_count;
        scanout=0;Step(t,0);CHECK(!asleep&&!light&&g_graphics.render_count==frames+1);
        Step(t+149,0);CHECK(!asleep&&!light);
        Step(t+150,0);CHECK(!asleep&&!light&&g_graphics.render_count==frames+1);
        Step(t+155,0);CHECK(!g_power_ui.compose&&g_graphics.render_count==frames+1);
        scanout=1;Step(t+160,0);Step(t+59000,0);
        CHECK(!asleep&&!light&&g_graphics.render_count==frames+1&&background_held&&sleep_requests==prior_sleep_requests);
    }
    /* IGN during a pending minute swap needs a fresh ON frame before PWM,
     * without replaying Welcome or accepting the old minute frame. */
    clock_minute=59;Step(8200000,0);CHECK(!asleep&&!light);
    scanout=0;Step(8200050,1);CHECK(state.display_wait&&!state.welcome_active&&!light);
    Step(8200150,1);CHECK(!asleep&&state.display_wait&&!light&&!shown.kind);
    scanout=1;Step(8200155,1);CHECK(!state.display_wait&&light==25);
    return 0;
}
