#include "ProductUI.h"
#include "BootStore.h"
#include "Rollback_View.h"
#include "Install_View.h"
#include "RuntimeUpdate.h"
#include "PowerService.h"
#include "BSP_Buttons.h"
#include "BSP_InputMode.h"
#include "InputMode.h"
#include "Power_UI.h"
#include "Product_Preview.h"
#include "Page_Preview.h"
#include "Product_Input.h"
#include "Dashboard_Pages.h"
#include "Development_Data.h"
#include "Dashboard_PagesView.h"
#include "Toast_View.h"
#include "Wallpaper.h"
#include "Notification_Preview.h"
#include "ButtonEvents.h"
#include "ButtonFeedback.h"
#include "MediaControl.h"
#include "Wallpaper_Runtime.h"
#include "Ui_Home.h"
#include "Ui_Theme.h"
#include "Fuel_Policy.h"
#include "Phone_Indicators.h"
#include "PhoneVisual.h"
#include "Product_StatusIconsView.h"
#include "ScreenWarningOverlay.h"
#include "ScreenWarning_View.h"
#include "Product_Theme.h"
#include "AmbientService.h"
#include "Product_ModeStrip.h"
#include "Settings_QuickPresenter.h"
#include "BSP_Calendar.h"
#include "Ui_State.h"
#include "SpeedHome_Model.h"
#include "SpeedHome_Startup.h"
#include "SpeedHome_View.h"
#include "Graphics.h"
#include "NoodoeRuntime.h"
#include "SettingsService.h"
#include "App_Settings.h"
#include "Odometer_Guard.h"
#include "Settings_Remote.h"
#include "Odometer_View.h"
#include "App_Persistence.h"
#include "Settings_UI.h"
#include "Settings_View.h"
#include "Settings_TestPort.h"
#include "OilUsageService.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"

static UiState state;
static NotificationPreview notification_preview;
static UiThemeState theme;
static FuelPolicy fuel_policy;
static uint32_t fuel_session,previous_card=UINT32_MAX;
static SpeedHomeModel model;
SpeedHomeStartup g_speed_startup;
static UiDashboardDistances published_distances;
static UiDashboardMaintenance published_maintenance;
static UiDashboardPresentation presentation;
static DashboardPage page;
TripComputer g_product_trips;
static PhoneContent phone_cache;
static PhoneContentSlot current_phone;
static PhoneTrail trail;
static PhoneCallsSnapshot calls_mailbox;
static volatile uint32_t call_target;
extern void UiCalls_Update(UiState*,const PhoneCallsSnapshot*,uint32_t);
uint32_t ProductUI_RideSession(void){return trail.session;}
/* One phone; legacy selection API accepts slot0 only. */
#define selected_phone 0U
/* Pure data cache is single UI-owner; task publishers copy only under a
 * bounded critical section. No caller receives mutable cache pointers. */
volatile ProductUI_Diagnostics g_product_ui;
/* Share only opaque context/availability with generic visual feedback. The
 * middleware never decides which page/menu/media action an event operates. */
static void FeedbackContext(void)
{
    uint32_t settings_open=g_settings_ui&&g_settings_ui->open;
    ButtonFeedback_SetContext(settings_open?100+g_settings_ui->menu:state.dashboard.card+1U,state.context_token,
        g_input_mode.allowed&&state.power==UI_RUNNING&&!state.warning&&!state.pending_id&&(!state.menu||settings_open)&&!state.modal);
}

/* Sample before both input dispatch and repeat processing. A selector change
 * cancels existing gestures without delivering a synthetic release/action. */
static void InputModeRefresh(void)
{
    uint32_t cancel=InputMode_Sample(BSP_InputMode_Read(),state.ign_on,BSP_Buttons_GetPressedMask(),HAL_GetTick());
    if(cancel){
        state.pressed_buttons&=~cancel;state.consumed_buttons|=cancel;
        if(g_settings_ui){g_settings_ui->pressed&=~cancel;g_settings_ui->repeated&=~cancel;}
    }
    FeedbackContext();
}

/* UI effects enqueue bounded media requests or update local trip state.
 * Unsupported SAVE/OFF still report failure, never a false power-cut permit.
 * The I/O service owns Bluetooth transmission; no waiting occurs here. */
