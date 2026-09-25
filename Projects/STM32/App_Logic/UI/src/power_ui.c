#include "Power_UI.h"
#include "Odometer_Guard.h"
#include "Ui_OffStages.h"
#include "Ignition_Session.h"
#include "PowerService.h"
#include "Power_View.h"
#include "Power_Scene.h"
#include "Button_Hints.h"
#include "App_Settings.h"
#include "Rider_Name.h"
#include "Scene_Transition.h"
#include "Page_Transition.h"
#include "SpeedHome_View.h"
#include "Graphics.h"
#include "Graphics_Background.h"
#include "Wallpaper.h"
#include "Wallpaper_Runtime.h"
#include "Settings_View.h"
#include "Settings_UI.h"
#include <stdio.h>
#include <string.h>
volatile PowerUIDiagnostics g_power_ui;
static IgnitionSession ride;
void PowerUI_SetDistanceScale(uint32_t q16){ride.distance_scale_q16=q16;}
static uint32_t previous=UINT32_MAX,last_minute=UINT32_MAX,cached_odo,odo_valid,frame_pending,frame_before;
static PowerScene animation;
static uint32_t wake_pending,wake_frame_before,wake_frame_armed,wake_epoch;
static uint32_t standby_settling,standby_fence;
static UiState *owner_state;

/* Fixed view is created once after the normal shell. No new scene objects on
 * each key cycle; per-ride counters have no nonvolatile side effects. */