static void Effects(uint32_t now)
{
    UiEffect f;
    while(Ui_TakeEffect(&state,&f)){
        ++g_product_ui.effects_observed;
        if(f.epoch!=state.epoch)continue;
        if(f.kind==UI_FX_SESSION_START||f.kind==UI_FX_SESSION_END){
            /* PowerUI owns a separate per-IGN ride; persistent trips remain. */
            if(f.kind==UI_FX_SESSION_START){PhoneTrail_Init(&trail);trail.session=now;if(!++fuel_session)++fuel_session;}
            if(f.kind==UI_FX_SESSION_END){uint32_t id;(void)RideStore_RequestCheckpoint(&id);OdometerService_Checkpoint();}
        }else if(f.kind==UI_FX_RESERVE_START||f.kind==UI_FX_RESERVE_CLEAR){
            uint32_t id;(void)RideStore_RequestCheckpoint(&id);
        }else if(f.kind==UI_FX_MEDIA){
            if(!DevelopmentData_MediaAction(selected_phone,f.arg,now)){
                uint32_t result=MediaControl_Request(selected_phone,f.arg,now);
                if(result)(void)ShowToastMessages(result==3?"Media command busy":"Phone not connected",2);
            }
        }else if(f.kind==UI_FX_CALL){
            if(state.power!=IGN_ON||!current_phone.connected||!PhoneCalls_Fresh(&state.calls.phone,now))continue;
            uint32_t words[5]={state.calls.phone.epoch,state.calls.phone.generation,f.value,f.arg,f.id};
            uint32_t r=MediaControl_Call(words,now);
            if(r)ShowToastMessages(r==3?"Command busy":"Phone not connected",2);
        }else if(f.kind==UI_FX_PHONE_REPLY){
            if(state.power!=IGN_ON||!state.speed_valid||state.speed_kph>50U||!current_phone.connected){state.notification_reply=0;continue;}
            const PhoneStatus *ps=&current_phone.status;const PhoneNotification *n=NULL;
            if(!f.arg){for(uint32_t i=0;i<ps->count;i++)if(ps->notifications[i].id==state.notification_id)n=&ps->notifications[i];
                if(!n||!n->replyable){ShowToastMessages("No reply action from app",2);continue;}
                if(!ps->reply_count){ShowToastMessages("Set quick replies in phone app",3);continue;}
                if(!ps->reply_key){ShowToastMessages("Replies loading",2);continue;}
                state.reply_id=n->id;state.reply_revision=n->revision;state.reply_config=ps->reply_revision;
                state.reply_count=ps->reply_count;state.reply_selection=0;state.notification_reply=1;
            }else{
                for(uint32_t i=0;i<ps->count;i++)if(ps->notifications[i].id==state.reply_id)n=&ps->notifications[i];
                if(state.reply_sending||!n||!n->replyable||n->revision!=state.reply_revision||ps->reply_revision!=state.reply_config||f.value>=ps->reply_count){ShowToastMessages("Message changed - open again",3);state.notification_reply=0;continue;}
                uint32_t words[5]={ps->connection_epoch,n->id,n->revision,ps->reply_revision,f.value};
                uint32_t r=MediaControl_Reply(words,now);
                if(r)ShowToastMessages(r==3?"Command busy":"Phone not connected",2);
                else {state.reply_sending=1;state.reply_sequence=g_media_control.last_sequence;ShowToastMessages("Sending reply...",2);}
            }
        }else if(f.kind==UI_FX_RESET_TRIP){
            uint32_t n=f.arg==UI_TRIP1?TRIP_A:f.arg==UI_TRIP2?TRIP_B:TRIP_COUNT;
            uint32_t ok=TripComputer_Reset(&g_product_trips,n);
            if(ok){uint32_t id;(void)RideStore_RequestCheckpoint(&id);}
            UiEvent done={UI_EVT_EFFECT_DONE,now,f.id,f.epoch,ok?0U:8U,0};Ui_Dispatch(&state,&done);
        }else if(f.kind!=UI_FX_SCREEN){
            ++g_product_ui.effects_unsupported;
            if(f.id==state.save_id||f.id==state.off_id||f.id==state.pending_id){
                UiEvent done={UI_EVT_EFFECT_DONE,now,f.id,f.epoch,8U,0};Ui_Dispatch(&state,&done);
            }
        }
    }
}

/* Preserve the source duration, but dispatch on the current UI clock: a
 * debounced event timestamp can precede the last rendered state tick. */
static void ProductButtonEvent(const ButtonEvent *event,void *context)
{(void)context;ProductUI_Button(event->button,event->type,event->duration_ms,HAL_GetTick());}

/* Allocation-free application model phase precedes resource I/O and views.
 * It is idempotent so a failed asset load cannot reset trips or leak objects. */
void ProductUI_InitModel(uint32_t now)
{
    static uint32_t initialized;if(initialized)return;initialized=1;(void)now;
    memset(&notification_preview,0,sizeof(notification_preview));ProductPreview_Init();PagePreview_Init();DevelopmentData_Init();TripComputer_Init(&g_product_trips);PhoneContent_Init(&phone_cache);PhoneTrail_Init(&trail);
    PopupNotifications_Init();Wallpaper_Init();
}

/* Initializes model and fixed view after Graphics_Init on the owner task.
 * Missing BT/ALS cannot disable the speed screen. Child cards are navigable
 * with explicit offline data. The OBD center card remains present offline. */
uint32_t ProductUI_Init(uint32_t now,uint32_t boot_held_mask)
{
    memset((void *)&g_product_ui,0,sizeof(g_product_ui));
    g_product_ui.magic=0x50554931U;g_product_ui.version=1U;g_product_ui.scope_speed_only=1U;
    memset(&published_distances,0,sizeof(published_distances));
    memset(&published_maintenance,0,sizeof(published_maintenance));
    ProductUI_InitModel(now);
    UiConfig c=Ui_DefaultConfig();
    c.card_mask=(1U<<UI_CARD_COUNT)-1U;c.footer_mask=(1U<<UI_FOOTER_COUNT)-1U;
    c.preferred_card=DATA_DEBUG?DATA_DEBUG_START_CARD:UI_BLANK;c.boot_held_mask=boot_held_mask;
    Ui_Init(&state,&c,now);SpeedHome_InitModel(&model,now,200U);
    BSP_InputMode_Init();InputMode_Init(boot_held_mask);
    SpeedHomeStartup_Init(&g_speed_startup);
    AppSettings *preferences=pvPortMalloc(sizeof(*preferences));SettingsUI *settings_ui=pvPortMalloc(sizeof(*settings_ui));
    if(!preferences||!settings_ui){g_product_ui.error=3;return 0;}
    AppSettings_Init(preferences);SettingsUI_Init(settings_ui,boot_held_mask);
    if(!SpeedHome_Create(Graphics_GetScreen())||!SettingsView_Create(Graphics_GetScreen())||!PowerUI_Init()||!RollbackView_Create(Graphics_GetScreen())||!InstallView_Create(Graphics_GetScreen())||!ScreenWarningView_Create(Graphics_GetScreen())||!OdometerView_Create(Graphics_GetScreen())){g_product_ui.error=1U;return 0U;}
    /* Observer callback alone produces product intents, not LVGL encoder twice. */
    (void)Graphics_SetInputEnabled(0U);(void)Graphics_SetTargetFPS(30U);
    (void)Graphics_SetContinuousRendering(1U);g_product_ui.ready=1U;
    if(!ButtonEvents_Subscribe(ProductButtonEvent,NULL)){g_product_ui.ready=0;g_product_ui.error=2;return 0;}
    return 1U;
}

/* Copy only the immutable domain snapshot under a bounded critical section.
 * No peripheral access, allocation or wait occurs; UI alone owns its state. */
uint32_t ProductUI_PublishDistances(const UiDashboardDistances *distances)
{
    if(!distances||!g_product_ui.ready)return 0U;
    taskENTER_CRITICAL();published_distances=*distances;
    published_distances.valid_mask&=((1U<<UI_FOOTER_COUNT)-1U)&~UI_BIT(UI_ODO);
    published_distances.distance_mm[UI_ODO]=0;taskEXIT_CRITICAL();return 1U;
}

/* Domain callers publish a combined remaining fraction without touching
 * renderer state. Invalid ratios clear validity rather than silently clamp. */
uint32_t ProductUI_PublishMaintenance(const UiDashboardMaintenance *maintenance)
{
    if(!maintenance||!g_product_ui.ready)return 0U;
    UiDashboardMaintenance next=*maintenance;next.valid_mask&=7U;next.days_valid_mask&=7U;next.oil_hours_valid=!!next.oil_hours_valid;
    for(uint32_t i=0;i<3U;++i)if(next.remaining_permille[i]>1000U)next.valid_mask&=~(1U<<i);
    taskENTER_CRITICAL();published_maintenance=next;taskEXIT_CRITICAL();return 1U;
}

/* The BSP event stream stays singly consumed by ButtonEvents. UI owns press
 * classification and boot-held suppression; task/main remains LCDTest only. */
void ProductUI_Button(uint32_t button,uint32_t event,uint32_t duration,uint32_t now)
{
    if(!g_product_ui.ready)return;
    if(PowerService_RunRequired())return; /* Storage owner handles the physical cancel hold. */
    GateJournalRecord result;
    if(BootStore_GetBootInfo(&result)&&(result.flags&GATE_F_RESULT_PENDING)){
        if(button==BSP_BUTTON_ENTER&&event==BSP_BUTTON_EVENT_SHORT_PRESS)(void)BootStore_RequestResultAck(result.transaction);
        return;
    }
    InputModeRefresh();
    uint32_t input=InputMode_Event(button,event);
    if(input!=INPUT_MODE_ACCEPT){
        if(button<3&&event==BSP_BUTTON_EVENT_RELEASE){
            uint32_t bit=1U<<button;state.blocked_buttons&=~bit;state.pressed_buttons&=~bit;
            if(g_settings_ui){g_settings_ui->blocked&=~bit;g_settings_ui->pressed&=~bit;}
        }
        /* The physical switch now has a persistent badge. Blocked presses
         * remain diagnostic events and never enqueue a transient toast. */
        return;
    }
    if(event==BSP_BUTTON_EVENT_PRESS)NotificationPreview_Input(&notification_preview);
    if(OdometerView_Button(button,event))return;
    if(ScreenWarningOverlay_Button(button,event)){
        uint32_t bit=1U<<button;state.pressed_buttons&=~bit;state.consumed_buttons|=bit;
        return;
    }
    if(SettingsUI_Button(&state,button,event,duration,now)||ProductInput_Dispatch(&state,button,event,duration,now)){
        ProductPreview_Cancel();PagePreview_Cancel();++g_product_ui.input_count;
    }
    FeedbackContext();
}