uint32_t PowerUI_Init(void){g_power_ui.version=3;g_power_ui.magic=0x50554947U;return PowerView_Create(Graphics_GetScreen());}
uint32_t PowerUI_Update(UiState *s,const NoodoeSystemSnapshot *sys,const VehicleSnapshot *v,uint32_t now)
{
    owner_state=s;
    if(PowerService_RunRequired()){
        PowerService_Request(POWER_RUN);PowerService_Acknowledge(POWER_OWNER_GRAPHICS,0);
        g_power_ui.display_asleep=Graphics_SetDisplaySleeping(0)||Graphics_DisplaySleeping();
        g_power_ui.compose=!g_power_ui.display_asleep;
        if(g_power_ui.compose){(void)Graphics_SetContinuousRendering(1);(void)Graphics_SetBrightnessPercent(g_app_settings->effective_brightness);}
        return g_power_ui.compose;
    }
    uint32_t on=sys->ign_valid&&sys->ign_on;
    uint32_t hold=s->power==IGN_STOPPING&&s->off_phase==UI_OFF_DELAY;
    IgnitionSession_Tick(&ride,now,s->session_open,!!(v->valid_fields&VEHICLE_VALID_SPEED),v->speed_kph);
    OdometerStatus odo;OdometerGuard_GetStatus(&odo);
    if(odo.valid){cached_odo=odo.display;odo_valid=1;}
    uint32_t changed=previous!=s->power;
    if(changed){
        standby_settling=s->power==OFF_DISPLAY_HOLD;standby_fence=0;
        /* Initial OFF boot also keeps the lamp dark until its first scene.
         * Any interrupted wake must acknowledge a frame for the new state. */
        if(previous==UINT32_MAX)wake_pending=1;
        if(wake_pending){wake_frame_armed=0;wake_epoch=s->epoch;}
        if(previous==IGN_ON||previous==UINT32_MAX){ScenePose visible;SpeedHome_GetScene(&visible);PowerScene_Seed(&animation,&visible);}
        if(s->power==IGN_STOPPING)PowerScene_HoldRing(&animation);
        if(s->power==IGN_STARTING&&s->welcome_active){
            /* The panel was dark. Prepare a fully hidden shell behind it. */
            ScenePose hidden={.progress=1024U,.footer_y=100,.clock_y=-100};
            PowerScene_Seed(&animation,&hidden);wake_pending=1;wake_frame_armed=0;wake_epoch=s->epoch;
        }
        /* Backlight-only standby keeps the panel running. Still wait for a
         * fresh ON frame before lighting it, without replaying Welcome. */
        if(s->power==IGN_STARTING&&previous==IGN_OFF_AWAKE){
            wake_pending=1;wake_frame_armed=0;wake_epoch=s->epoch;
        }
        if(Ui_OffStages_IsOff(s->power)){wake_pending=0;wake_frame_armed=0;}
        previous=s->power;
        frame_pending=1;frame_before=g_graphics.render_count;}
    uint32_t policy=hold||s->power==IGN_ON||s->power==IGN_STARTING||s->power==UI_BOOT?POWER_RUN:POWER_ECONOMY;
    /* The first two OFF stages retain H4 UART/BT. Only the separately enabled
     * final stage asks all owners to quiesce before coordinated MCU STOP. */
    uint32_t standby=s->power==IGN_OFF_AWAKE;
    if(Ui_OffStages_IsOff(s->power))policy=s->power==OFF_DEEP_SLEEP?POWER_DEEP:POWER_DISPLAY_SLEEP;
    PowerService_Request(policy);
    g_power_ui.state=s->power;g_power_ui.policy=policy;g_power_ui.entered_ms=s->entered_ms;
    g_power_ui.welcome_active=s->welcome_active;g_power_ui.wake_pending=wake_pending;
    g_power_ui.ride_ms=ride.ride_ms;g_power_ui.distance_mm=ride.distance_mm;
    g_power_ui.off_phase=s->off_phase;g_power_ui.off_phase_ms=s->off_phase_ms;
    g_power_ui.session_open=s->session_open;g_power_ui.completed=ride.completed;
    g_power_ui.finished_ride_ms=ride.finished_ride_ms;g_power_ui.finished_distance_mm=ride.finished_distance_mm;
    PowerService_Acknowledge(POWER_OWNER_GRAPHICS,0);
    uint32_t minute=sys->clock_valid?sys->hour*60U+sys->minute:UINT32_MAX;
    if(standby&&minute!=last_minute&&!frame_pending){frame_pending=1;frame_before=g_graphics.render_count;}
    last_minute=minute;
    /* AWAKE means backlight OFF, panel/EVE scanout ON. The GPU keeps displaying
     * its retained clock/ODO frame without MCU rendering between minutes.
     * Hardware display sleep belongs only to the later SLEEPING state. */
    if(Ui_OffStages_IsOff(s->power)){
        if(Graphics_GetBrightnessPercent())(void)Graphics_SetBrightnessPercent(0);
        (void)Graphics_SetContinuousRendering(0);
    }
    uint32_t want_sleep=s->power==OFF_BT_HOLD||s->power==OFF_DEEP_SLEEP;
    uint32_t result=Graphics_SetDisplaySleeping(want_sleep);
    if(result>1U)g_power_ui.error=result;
    g_power_ui.display_asleep=Graphics_DisplaySleeping();
    WallpaperRuntime_HoldShutdown(hold||(Ui_OffStages_IsOff(s->power)&&!standby_settling));
    if(want_sleep){
        /* Transitional unavailability is not an acknowledgement of sleep. */
        g_power_ui.display_asleep=!result&&Graphics_DisplaySleeping();
        PowerService_Acknowledge(POWER_OWNER_GRAPHICS,g_power_ui.display_asleep);g_power_ui.compose=0;return 0;}
    if(result||g_power_ui.display_asleep){g_power_ui.compose=0;return 0;}
    /* Freeze the entire last frame, including shade, for the provisional second.
     * No page/source/shell updates, power reduction or ride closure occur.
     * Releasing the hold on either ON or confirmed OFF resumes current pose. */
    if(hold){(void)Graphics_SetContinuousRendering(0);g_power_ui.compose=0;return 0;}
    /* Arm only AFTER panel/EVE wake completion and before a freshly composed
     * frame. Submissions made before wake or the OFF hold never qualify. */
    if(wake_pending&&!wake_frame_armed){wake_frame_before=g_graphics.render_count;wake_frame_armed=1;}
    if(standby){
        /* Keep the lamp OFF while preparing the configured standby photo.
         * OFF timers continue from their original entry; only rendering waits
         * for the final upload/blend and a subsequent physical frame swap. */
        if(standby_settling){
            if(!changed&&!standby_fence&&GraphicsBackground_Settled()){
                standby_fence=1;frame_before=g_graphics.render_count;
            }
            (void)Graphics_SetContinuousRendering(1);g_power_ui.compose=1;return 1;
        }
        /* A submitted DL is not yet visible. Wait without recomposing until
         * DLSWAP acknowledges it; retries are allowed only before submission. */
        g_power_ui.compose=frame_pending&&g_graphics.render_count==frame_before;
        if(g_power_ui.compose)(void)Graphics_Invalidate();
        return g_power_ui.compose;
    }
    uint32_t active=1;
    /* Wake settling can finish several calls after the state change. Always
     * restore this policy once awake; the setter ignores an unchanged value. */
    (void)Graphics_SetContinuousRendering(active);
    if(changed||frame_pending)(void)Graphics_Invalidate();
    g_power_ui.compose=active||frame_pending;
    return g_power_ui.compose;
}
/* Preserve the last known ODO across UART shutdown. Never manufacture a
 * reading on an unknown boot or promote a stale speed to valid. */