/* Read cached device service snapshots, dispatch priority facts before Tick,
 * then render. No synthetic speed is ever substituted for a missing UART.
 * seq odd/even protects SWD observations from being mistaken for atomic data. */
void ProductUI_Process(uint32_t now)
{
    if(!g_product_ui.ready)return;
    ++g_product_ui.seq;
    DevelopmentData_Poll(now);
    NoodoeSystemSnapshot sys={0};VehicleSnapshot vehicle={0};Settings_Values settings={0};
    uint32_t system_ready=NoodoeRuntime_GetSystem(&sys);
    uint32_t vehicle_ready=NoodoeRuntime_GetVehicle(&vehicle);
    (void)SettingsService_GetValues(&settings);
    state.display_asleep=Graphics_DisplaySleeping();
    state.config.standby_ms=(uint32_t)AppSettings_Value(SK_POWER)*60000U;
    state.config.bt_retention_ms=(uint32_t)AppSettings_Value(SK_BT_HOLD)*60000U;
    state.config.off_stage_mask=SettingsPower_EnabledMask();
    if(system_ready&&sys.ign_valid&&(uint32_t)(now-sys.power_ms)<=1000U){
        UiEvent e={UI_EVT_IGN,now,sys.ign_on,0,0,0};Ui_Dispatch(&state,&e);
    }
    InputModeRefresh();
    OdometerService_Tick(now,vehicle.telemetry_ms,vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_ODOMETER),
        vehicle.odometer_km,state.ign_on,vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_SPEED),vehicle.speed_kph);
    OdometerStatus odo;OdometerGuard_GetStatus(&odo);
    SettingsRemote_Tick(now,state.ign_on,vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_SPEED),vehicle.speed_kph);
    /* Observe even when provisional OFF freezes the entire previous frame.
     * Cancelling a sweep changes no visible pose until composition resumes. */
    SpeedHomeStartup_Ignition(&g_speed_startup,state.epoch,state.ign_on,state.startup_sweep);
    UiEvent speed={UI_EVT_SPEED,now,vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_SPEED),vehicle.speed_kph,0,0};
    Ui_Dispatch(&state,&speed);
    taskENTER_CRITICAL();
    PhoneContent_Links(&phone_cache,((sys.links&UI_LINK_PHONE1)?1U:0U));
    /* Explicit1/2 target selection survives an offline peer. Never redirect a
     * key intended for Phone2 to Phone1 merely because the former is offline. */
    current_phone=phone_cache.slots[selected_phone];
    taskEXIT_CRITICAL();
    if(previous_card!=state.dashboard.card){
        previous_card=state.dashboard.card;
        if(previous_card==UI_NOTIFICATIONS){taskENTER_CRITICAL();PhoneIndicators_MarkRead();taskEXIT_CRITICAL();}
    }
    DashboardPageFacts resolved={0};DevelopmentScratch development_scratch;
    DevelopmentData_SelectPhone(selected_phone);
    DevelopmentData_Resolve(&resolved,&current_phone,&development_scratch,now);
    UiEvent notifications={UI_EVT_PHONE_COUNT,now,current_phone.status.notifications_valid?current_phone.status.count:0U,0,0,0};
    Ui_Dispatch(&state,&notifications);
    /* Keep the message being read anchored to its ID when new arrivals reorder
     * the list. Never silently move a reply gesture to a different recipient. */
    if(state.dashboard.card==UI_NOTIFICATIONS){
        state.notification_open=1;
        if(state.notification_id){
            uint32_t found=0;
            for(uint32_t i=0;i<current_phone.status.count;i++)if(current_phone.status.notifications[i].id==state.notification_id){state.dashboard.selection=i;found=1;break;}
            if(!found){state.notification_reply=0;state.notification_id=0;state.dashboard.selection=0;}
        }else if(state.dashboard.selection<current_phone.status.count)state.notification_id=current_phone.status.notifications[state.dashboard.selection].id;
    }else state.notification_id=0;
    if(!current_phone.connected||state.reply_config!=current_phone.status.reply_revision)state.notification_reply=0;
    if(state.reply_sending&&g_media_control.last_sequence==state.reply_sequence&&g_media_control.last_result!=0xffffffffU){
        state.reply_sending=0;state.notification_reply=0;
        ShowToastMessages(g_media_control.last_result?"Reply not confirmed":"Reply handed to app",3);
    }
    UiEvent links={UI_EVT_LINKS,now,sys.links,0,0,0};Ui_Dispatch(&state,&links);
    UiEvent tick={UI_EVT_TICK,now,0,0,0,0};Ui_Dispatch(&state,&tick);Effects(now);
    PowerUI_SetDistanceScale(g_product_trips.distance_q16);
    uint32_t compose=PowerUI_Update(&state,&sys,&vehicle,now);
    FeedbackContext();
    AppPersistence_UI(now,&g_product_trips,&state);
    if(!compose)goto diagnostics;
    ScreenWarningOverlay_Process(now,state.power==IGN_ON&&!PowerService_RunRequired());
    if(state.power==IGN_ON){
        uint32_t action=FuelPolicy_Step(&fuel_policy,fuel_session,now,vehicle_ready&&
            !!(vehicle.valid_fields&(VEHICLE_VALID_FUEL_OBSERVED|VEHICLE_FUEL_MEASUREMENT_ERROR)),
            vehicle.telemetry_ms,vehicle.status_raw,state.reserve_active);
        if(action&(FUEL_START_RESERVE|FUEL_CLEAR_RESERVE)){UiEvent e={action&FUEL_START_RESERVE?UI_EVT_RESERVE_ENTER:UI_EVT_REFUEL,now,0,0,0,0};Ui_Dispatch(&state,&e);}
        uint32_t warning=action&255U;
        ScreenWarningState current;ScreenWarningOverlay_Get(&current);
        if(warning&&(!current.active||current.color!=0xFF3030U||warning==FUEL_CRITICAL))
            ScreenWarningOverlay(warning==FUEL_ERROR?1:0,warning==FUEL_CRITICAL?0xFF3030U:0xFF9800U,3,3,
                warning==FUEL_CRITICAL?"Fuel Level Critical":warning==FUEL_ERROR?"Fuel Measurement Error":"Low Fuel");
    }
    SettingsFacts settings_facts={.clock_valid=system_ready&&sys.clock_valid&&now-sys.clock_ms<=3000U,
        .odo_valid=odo.valid,.odo_km=odo.display,
        .links=sys.links,.phone=selected_phone,.utc={sys.year,sys.month,sys.day,sys.weekday,sys.hour,sys.minute,sys.second}};
    OilUsageSnapshot settings_usage;
    if(OilUsageService_Get(&settings_usage)){settings_facts.on_valid=1;settings_facts.on_ms=settings_usage.total_ms;}
    WallpaperState wallpaper;Wallpaper_GetState(&wallpaper);
    for(uint32_t i=0;i<WALLPAPER_PHOTO_COUNT;++i)if(BackgroundImage_Valid(&wallpaper.photos[i]))settings_facts.photo_mask|=1U<<i;
    AppSettings_Process(&settings_facts,now);SettingsUI_Process(&state,now,speed.a,speed.b);
    SettingsDisplay_Tick(now,state.power==IGN_ON&&!g_power_ui.wake_pending);
    SettingsTestPort_Process(now);FeedbackContext();
    SpeedHomeInput in={0};in.now_ms=now;
    in.valid_mask=vehicle_ready?vehicle.valid_fields:0U;in.speed_kph=vehicle.speed_kph;
    in.odometer_km=odo.display;if(odo.valid)in.valid_mask|=VEHICLE_VALID_ODOMETER;else in.valid_mask&=~VEHICLE_VALID_ODOMETER;in.units=AppSettings_Value(SK_UNITS);model.max_kph=AppSettings_Value(SK_SCALE);
    in.clock_valid=system_ready&&sys.clock_valid&&(uint32_t)(now-sys.clock_ms)<=3000U;
    in.hour=sys.hour;in.minute=sys.minute;

    BSP_CalendarDateTime utc={sys.year,sys.month,sys.day,sys.weekday,sys.hour,sys.minute,sys.second},local;
    uint32_t date=0,weekday=0;
    if(in.clock_valid&&BSP_Calendar_AddSeconds(&utc,AppSettings_Value(SK_ZONE)*60,&local)){
        date=local.year*10000U+local.month*100U+local.day;weekday=local.weekday;in.hour=local.hour;in.minute=local.minute;
    }
    Ambient_Snapshot ambient;AmbientService_GetSnapshot(&ambient);
    UiThemeConfig theme_config={(uint32_t)AppSettings_Value(SK_THEME),(uint32_t)AppSettings_Value(SK_THEME_SOURCE),
        (uint32_t)AppSettings_Value(SK_THEME_DAY),(uint32_t)AppSettings_Value(SK_THEME_NIGHT),
        (uint32_t)AppSettings_Value(SK_THEME_DARK_LUX),(uint32_t)AppSettings_Value(SK_THEME_LIGHT_LUX)};
    Theme_SetLight(UiTheme_Step(&theme,&theme_config,now,in.clock_valid,in.hour*60U+in.minute,
        ambient.driver.valid&&!ambient.stale,ambient.driver.millilux),now);Theme_Tick(now);
    PowerUI_Input(&in);SpeedHome_UpdateModel(&model,&in);
    uint32_t presented=SpeedHomeStartup_WaitsForFrame(&g_speed_startup)&&
        Graphics_FramePresentedAfter(g_speed_startup.frame_before);
    if(SpeedHomeStartup_Step(&g_speed_startup,now,
        state.power==IGN_ON&&!g_power_ui.wake_pending&&!Graphics_DisplaySleeping(),
        g_graphics.render_count,presented,!!(in.valid_mask&VEHICLE_VALID_SPEED),vehicle.telemetry_ms))
        SpeedHome_OverrideArc(&model,g_speed_startup.value,now);
    (void)TripComputer_SetStopSpeed(&g_product_trips,(uint32_t)AppSettings_Value(SK_TRIP_STOP_SPEED));
    TripComputer_Tick(&g_product_trips,now,system_ready&&sys.ign_valid&&sys.ign_on&&now-sys.power_ms<=1000U,
        model.speed_valid,vehicle.speed_kph,date);
    TripComputer_Odometer(&g_product_trips,OdometerService_TripValid()&&vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_ODOMETER),vehicle.odometer_km);
    if(TripComputer_Fuel(&g_product_trips,now,vehicle_ready&&!!(vehicle.valid_fields&VEHICLE_VALID_FUEL_OBSERVED),vehicle.fuel_observed)){
        /* Refuel-trip detection is separate from Reserve recovery. */
        uint32_t id;(void)RideStore_RequestCheckpoint(&id);
    }
    AppPersistence_UI(now,&g_product_trips,&state);
    GnssSnapshot gps={0};
    uint32_t gps_valid=state.session_open&&state.ign_on&&NoodoeRuntime_GetGnss(&gps)&&gps.source==GNSS_SOURCE_PHONE&&gps.valid&&!gps.stale&&(int32_t)(gps.fix.field_ms[0]-trail.session)>=0&&(gps.fix.fields&GNSS_VALID_POSITION);
    PhoneTrail_FeedHeading(&trail,gps.fix.field_ms[0],gps_valid,gps.fix.latitude_e7,gps.fix.longitude_e7,
        (gps.fix.fields&(GNSS_VALID_COURSE|GNSS_VALID_SPEED))==(GNSS_VALID_COURSE|GNSS_VALID_SPEED)&&gps.fix.speed_mm_s>=2000,
        gps.fix.course_mdeg);
    PhoneTrail_RenderStep(&trail,now);
    UiDashboardDistances distances;UiDashboardMaintenance maintenance;
    taskENTER_CRITICAL();distances=published_distances;maintenance=published_maintenance;taskEXIT_CRITICAL();
    UiDashboardDistances service_distances;UiDashboardMaintenance service_maintenance;uint32_t due;
    if(state.reserve_active&&g_app_persistence.ride_restored){distances.valid_mask|=UI_BIT(UI_RESV);distances.distance_mm[UI_RESV]=g_app_persistence.reserve_mm;}
    AppSettings_Maintenance(&service_distances,&service_maintenance,&due);
    for(uint32_t n=0;n<3;++n){uint32_t key=SK_OIL_DISTANCE+n*4,bit=1U<<n;
        if(!g_app_settings->origins[n].valid&&!AppSettings_Value(key)&&!AppSettings_Value(key+1)&&!AppSettings_Value(key+2))continue;
        uint32_t f=UI_OIL+n;distances.valid_mask=(distances.valid_mask&~UI_BIT(f))|(service_distances.valid_mask&UI_BIT(f));distances.distance_mm[f]=service_distances.distance_mm[f];
        maintenance.valid_mask=(maintenance.valid_mask&~bit)|(service_maintenance.valid_mask&bit);maintenance.remaining_permille[n]=service_maintenance.remaining_permille[n];
        maintenance.days_valid_mask=(maintenance.days_valid_mask&~bit)|(service_maintenance.days_valid_mask&bit);maintenance.elapsed_days[n]=service_maintenance.elapsed_days[n];
        if(!n){maintenance.oil_hours_valid=service_maintenance.oil_hours_valid;maintenance.oil_ignition_ms=service_maintenance.oil_ignition_ms;}
    }
    UiEvent service_event={UI_EVT_MAINTENANCE,now,due,0,0,0};Ui_Dispatch(&state,&service_event);
    for(uint32_t i=0;i<2U;++i){uint32_t f=i?UI_TRIP2:UI_TRIP1;
        if(g_product_trips.records[i].valid){distances.valid_mask|=UI_BIT(f);distances.distance_mm[f]=g_product_trips.records[i].distance_mm;}}
    UiDashboard_Present(&state,&distances,odo.display,model.odo_valid,
        model.units,model.speed_valid,&presentation);
    UiDashboard_PresentMaintenance(&maintenance,&presentation);
    if(odo.corrected&&presentation.footer==UI_ODO)strcpy(presentation.footer_title,"ODO*");
    (void)ProductPreview_Apply(&state,now,&presentation);
    SpeedHome_Render(&model,&presentation,now);
    {PhoneCallsSnapshot calls;taskENTER_CRITICAL();calls=calls_mailbox;taskEXIT_CRITICAL();
     if(!current_phone.connected)memset(&calls,0,sizeof(calls));
     UiCalls_Update(&state,&calls,state.power==IGN_ON&&!state.menu&&!state.modal&&!state.warning&&!g_settings_ui->open&&!PowerService_RunRequired());
     call_target=PhoneCalls_Target(&state.calls);}
    {ScreenWarningState warning;ScreenWarningOverlay_Get(&warning);
     NotificationPreview_Step(&notification_preview,&state,&current_phone,
        AppSettings_Value(SK_NOTIFICATION_PREVIEW),AppSettings_Value(SK_NOTIFICATION_SECONDS),
        !g_settings_ui->open&&!warning.active&&!OdometerView_Visible()&&!PowerService_RunRequired(),now);}
    DashboardPageFacts facts={.trips=resolved.trips?resolved.trips:&g_product_trips,.phone=&current_phone,
        .trail=resolved.trail?resolved.trail:&trail,.phone_slot=resolved.phone?resolved.phone_slot:selected_phone,
        .now_ms=now,.units=model.units,.date=date,.weekday=weekday,
        .development_mask=resolved.development_mask};
    DashboardPages_Present(&state,&presentation,&facts,&page);
    uint32_t frame_now=PagePreview_Apply(&state,&presentation,&facts,&page,now);
    SettingsQuick_Present(&page,g_settings_ui);
    PhoneVisual_MusicVisible(page.kind==UI_MUSIC&&page.known&&state.power==IGN_ON&&!g_settings_ui->open&&!state.warning?page.visual_key:0,now);
    if(!g_settings_ui->open||state.power!=IGN_ON)WallpaperRuntime_Process(&page,&state);
    DashboardPagesView_Render(&page,frame_now);
    ProductModeStrip_SetOpacity(UiHome_StripVisible(page.kind,now),now);
    uint32_t quick=SettingsUI_Quick(&state)&&!state.warning&&!state.modal;
    SpeedHome_ApplyScene(&g_settings_ui->pose,quick);
    if(g_settings_ui->open&&state.power==IGN_ON){
        (void)GraphicsBackground_SetImage(NULL);GraphicsBackground_Process();
    }
    SettingsView_Render(g_settings_ui,quick,now);
    {uint32_t bt,gps;taskENTER_CRITICAL();PhoneIndicators_Colors(now,current_phone.connected,&bt,&gps);taskEXIT_CRITICAL();
     ProductStatusIcons_SetCall(state.calls.phone.state!=CALL_IDLE);
     ProductStatusIcons_Render(state.power==IGN_ON&&!g_settings_ui->open,state.calls.phone.state?0x42C975:bt,gps,
        state.power==IGN_ON&&!g_input_mode.allowed&&!state.warning&&!PowerService_RunRequired());}
    /* Page entry policy belongs to the App, not the renderer. Only A/B has
     * a manual reset action. Modal/warning UI has priority over transient hints. */
    uint32_t toast_allowed=state.power==UI_RUNNING&&!state.warning&&!state.modal&&!state.menu;
    PopupNotifications_Process(now,toast_allowed&&page.kind==UI_TRIP&&page.selection<2U);
    PopupNotificationSnapshot toast;
    if(PopupNotifications_Get(&toast)){toast.visible&=toast_allowed;ToastView_Render(&toast);}
    PowerUI_Render(&state,&model,&presentation,&maintenance,now);
    OdometerView_Render(&odo,state.power==IGN_ON&&!PowerService_RunRequired()&&!state.warning,now);
    if(OdometerView_Visible()){lv_obj_add_flag(SpeedHome_GetContentRoot(),LV_OBJ_FLAG_HIDDEN);SettingsView_Hide();}
    GateJournalRecord result={0};uint32_t have_result=BootStore_GetBootInfo(&result);
    RollbackView_Render(have_result&&(result.flags&GATE_F_RESULT_PENDING),result.failed_version,
        result.restored_version,result.failure_reason,result.transaction);
    {UpdateService *u=RuntimeUpdate_GetService();
     InstallView_Render(PowerService_RunRequired(),u&&InstallSession_Active(&u->install)?&u->install:0,now);}
    {ScreenWarningState warning;ScreenWarningOverlay_Get(&warning);ScreenWarningView_Render(&warning);}
diagnostics:
    g_product_ui.power=state.power;g_product_ui.home=0U;g_product_ui.card=state.dashboard.card;
    g_product_ui.footer=state.dashboard.footer;g_product_ui.menu=state.menu;g_product_ui.modal=state.modal;
    g_product_ui.warning=state.warning;g_product_ui.remote=state.dashboard.remote_active;g_product_ui.epoch=state.epoch;
    g_product_ui.speed_valid=model.speed_valid;g_product_ui.odo_valid=model.odo_valid;
    g_product_ui.clock_valid=model.clock_valid;g_product_ui.speed_kph=vehicle.speed_kph;
    g_product_ui.odo_km=odo.display;g_product_ui.arc_value=model.arc_value;
    g_product_ui.arc_color=model.arc_color;g_product_ui.max_kph=model.max_kph;g_product_ui.units=model.units;
    g_product_ui.hour=sys.hour;g_product_ui.minute=sys.minute;g_product_ui.state_revision=state.revision;
    g_product_ui.effect_overflows=state.effect_overflows;g_product_ui.ign_valid=sys.ign_valid;
    g_product_ui.ign_on=sys.ign_on;g_product_ui.links=sys.links;g_product_ui.now_ms=now;
    ++g_product_ui.process_count;++g_product_ui.seq;
}