void PowerUI_Input(SpeedHomeInput *in)
{
    in->session_valid=ride.on&&ride.have_speed;
    in->session_peak_kph=ride.peak_kph;
    in->session_generation=ride.completed;
    in->session_average_kph10=ride.known_ms?(uint32_t)(ride.speed_ms2*5U/ride.known_ms):ride.speed*10U;
    if(g_power_ui.state==IGN_ON||g_power_ui.state==IGN_STARTING)return;
    in->odometer_km=cached_odo;
    if(odo_valid)in->valid_mask|=VEHICLE_VALID_ODOMETER;
}
/* Off exits ONLY the speed ring. Clock/ODO stay fixed. Starting uses the same
 * reversible shell pose as settings, bringing footer UP from below. */
void PowerUI_Render(UiState *s,const SpeedHomeModel *m,UiDashboardPresentation *footer,const UiDashboardMaintenance *maintenance,uint32_t now)
{
    if(PowerService_RunRequired())return;
    if(s->power==IGN_ON){PowerViewModel hidden={0};PowerView_Render(&hidden);
        if(!wake_pending&&Graphics_GetBrightnessPercent()!=g_app_settings->effective_brightness)
            (void)Graphics_SetBrightnessPercent(g_app_settings->effective_brightness);
        return;}
    uint32_t dt=s->display_wait?0U:now-s->entered_ms,q=1024U;
    uint32_t welcome=s->power==IGN_STARTING&&s->welcome_active&&dt<s->config.welcome_ms;
    if(s->power==IGN_STOPPING&&s->off_phase==UI_OFF_DELAY)
        SceneTransition_Request(&animation.shell,0,now);
    else PowerScene_Request(&animation,s->power==IGN_STARTING?welcome:1U,welcome,now);
    ScenePose pose;PowerScene_Step(&animation,now,&pose);q=pose.progress;
    g_power_ui.ring_progress=q;g_power_ui.shell_progress=animation.shell.value;
    pose.progress=(s->power==IGN_STOPPING&&s->off_phase==UI_OFF_DELAY)?q:(q?q:1U); /* Non-ON pages hide center independently of ring alpha. */
    ButtonHints_Update(NULL,0,0,now);SettingsView_Hide();
    /* The background owner retains selected photo and fixed OFF shade.
     * Clock/ODO masks are owned by the common background renderer. */
    /* Standby displays ODO only; this local presentation does not change the
     * rider's selected footer or its reserve/maintenance domain state. */
    if(s->power!=IGN_STARTING){footer->footer=UI_ODO;strcpy(footer->footer_title,"ODO");
        if(odo_valid){uint64_t value=m->units?(uint64_t)cached_odo*1000000U/1609344U:cached_odo;
            snprintf(footer->footer_value,sizeof(footer->footer_value),"%lu",(unsigned long)value);}
        else strcpy(footer->footer_value,"------");
        SpeedHome_Render(m,footer,now);}
    SpeedHome_ApplyScene(&pose,0);
    uint32_t light=wake_pending||Ui_OffStages_IsOff(s->power)?0U:g_app_settings->effective_brightness;
    if(Graphics_GetBrightnessPercent()!=light)(void)Graphics_SetBrightnessPercent(light);
    PowerViewModel out={0};
    if(s->power==IGN_STARTING&&s->welcome_active){out.kind=1;out.alpha=255U*q/1024U;
        /* Copy, not a borrowed phone packet: deferred rendering retains one
         * immutable frame model. Setting a name never starts Welcome itself. */
        _Static_assert(POWER_VIEW_NAME_CAPACITY>=RIDER_NAME_CAPACITY,"Welcome name capacity");
        (void)RiderName_Get(out.rider_name,sizeof(out.rider_name));}
    if(s->power==IGN_STOPPING&&s->off_phase==UI_OFF_SUMMARY){
        /* Only the committed snapshot is shown. A provisional OFF never
         * exposes or clears the active ride; the summary cannot drift. */
        uint64_t unit=m->units?1609344U:1000000U,tenths=ride.finished_distance_mm*10U/unit;
        uint32_t age=now-s->off_phase_ms;
        uint32_t ease=PageTransition_Ease(age>240U?240U:age);
        out.kind=2;out.alpha=255U*ease/1024U;out.offset_y=(int32_t)(24U*(1024U-ease)/1024U);
        snprintf(out.distance,sizeof(out.distance),"%lu.%lu",(unsigned long)(tenths/10U),(unsigned long)(tenths%10U));
        strcpy(out.unit,m->units?"mi":"km");
        uint64_t minutes=ride.finished_ride_ms/60000U;
        snprintf(out.ride,sizeof(out.ride),"%lu:%02lu",(unsigned long)(minutes/60U),(unsigned long)(minutes%60U));
        /* Natural percentage in its own fixed row; unknown is never0%. */
        if(maintenance->valid_mask&1U)snprintf(out.oil,sizeof(out.oil),"%lu",
            (unsigned long)((maintenance->remaining_permille[0]>1000U?1000U:maintenance->remaining_permille[0])/10U));
        else strcpy(out.oil,"--");
        g_power_ui.summary_oil_valid=maintenance->valid_mask&1U;
        g_power_ui.summary_oil_permille=maintenance->remaining_permille[0];
    }
    PowerView_Render(&out);
}
uint32_t PowerUI_GraphicsDue(void){return g_power_ui.compose&&!g_power_ui.display_asleep;}
/* A CPU submission is not scanout. Keep the lamp off through panel settling
 * and REG_DLSWAP, then start Welcome's timer only when its frame is visible. */
void PowerUI_AfterGraphics(uint32_t now)
{
    if(frame_pending&&(!standby_settling||standby_fence)&&Graphics_FramePresentedAfter(frame_before)){
        frame_pending=0;
        if(owner_state&&owner_state->power==IGN_OFF_AWAKE){
            standby_settling=0;g_power_ui.compose=0;(void)Graphics_SetContinuousRendering(0);
        }
    }
    if(wake_pending&&wake_frame_armed&&owner_state&&owner_state->epoch==wake_epoch&&
       g_power_ui.compose&&Graphics_FramePresentedAfter(wake_frame_before)){
        wake_pending=0;
        wake_frame_armed=0;g_power_ui.wake_pending=0;
        UiEvent ready={UI_EVT_DISPLAY_READY,now,wake_epoch,0,0,0};Ui_Dispatch(owner_state,&ready);
        (void)Graphics_SetBrightnessPercent(g_app_settings->effective_brightness);
    }
}
uint32_t PowerUI_WaitMs(void)
{
    if(g_power_ui.state==IGN_OFF_AWAKE)return frame_pending?5U:1000U;
    return g_power_ui.compose?5U:Ui_OffStages_IsOff(g_power_ui.state)?1000U:100U;
}