/* Token and publication APIs are task-context, bounded-copy operations. The
 * companion adapter decodes protocol bytes first; acceptance is not a radio
 * acknowledgement or persistent commit. Token0 means no current peer. */
uint32_t ProductUI_PhoneToken(uint32_t slot)
{
    if(slot>=PHONE_CONTENT_SLOTS||!g_product_ui.ready)return 0;
    uint32_t token;taskENTER_CRITICAL();token=phone_cache.slots[slot].token;taskEXIT_CRITICAL();return token;
}
/* Bounded cross-owner publication. Generation regression cannot retarget a
 * dial command or resurrect a stale call panel after a refreshed contact list. */
uint32_t ProductUI_PublishCalls(const PhoneCallsSnapshot *v)
{
    if(!v||!g_product_ui.ready)return 0;
    taskENTER_CRITICAL();
    uint32_t ok=calls_mailbox.epoch!=v->epoch||(int32_t)(v->generation-calls_mailbox.generation)>=0;
    if(ok)calls_mailbox=*v;
    taskEXIT_CRITICAL();return ok;
}
uint32_t ProductUI_CallTarget(void){return call_target;}
uint32_t ProductUI_PublishPhone(uint32_t slot,uint32_t token,const PhoneStatus *v,uint32_t now)
{
    if(!g_product_ui.ready)return 0;
    uint32_t ok;
    taskENTER_CRITICAL();ok=PhoneContent_Status(&phone_cache,slot,token,v,now);taskEXIT_CRITICAL();return ok;
}
uint32_t ProductUI_PublishMusic(uint32_t slot,uint32_t token,const PhoneMusic *v,uint32_t now)
{
    if(!g_product_ui.ready)return 0;
    uint32_t ok;
    taskENTER_CRITICAL();ok=PhoneContent_Music(&phone_cache,slot,token,v,now);taskEXIT_CRITICAL();return ok;
}
uint32_t ProductUI_SelectPhone(uint32_t slot)
{
    if(slot>=PHONE_CONTENT_SLOTS||!g_product_ui.ready)return 0;
    return 1;
}

/* Development publication never enters the live BT cache. Commit is consumed
 * by the UI owner next tick; busy is returned immediately. No device I/O. */
uint32_t ProductUI_InjectDevelopmentData(const DevelopmentSample *sample,uint32_t ttl_ms)
{
    if(!g_product_ui.ready)return 0;
    taskENTER_CRITICAL();uint32_t ok=DevelopmentData_Request(sample,ttl_ms);taskEXIT_CRITICAL();return ok;
}

/* Multi-producer task facade: a short bounded copy publishes one request;
 * UI alone advances its timer and calls LVGL. ISR callers are not supported. */
uint32_t ShowToastMessages(const char *message,uint32_t seconds)
{
    if(!g_product_ui.ready||!message)return 0;
    taskENTER_CRITICAL();uint32_t ok=PopupNotifications_Request(message,seconds);taskEXIT_CRITICAL();return ok;
}
uint32_t HideToastMessages(void)
{
    if(!g_product_ui.ready)return 0;
    taskENTER_CRITICAL();uint32_t ok=PopupNotifications_Request(NULL,0);taskEXIT_CRITICAL();return ok;
}
