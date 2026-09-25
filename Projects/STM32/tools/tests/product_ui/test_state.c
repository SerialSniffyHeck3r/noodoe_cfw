#include "Ui_State.h"
#include "Ui_OffStages.h"
#include "Ignition_Session.h"
#include "Ui_DashboardPresentation.h"
#include <stddef.h>
static UiState s;
static unsigned assertions,counts[UI_FX_COUNT];
static UiEffect last;
void *memset(void *p,int c,size_t n){unsigned char *b=p;while(n--)*b++=(unsigned char)c;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *a=p;const unsigned char *b=q;while(n--)*a++=*b++;return p;}
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
static void send(unsigned kind,unsigned now,unsigned a,unsigned b,unsigned c)
{UiEvent e={kind,now,a,b,c,0};Ui_Dispatch(&s,&e);
 /* Navigation fixtures use immediately available scanout. Delayed/stale
  * acknowledgements are exercised explicitly in test_display_ready. */
 if(kind==UI_EVT_IGN&&a&&s.display_wait){UiEvent ready={UI_EVT_DISPLAY_READY,now,s.epoch,0,0,0};Ui_Dispatch(&s,&ready);}}
static void drain(void){UiEffect f;while(Ui_TakeEffect(&s,&f)){++counts[f.kind];last=f;}}
static void reset(unsigned now){UiConfig c=Ui_DefaultConfig();c.preferred_card=UI_TRIP;Ui_Init(&s,&c,now);memset(counts,0,sizeof(counts));}
/* Navigation fixtures begin on Trip; cold boot itself is checked separately. */
static void running(unsigned now){reset(now);send(UI_EVT_IGN,now,1,0,0);drain();send(UI_EVT_TICK,now+2400,0,0,0);drain();send(UI_EVT_SPEED,now+2400,1,0,0);s.dashboard.card=UI_TRIP;}
static void key(unsigned b,unsigned duration,unsigned now)
{send(UI_EVT_BUTTON,now,b,UI_PRESS,0);send(UI_EVT_BUTTON,now+duration,b,UI_RELEASE,duration);send(UI_EVT_BUTTON,now+duration,b,UI_SHORT,duration);}

unsigned test_phone_reply_navigation(void)
{
    running(0);send(UI_EVT_PHONE_COUNT,2500,10,0,0);
    key(UI_ENTER,100,2600);CHECK(s.dashboard.card==UI_NOTIFICATIONS&&s.notification_open&&!s.dashboard.selection);
    key(UI_DOWN,100,3000);CHECK(s.dashboard.selection==1);
    key(UI_UP,100,3300);CHECK(!s.dashboard.selection);
    key(UI_UP,100,3600);CHECK(s.dashboard.selection==9);
    key(UI_ENTER,UI_LONG_MS,4000);drain();CHECK(counts[UI_FX_PHONE_REPLY]==1&&last.arg==0&&last.value==9);
    s.notification_reply=1;s.reply_count=5;s.reply_selection=0;
    key(UI_DOWN,100,7000);CHECK(s.reply_selection==1);
    key(UI_ENTER,100,7400);drain();CHECK(counts[UI_FX_PHONE_REPLY]==2&&last.arg==1&&last.value==1);
    s.reply_sending=1;key(UI_ENTER,100,7800);drain();CHECK(counts[UI_FX_PHONE_REPLY]==2);
    s.reply_sending=0;s.reply_selection=5;key(UI_ENTER,100,8200);CHECK(!s.notification_reply);
    s.notification_reply=1;send(UI_EVT_SPEED,9000,1,51,0);CHECK(!s.notification_reply);
    key(UI_ENTER,UI_LONG_MS,9500);drain();CHECK(counts[UI_FX_PHONE_REPLY]==2);
    send(UI_EVT_SPEED,13000,1,0,0);
    for(unsigned i=0;i<1000;i++){key(UI_DOWN,100,14000+300*i);CHECK(s.dashboard.selection<10);drain();}
    CHECK(counts[UI_FX_PHONE_REPLY]==2);
    return 0;
}
unsigned test_power(void)
{
    running(0);CHECK(s.power==IGN_ON&&counts[UI_FX_SESSION_START]==1);
    s.dashboard.card=UI_MUSIC;s.dashboard.footer=UI_TRIP2;
    send(UI_EVT_IGN,3000,0,0,0);drain();CHECK(s.power==IGN_STOPPING);
    CHECK(!counts[UI_FX_SESSION_END]&&!counts[UI_FX_SAVE]&&!counts[UI_FX_POWER_OFF]);
    send(UI_EVT_TICK,3999,0,0,0);CHECK(s.off_phase==UI_OFF_DELAY&&s.session_open);
    send(UI_EVT_RING_HIDDEN,3999,s.epoch,0,0);CHECK(s.off_phase==UI_OFF_DELAY);
    send(UI_EVT_TICK,4000,0,0,0);drain();CHECK(s.off_phase==UI_OFF_SUMMARY&&!s.session_open&&counts[UI_FX_SESSION_END]==1);
    send(UI_EVT_TICK,6000,0,0,0);CHECK(s.off_phase==UI_OFF_SUMMARY&&!s.session_open); /* Commit does not wait for rendering. */
    send(UI_EVT_RING_HIDDEN,6010,s.epoch-1,0,0);CHECK(s.off_phase==UI_OFF_SUMMARY);
    send(UI_EVT_RING_HIDDEN,6100,s.epoch,0,0);CHECK(s.off_phase==UI_OFF_SUMMARY&&!s.session_open);
    send(UI_EVT_RING_HIDDEN,6200,s.epoch,0,0);CHECK(s.off_phase_ms==4000);
    send(UI_EVT_TICK,7099,0,0,0);CHECK(!s.session_open);
    send(UI_EVT_TICK,7100,0,0,0);drain();CHECK(!s.session_open&&s.off_phase==UI_OFF_SUMMARY&&counts[UI_FX_SESSION_END]==1);
    send(UI_EVT_TICK,8999,0,0,0);CHECK(s.power==IGN_STOPPING);
    send(UI_EVT_TICK,9000,0,0,0);drain();CHECK(s.power==IGN_OFF_AWAKE&&s.entered_ms==9000);
    send(UI_EVT_TICK,3608999,0,0,0);CHECK(s.power==IGN_OFF_AWAKE);
    send(UI_EVT_TICK,3609000,0,0,0);drain();CHECK(s.power==IGN_OFF_SLEEPING);
    s.display_asleep=1;send(UI_EVT_IGN,3612500,1,0,0);drain();CHECK(s.power==IGN_STARTING&&s.welcome_active);
    CHECK(s.dashboard.card==UI_MUSIC&&!s.dashboard.selection&&s.dashboard.footer==UI_TRIP2);
    send(UI_EVT_TICK,3614899,0,0,0);CHECK(s.power==IGN_STARTING);
    send(UI_EVT_TICK,3614900,0,0,0);drain();CHECK(s.power==IGN_ON&&counts[UI_FX_SESSION_START]==2);
    return 0;
}
unsigned test_standby(void)
{
    reset(0);send(UI_EVT_IGN,0,0,0,0);drain();CHECK(s.power==IGN_OFF_AWAKE);
    CHECK(!counts[UI_FX_SESSION_START]&&!counts[UI_FX_SESSION_END]);
    for(unsigned n=1;n<20;++n){send(UI_EVT_LINKS,n*100,1,1,0);key(UI_ENTER,2500,n*3000);}
    CHECK(s.power==IGN_OFF_AWAKE&&s.entered_ms==0);
    s.config.standby_ms=0;send(UI_EVT_TICK,70000,0,0,0);drain();CHECK(s.power==IGN_OFF_SLEEPING);
    send(UI_EVT_LINKS,71000,0,0,0);send(UI_EVT_LINKS,72000,9,1,0);key(UI_UP,100,73000);
    CHECK(s.power==IGN_OFF_SLEEPING);
    reset(0xfffff000U);send(UI_EVT_IGN,0xfffff000U,1,0,0);drain();
    send(UI_EVT_TICK,0xfffff000U+2400U,0,0,0);drain();CHECK(s.power==IGN_ON);
    send(UI_EVT_IGN,0xfffffff0U,0,0,0);drain();
    send(UI_EVT_TICK,983U,0,0,0);CHECK(s.off_phase==UI_OFF_DELAY);
    send(UI_EVT_TICK,984U,0,0,0);CHECK(s.off_phase==UI_OFF_SUMMARY&&!s.session_open);
    send(UI_EVT_RING_HIDDEN,1384U,s.epoch,0,0);send(UI_EVT_TICK,11384U,0,0,0);drain();
    send(UI_EVT_TICK,61384U,0,0,0);drain();CHECK(s.power==IGN_OFF_AWAKE);
    s.dashboard.card=UI_MUSIC;send(UI_EVT_IGN,61385U,1,0,0);drain();
    CHECK(s.startup_sweep&&!s.welcome_active&&s.dashboard.card==UI_MUSIC&&!s.dashboard.selection);
    /* Quick OFF->ON keeps the same ride, and never replays Welcome. */
    running(0);send(UI_EVT_IGN,3000,0,0,0);drain();send(UI_EVT_IGN,3010,1,0,0);drain();
    CHECK(s.power==IGN_STARTING&&counts[UI_FX_SESSION_START]==1&&counts[UI_FX_SESSION_END]==0);
    CHECK(!s.welcome_active&&!s.startup_sweep&&s.dashboard.card==UI_TRIP);
    send(UI_EVT_TICK,3409,0,0,0);CHECK(s.power==IGN_STARTING);
    send(UI_EVT_TICK,3410,0,0,0);CHECK(s.power==IGN_ON);
    return 0;
}
unsigned test_display_ready(void)
{
    reset(0);UiEvent e={UI_EVT_IGN,0,1,0,0,0};Ui_Dispatch(&s,&e);drain();
    send(UI_EVT_TICK,10000,0,0,0);CHECK(s.power==IGN_STARTING&&s.display_wait);
    send(UI_EVT_DISPLAY_READY,10000,s.epoch-1,0,0);CHECK(s.display_wait);
    send(UI_EVT_DISPLAY_READY,10001,s.epoch,0,0);CHECK(!s.display_wait&&s.entered_ms==10001);
    send(UI_EVT_TICK,12400,0,0,0);CHECK(s.power==IGN_STARTING);
    send(UI_EVT_TICK,12401,0,0,0);CHECK(s.power==IGN_ON);
    send(UI_EVT_DISPLAY_READY,12402,s.epoch,0,0);CHECK(s.entered_ms==12401);
    return 0;
}
unsigned test_ignition_session(void)
{
    IgnitionSession v={0};IgnitionSession_Tick(&v,0,1,1,36);
    for(unsigned t=100;t<=10000;t+=100)IgnitionSession_Tick(&v,t,1,1,36);
    CHECK(v.distance_mm==100000&&v.ride_ms==10000);
    for(unsigned t=10100;t<=20000;t+=100)IgnitionSession_Tick(&v,t,1,1,0);
    CHECK(v.ride_ms==20000&&v.distance_mm==100500);
    IgnitionSession_Tick(&v,20100,0,1,0);unsigned long long frozen=v.finished_distance_mm;
    CHECK(!v.distance_mm&&!v.ride_ms&&v.completed==1);
    IgnitionSession_Tick(&v,30000,0,1,100);CHECK(v.finished_distance_mm==frozen&&v.finished_ride_ms==20100&&v.completed==1);
    IgnitionSession_Tick(&v,0xfffffff0U,1,1,36);IgnitionSession_Tick(&v,84,0,1,36);
    CHECK(v.finished_ride_ms==100&&v.finished_distance_mm==1000&&v.completed==2);
    IgnitionSession_Tick(&v,1000,1,0,0);IgnitionSession_Tick(&v,4000,0,0,0);
    CHECK(v.finished_partial&&v.finished_ride_ms==3000&&!v.finished_distance_mm);
    IgnitionSession_Tick(&v,5000,1,1,1);
    for(unsigned t=5005;t<=8600;t+=5)IgnitionSession_Tick(&v,t,1,1,1);
    CHECK(v.distance_mm==1000); /*1km/h for3.6s, without sub-mm truncation. */
    return 0;
}

unsigned test_input(void)
{
    running(0);UiConfig cfg=Ui_DefaultConfig();cfg.boot_held_mask=UI_BIT(UI_UP);cfg.preferred_card=UI_TRIP;Ui_Init(&s,&cfg,0);
    send(UI_EVT_IGN,0,1,0,0);drain();send(UI_EVT_TICK,2400,0,0,0);drain();
    CHECK(s.dashboard.card==UI_BLANK);key(UI_ENTER,100,2500);CHECK(s.dashboard.card==UI_TRIP);
    key(UI_UP,100,3000);CHECK(s.dashboard.footer==UI_ODO&&!s.blocked_buttons);
    key(UI_DOWN,100,4000);CHECK(s.dashboard.selection==1&&s.dashboard.footer==UI_ODO);
    key(UI_ENTER,100,5000);CHECK(s.dashboard.card==UI_NOTIFICATIONS);
    key(UI_ENTER,100,6000);CHECK(s.dashboard.card==UI_MUSIC);
    key(UI_ENTER,100,7000);CHECK(s.dashboard.card==UI_CALLS);
    key(UI_ENTER,2200,8000);CHECK(!s.dashboard.remote_active);
    send(UI_EVT_LINKS,11000,1,1,0);key(UI_ENTER,2200,12000);CHECK(!s.dashboard.remote_active);
    drain();CHECK(!counts[UI_FX_REMOTE]);
    return 0;
}

/* Exercise actual state/ride code: cancel before1000ms, commit exactly once
 * at the threshold, and reject legacy frame acknowledgements as end triggers. */
unsigned test_provisional_ride(void)
{
    IgnitionSession ride={0};running(0);
    IgnitionSession_Tick(&ride,2400,s.session_open,1,36);
    for(unsigned t=2500;t<=3400;t+=100)IgnitionSession_Tick(&ride,t,s.session_open,1,36);
    unsigned now=3400;
    for(unsigned phase=0;phase<3;++phase){
        unsigned long long before=ride.distance_mm,elapsed=ride.ride_ms;
        send(UI_EVT_IGN,now,0,0,0);drain();
        unsigned old_epoch=s.epoch,delay=phase==2?999:phase==1?500:100;
        for(unsigned i=0;i<delay;++i){++now;send(UI_EVT_TICK,now,0,0,0);IgnitionSession_Tick(&ride,now,s.session_open,1,36);}
        send(UI_EVT_IGN,now,1,0,0);drain();IgnitionSession_Tick(&ride,now,s.session_open,1,36);
        CHECK(s.session_open&&!ride.completed&&ride.distance_mm>before&&ride.ride_ms>elapsed);
        CHECK(counts[UI_FX_SESSION_START]==1&&!counts[UI_FX_SESSION_END]&&!s.welcome_active);
        send(UI_EVT_RING_HIDDEN,now,old_epoch,0,0);CHECK(s.power==IGN_STARTING);
        now+=400;send(UI_EVT_TICK,now,0,0,0);drain();
    }
    send(UI_EVT_IGN,++now,0,0,0);drain();
    now+=1000;send(UI_EVT_TICK,now,0,0,0);IgnitionSession_Tick(&ride,now,s.session_open,1,36);
    drain();CHECK(ride.completed==1&&counts[UI_FX_SESSION_END]==1);
    now+=400;send(UI_EVT_RING_HIDDEN,now,s.epoch,0,0);
    for(unsigned i=0;i<1000;++i){++now;send(UI_EVT_TICK,now,0,0,0);IgnitionSession_Tick(&ride,now,s.session_open,1,36);}
    drain();CHECK(!ride.distance_mm&&!ride.ride_ms&&ride.completed==1&&counts[UI_FX_SESSION_END]==1);
    unsigned long long final=ride.finished_distance_mm,final_time=ride.finished_ride_ms;
    send(UI_EVT_TICK,++now,0,0,0);IgnitionSession_Tick(&ride,now,s.session_open,1,36);
    CHECK(ride.completed==1&&ride.finished_distance_mm==final);
    send(UI_EVT_IGN,++now,1,0,0);drain();IgnitionSession_Tick(&ride,now,s.session_open,1,36);
    CHECK(counts[UI_FX_SESSION_START]==2&&!ride.distance_mm&&!ride.ride_ms&&!s.welcome_active);
    CHECK(ride.finished_ride_ms==final_time&&ride.finished_distance_mm==final);
    return 0;
}

unsigned test_navigation(void)
{
    running(0);s.dashboard.card=UI_SYSTEM;key(UI_ENTER,2200,3000);
    CHECK(!s.menu); /* Only the SettingsUI owner may open settings. */
    CHECK(!Ui_BeginEdit(&s,42,10,0,20,3)&&!s.modal);
    running(0);s.dashboard.footer=UI_TRIP1;key(UI_ENTER,2200,3000);drain();
    CHECK(!s.modal&&s.pending_id&&counts[UI_FX_RESET_TRIP]==1&&last.arg==UI_TRIP1);
    send(UI_EVT_EFFECT_DONE,5300,s.pending_id,s.epoch,0);
    s.dashboard.selection=1;send(UI_EVT_BUTTON,6000,UI_ENTER,UI_PRESS,0);
    send(UI_EVT_BUTTON,8001,UI_ENTER,UI_LONG,2001);drain();
    CHECK(!s.modal&&s.pending_id&&counts[UI_FX_RESET_TRIP]==2&&last.arg==UI_TRIP2);
    send(UI_EVT_EFFECT_DONE,8002,s.pending_id,s.epoch,0);
    send(UI_EVT_BUTTON,9000,UI_ENTER,UI_VERY_LONG,3000);
    send(UI_EVT_BUTTON,9200,UI_ENTER,UI_RELEASE,3200);drain();
    CHECK(counts[UI_FX_RESET_TRIP]==2&&s.dashboard.card==UI_TRIP);
    s.dashboard.selection=2;key(UI_ENTER,2200,10000);drain();CHECK(counts[UI_FX_RESET_TRIP]==2);
    running(0);s.dashboard.card=UI_SYSTEM;s.menu=UI_MENU_QUICK;
    CHECK(s.menu==UI_MENU_QUICK);
    key(UI_DOWN,100,6000);key(UI_DOWN,100,7000);key(UI_ENTER,2200,8000);
    CHECK(s.menu==UI_MENU_QUICK&&s.dashboard.card==UI_SYSTEM);s.menu=UI_MENU_NONE;
    /* OBD uses the child cycle, never a top-level home or late automatic jump. */
    CHECK(!Ui_CardAvailable(&s,UI_RETIRED_OBD));s.dashboard.card=UI_SYSTEM;
    key(UI_ENTER,100,11000);CHECK(s.dashboard.card==UI_BLANK);
    send(UI_EVT_LINKS,12000,UI_LINK_OBD,0,0);CHECK(s.dashboard.card==UI_BLANK);
    s.dashboard.card=UI_MUSIC;key(UI_ENTER,100,13000);CHECK(s.dashboard.card==UI_CALLS);
    key(UI_ENTER,100,13500);CHECK(s.dashboard.card==UI_PHONE_GPS);
    unsigned footer=s.dashboard.footer;key(UI_DOWN,100,14000);
    CHECK(s.dashboard.selection==0&&s.dashboard.footer==footer);
    s.menu=UI_MENU_SETTINGS;s.selection=2;
    send(UI_EVT_LINKS,16000,0,0,0);CHECK(s.dashboard.card==UI_PHONE_GPS);
    CHECK(s.menu==UI_MENU_SETTINGS&&s.selection==2&&s.dashboard.footer==footer);
    return 0;
}

unsigned test_warnings(void)
{
    running(0);s.dashboard.footer=UI_TRIP2;send(UI_EVT_RESERVE_ENTER,3000,0,0,0);drain();
    CHECK(!s.warning&&s.dashboard.footer==UI_RESV); /* Overlay presentation is independently tested. */
    CHECK(counts[UI_FX_RESERVE_START]==1);send(UI_EVT_RESERVE_ENTER,3100,0,0,0);drain();CHECK(counts[UI_FX_RESERVE_START]==1);
    send(UI_EVT_TICK,7999,0,0,0);CHECK(!s.warning);
    send(UI_EVT_TICK,8000,0,0,0);CHECK(!s.warning);
    send(UI_EVT_TICK,8400,0,0,0);CHECK(!s.warning);
    send(UI_EVT_TICK,11400,0,0,0);CHECK(s.dashboard.footer==UI_RESV&&!s.warning);
    key(UI_DOWN,100,12000);CHECK(s.dashboard.footer==UI_RESV);
    key(UI_ENTER,100,13000);CHECK(s.dashboard.card==UI_NOTIFICATIONS); /* Only footer locked */
    send(UI_EVT_REFUEL,14000,0,0,0);drain();CHECK(!s.reserve_active&&s.dashboard.footer==UI_TRIP2&&counts[UI_FX_RESERVE_CLEAR]==1);
    send(UI_EVT_MAINTENANCE,15000,7,0,0);CHECK(s.warning==UI_WARN_OIL);
    send(UI_EVT_TICK,23400,0,0,0);CHECK(s.warning==UI_WARN_BELT);
    send(UI_EVT_TICK,31800,0,0,0);CHECK(s.warning==UI_WARN_SERV);
    send(UI_EVT_TICK,40200,0,0,0);CHECK(!s.warning);
    send(UI_EVT_MAINTENANCE,41000,7,0,0);CHECK(!s.warning);
    send(UI_EVT_IGN,42000,0,0,0);drain();send(UI_EVT_IGN,43000,1,0,0);drain();
    send(UI_EVT_TICK,45400,0,0,0);drain();CHECK(s.warning==UI_WARN_OIL);
    send(UI_EVT_RESERVE_ENTER,46000,0,0,0);drain();CHECK(s.warning==UI_WARN_OIL&&s.dashboard.footer==UI_RESV);
    send(UI_EVT_IGN,47000,0,0,0);drain();CHECK(!s.warning&&s.reserve_active);
    send(UI_EVT_IGN,48000,1,0,0);drain();send(UI_EVT_TICK,50400,0,0,0);drain();
    CHECK(s.warning!=UI_WARN_FUEL&&s.reserve_active&&counts[UI_FX_RESERVE_START]==2);
    return 0;
}

unsigned test_masks_and_stress(void)
{
    UiConfig c=Ui_DefaultConfig();c.card_mask=UI_BIT(UI_BLANK);c.footer_mask=1;
    Ui_Init(&s,&c,0);send(UI_EVT_IGN,0,1,0,0);drain();send(UI_EVT_TICK,2400,0,0,0);drain();
    key(UI_ENTER,100,3000);CHECK(s.dashboard.card==UI_BLANK);send(UI_EVT_LINKS,4000,7,1,0);CHECK(!Ui_CardAvailable(&s,UI_RETIRED_OBD));
    running(0);unsigned seed=1234567,now=2000;
    for(unsigned n=0;n<30000;++n){
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;now+=seed%200;
        switch(seed%9){
        case 0:send(UI_EVT_TICK,now,0,0,0);break;
        case 1:send(UI_EVT_IGN,now,(seed>>8)&1,0,0);break;
        case 2:send(UI_EVT_LINKS,now,(seed>>8)&15,1,0);break;
        case 3:send(UI_EVT_BUTTON,now,(seed>>8)%3,UI_PRESS,0);break;
        case 4:send(UI_EVT_BUTTON,now,(seed>>8)%3,UI_RELEASE,seed%4000);break;
        case 5:send(UI_EVT_RESERVE_ENTER,now,0,0,0);break;
        case 6:send(UI_EVT_REFUEL,now,0,0,0);break;
        case 7:send(UI_EVT_MAINTENANCE,now,(seed>>8)&7,0,0);break;
        case 8:send(UI_EVT_EFFECT_DONE,now,s.save_id,s.epoch,0);break;
        }
        CHECK(s.power<=OFF_DEEP_SLEEP&&s.dashboard.card<UI_CARD_COUNT);
        CHECK(s.dashboard.footer<UI_FOOTER_COUNT&&s.menu<UI_MENU_COUNT&&s.effect_count<=UI_EFFECT_CAPACITY);
        CHECK(!s.dashboard.remote_active||(s.power==UI_RUNNING&&(s.links&UI_LINK_PHONES)&&!s.warning));
        drain();
    }
    CHECK(!s.effect_overflows);return 0;
}
unsigned get_assertions(void){return assertions;}

/* Child selection survives parent menus and cannot corrupt the parent's
 * focused item. Physical press/release events exercise the real reducer. */
unsigned test_dashboard_hierarchy(void)
{
    running(0);s.dashboard.footer=UI_TRIP2;s.selection=5;
    key(UI_ENTER,100,3000);key(UI_ENTER,100,4000);
    CHECK(s.dashboard.card==UI_MUSIC&&s.dashboard.footer==UI_TRIP2);
    /* Music DOWN now emits NEXT; it no longer selects a child row. */
    key(UI_DOWN,100,5000);CHECK(s.dashboard.selection==0&&s.selection==5);
    key(UI_ENTER,100,6000);CHECK(s.dashboard.card==UI_CALLS&&!s.dashboard.selection);
    key(UI_ENTER,100,7000);CHECK(s.dashboard.card==UI_PHONE_GPS&&!s.dashboard.selection);
    key(UI_ENTER,100,8000);
    CHECK(s.dashboard.card==UI_SYSTEM);key(UI_ENTER,2200,10000);
    CHECK(!s.menu&&s.dashboard.card==UI_SYSTEM&&s.dashboard.footer==UI_TRIP2);
    s.menu=UI_MENU_SETTINGS; /* Simulate SettingsUI overlay ownership. */
    unsigned selected=s.selection;key(UI_DOWN,100,13000);
    CHECK(s.selection==selected&&!s.dashboard.selection&&s.dashboard.card==UI_SYSTEM);
    s.menu=UI_MENU_NONE;
    key(UI_ENTER,100,19000);CHECK(s.dashboard.card==UI_BLANK);key(UI_ENTER,100,19300);CHECK(s.dashboard.card==UI_TRIP);
    key(UI_DOWN,2200,20000);CHECK(s.dashboard.footer==UI_OIL); /* Skip forced TRIP F. */
    key(UI_ENTER,2200,21000);CHECK(!s.modal&&s.pending_id);
    key(UI_DOWN,100,24000);CHECK(s.dashboard.footer==UI_OIL);
    send(UI_EVT_IGN,25000,0,0,0);CHECK(!s.modal&&s.power==UI_STOP_SUMMARY);
    return 0;
}

static unsigned Equal(const char *a,const char *b)
{while(*a&&*a==*b){++a;++b;}return *a==*b;}

/* Unknown, due, fraction and maximum64-bit domain distances remain distinct. */
unsigned test_maintenance_fraction(void)
{
    uint32_t ratio=17;CHECK(!UiDashboard_Remaining(0,0,&ratio)&&ratio==17);
    CHECK(!UiDashboard_Remaining(1,2,0));
    CHECK(UiDashboard_Remaining(0,2000,&ratio)&&ratio==1000);
    CHECK(UiDashboard_Remaining(500,2000,&ratio)&&ratio==750);
    CHECK(UiDashboard_Remaining(2000,2000,&ratio)&&ratio==0);
    CHECK(UiDashboard_Remaining(UINT64_MAX,2000,&ratio)&&ratio==0);
    CHECK(UiDashboard_Remaining(1,UINT64_MAX,&ratio)&&ratio==999);
    CHECK(UiDashboard_Remaining(UINT64_MAX-1,UINT64_MAX,&ratio)&&ratio==0);
    for(uint32_t used=0;used<=10000;++used){
        CHECK(UiDashboard_Remaining(used,10000,&ratio));
        CHECK(ratio==(10000-used)*1000U/10000U);
    }
    UiDashboardPresentation p={0};UiDashboardMaintenance m={.valid_mask=7,.remaining_permille={750,0,1000}};
    for(uint32_t f=0;f<UI_FOOTER_COUNT;++f){
        p.footer=f;UiDashboard_PresentMaintenance(&m,&p);
        CHECK(p.maintenance==(f>=UI_OIL));
        if(f<=UI_OIL)CHECK(p.remaining_valid&&p.remaining_permille==750);
        else CHECK(!p.remaining_valid&&!p.auxiliary_value[0]);
    }
    p.footer=UI_OIL;m.valid_mask=0;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(p.maintenance&&!p.remaining_valid);
    m.valid_mask=1;m.remaining_permille[0]=1001;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(!p.remaining_valid);
    return 0;
}

/* Real formatter: explicit unknowns, exact decimal/unit conversion and safe
 * overflow. Neither a phone absence nor a missing distance turns into zero. */
unsigned test_dashboard_presentation(void)
{
    running(0);UiDashboardPresentation p;UiDashboardDistances d={0};
    UiDashboard_Present(&s,&d,36475,1,0,1,&p);
    CHECK(Equal(p.footer_title,"ODO")&&Equal(p.footer_value,"36475")&&Equal(p.unit,"km"));
    CHECK(Equal(p.title,"Trip computer")&&Equal(p.line,"Dashboard connected"));
    UiDashboard_Present(&s,&d,36475,0,0,0,&p);CHECK(Equal(p.footer_value,"------")&&!p.footer_valid);
    s.dashboard.footer=UI_TRIP1;UiDashboard_Present(&s,&d,36475,1,0,1,&p);
    CHECK(Equal(p.footer_value,"----.-")&&!p.footer_valid);
    d.valid_mask=UI_BIT(UI_TRIP1);d.distance_mm[UI_TRIP1]=123456789ULL;
    UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(Equal(p.footer_value,"123.4")&&p.footer_valid);
    d.distance_mm[UI_TRIP1]=1609344ULL;
    UiDashboard_Present(&s,&d,36475,1,1,1,&p);CHECK(Equal(p.footer_value,"1.0")&&Equal(p.unit,"mi"));
    for(unsigned f=UI_TRIP1;f<UI_FOOTER_COUNT;++f){
        s.dashboard.footer=f;d.valid_mask=UI_BIT(f);d.distance_mm[f]=0;
        UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(p.footer_valid&&Equal(p.footer_value,f<=UI_OIL?"0.0":"0"));
        d.distance_mm[f]=UINT64_MAX;UiDashboard_Present(&s,&d,36475,1,1,1,&p);
        CHECK(p.footer_value[0]=='#');
    }
    s.dashboard.footer=UI_RESV;CHECK(Equal(UiDashboard_FooterTitle(UI_RESV),"TRIP F"));
    s.dashboard.footer=UI_ODO;d.distance_mm[UI_ODO]=0;d.valid_mask=1;
    UiDashboard_Present(&s,&d,1000000,1,0,1,&p);CHECK(Equal(p.footer_value,"######"));
    for(unsigned card=0;card<UI_CARD_COUNT;++card){
        s.dashboard.card=card;UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(p.card==card);
        if(card==UI_MUSIC||card==UI_NOTIFICATIONS)CHECK(Equal(p.line,"No phone connected"));
        if(card==UI_REMOTE)CHECK(!p.title[0]&&!p.line[0]);
        if(card==UI_BLANK)CHECK(!p.title[0]&&!p.line[0]&&!p.hint[0]);
    }
    s.dashboard.card=UI_MUSIC;s.dashboard.selection=2;s.links=1;
    UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(Equal(p.hint,"Previous track"));
    s.menu=UI_MENU_SETTINGS;s.selection=3;
    UiDashboard_Present(&s,&d,36475,1,0,1,&p);
    CHECK(!p.title[0]&&!p.line[0]&&!p.hint[0]&&p.footer_valid);
    CHECK(s.dashboard.card==UI_MUSIC&&s.dashboard.selection==2);
    s.modal=UI_MODAL_EDIT;s.edit_value=123;
    UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(!p.number[0]);
    s.warning=UI_WARN_FUEL;UiDashboard_Present(&s,&d,36475,1,0,1,&p);CHECK(Equal(p.title,"Low fuel")&&!p.number[0]);
    return 0;
}

/* Formatting must preserve zero/fractional precision through each decimal
 * width transition. Exercise public presenters, including metric/mile input,
 * so normalization cannot alter units, stored values or unknown handling. */
unsigned test_unpadded_numbers(void)
{
    running(0);UiDashboardPresentation p;UiDashboardDistances d={0};
    static const unsigned whole[]={0,1,9,10,99,100,999,1000,99999,999999};
    static const char *const integers[]={"0","1","9","10","99","100","999","1000","99999","999999"};
    for(unsigned n=0;n<sizeof(whole)/sizeof(whole[0]);++n){
        UiDashboard_Present(&s,&d,whole[n],1,0,1,&p);
        CHECK(Equal(p.footer_value,integers[n]));
    }
    static const unsigned tenths[]={0,1,9,10,99,100,999,1000};
    static const char *const fractions[]={"0.0","0.1","0.9","1.0","9.9","10.0","99.9","100.0"};
    for(unsigned miles=0;miles<2;++miles){
        uint64_t unit=miles?1609344ULL:1000000ULL;
        for(unsigned f=UI_TRIP1;f<UI_FOOTER_COUNT;++f){
            s.dashboard.footer=f;d.valid_mask=UI_BIT(f);
            for(unsigned n=0;n<sizeof(tenths)/sizeof(tenths[0]);++n){
                /* Round source mm up so a tenth-mile test stays in its bin. */
                d.distance_mm[f]=(unit*tenths[n]+9U)/10U;
                UiDashboard_Present(&s,&d,0,0,miles,1,&p);
                if(f<=UI_OIL)CHECK(Equal(p.footer_value,fractions[n]));
                else CHECK(p.footer_value[0]!='0'||p.footer_value[1]==0);
                CHECK(Equal(p.unit,miles?"mi":"km"));
                CHECK(d.distance_mm[f]==(unit*tenths[n]+9U)/10U);
            }
        }
    }
    UiDashboardMaintenance m={0};m.oil_hours_valid=1;m.days_valid_mask=6;
    p.footer=UI_OIL;
    for(unsigned n=0;n<sizeof(tenths)/sizeof(tenths[0]);++n){
        m.oil_ignition_ms=(uint64_t)tenths[n]*360000ULL;
        UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_value[0]);
        CHECK(m.oil_ignition_ms==(uint64_t)tenths[n]*360000ULL);
    }
    for(unsigned f=UI_BELT;f<=UI_SERV;++f){
        p.footer=f;
        for(unsigned n=0;n<9;++n){
            m.elapsed_days[f-UI_OIL]=whole[n];UiDashboard_PresentMaintenance(&m,&p);
            CHECK(!p.auxiliary_value[0]&&m.elapsed_days[f-UI_OIL]==whole[n]);
        }
    }
    s.modal=UI_MODAL_EDIT;s.edit_value=0;
    UiDashboard_Present(&s,&d,0,1,0,1,&p);CHECK(!p.number[0]);
    s.edit_value=UINT32_MAX;
    UiDashboard_Present(&s,&d,0,1,0,1,&p);CHECK(!p.number[0]);
    return 0;
}

unsigned test_fault_timeout_cancel(void)
{
    UiConfig cfg=Ui_DefaultConfig();cfg.preferred_footer=UI_RESV;Ui_Init(&s,&cfg,0);CHECK(s.dashboard.footer==UI_ODO);
    cfg.reserve_active=1;Ui_Init(&s,&cfg,0);CHECK(s.dashboard.footer==UI_RESV&&s.dashboard.footer_before_reserve==UI_ODO);
    running(0);s.dashboard.remote_active=1;s.links=1;send(UI_EVT_FAULT,3000,42,0,0);CHECK(!s.dashboard.remote_active&&s.power==UI_FAULT);
    running(0);CHECK(!Ui_BeginEdit(&s,1,5,0,10,1)&&!s.modal);
    /* An actual asynchronous trip-reset effect still times out and rejects a
     * late completion after its ownership has been retired. */
    s.dashboard.card=UI_TRIP;key(UI_ENTER,2200,8000);drain();unsigned id=s.pending_id;
    CHECK(id&&counts[UI_FX_RESET_TRIP]==1);
    send(UI_EVT_TICK,15200,0,0,0);CHECK(!s.pending_id&&s.last_error==9&&!s.modal);
    send(UI_EVT_EFFECT_DONE,15201,id,s.epoch,0);CHECK(!s.modal&&s.ignored_completions==1);
    send(UI_EVT_IGN,16000,0,0,0);drain();
    send(UI_EVT_TICK,17000,0,0,0);send(UI_EVT_RING_HIDDEN,17400,s.epoch,0,0);
    send(UI_EVT_TICK,17900,0,0,0);drain();send(UI_EVT_TICK,22900,0,0,0);drain();CHECK(s.power==IGN_OFF_AWAKE);
    send(UI_EVT_FAULT,22901,42,0,0);send(UI_EVT_EFFECT_DONE,22902,id,s.epoch,0);CHECK(s.power==UI_FAULT);
    send(UI_EVT_IGN,23000,1,0,0);drain();CHECK(s.power==IGN_STARTING&&!s.last_error);
    /* Overflow is explicit; does not overwrite an earlier effect. */
    running(0);s.dashboard.card=UI_MUSIC;send(UI_EVT_LINKS,3000,1,1,0);
    for(unsigned n=0;n<UI_EFFECT_CAPACITY+1;n++)key(UI_UP,100,7000+n*200);
    CHECK(s.effect_overflows==1&&s.power==UI_FAULT&&s.effect_count==UI_EFFECT_CAPACITY);
    drain();return 0;
}

/* Hours/days remain domain values but no longer occupy the footer. Oil
 * remaining is still visible independently of internal hours validity. */
unsigned test_maintenance_auxiliary(void)
{
    UiDashboardPresentation p={0};UiDashboardMaintenance m={0};
    m.valid_mask=1;m.remaining_permille[0]=750;
    for(unsigned f=UI_ODO;f<=UI_RESV;++f){p.footer=f;UiDashboard_PresentMaintenance(&m,&p);
        CHECK(!p.maintenance&&p.remaining_valid&&p.remaining_permille==750&&!p.auxiliary_title[0]);}
    p.footer=UI_OIL;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(p.maintenance&&p.remaining_valid&&p.remaining_permille==750&&!p.auxiliary_title[0]&&!p.auxiliary_value[0]);
    m.oil_hours_valid=1;m.oil_ignition_ms=123450000ULL;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(!p.auxiliary_value[0]&&m.oil_ignition_ms==123450000ULL);
    m.valid_mask=0;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(!p.remaining_valid&&!p.auxiliary_value[0]);
    m.valid_mask=1;UiDashboard_PresentMaintenance(&m,&p);
    CHECK(p.remaining_valid&&p.remaining_permille==750);
    m.oil_ignition_ms=UINT64_MAX;UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_value[0]&&m.oil_ignition_ms==UINT64_MAX);
    m.days_valid_mask=6;m.elapsed_days[1]=365;m.elapsed_days[2]=99999;
    p.footer=UI_BELT;UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_title[0]&&!p.auxiliary_value[0]&&m.elapsed_days[1]==365);
    p.footer=UI_SERV;UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_value[0]&&m.elapsed_days[2]==99999);
    m.elapsed_days[2]=100000;UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_value[0]);
    m.days_valid_mask=0;UiDashboard_PresentMaintenance(&m,&p);CHECK(!p.auxiliary_value[0]);
    return 0;
}

/* Raw20km is the same threshold in either display unit; exactly20km is white.
 * Invalid data and other modes must clear a previous yellow reserve value. */
unsigned test_reserve_distance_color(void)
{
    running(0);s.dashboard.footer=UI_RESV;UiDashboardDistances d={0};UiDashboardPresentation p;
    d.valid_mask=UI_BIT(UI_RESV);
    for(unsigned miles=0;miles<2;++miles){
        d.distance_mm[UI_RESV]=19999999;UiDashboard_Present(&s,&d,0,0,miles,0,&p);CHECK(!p.reserve_distance_warning);
        d.distance_mm[UI_RESV]=20000000;UiDashboard_Present(&s,&d,0,0,miles,0,&p);CHECK(!p.reserve_distance_warning);
        d.distance_mm[UI_RESV]=20000001;UiDashboard_Present(&s,&d,0,0,miles,0,&p);CHECK(p.reserve_distance_warning);
        d.distance_mm[UI_RESV]=UINT64_MAX;UiDashboard_Present(&s,&d,0,0,miles,0,&p);CHECK(p.reserve_distance_warning);
    }
    d.valid_mask=0;UiDashboard_Present(&s,&d,0,0,0,0,&p);CHECK(!p.reserve_distance_warning&&!p.footer_valid);
    d.valid_mask=UI_BIT(UI_RESV);s.dashboard.footer=UI_ODO;UiDashboard_Present(&s,&d,36475,1,0,0,&p);CHECK(!p.reserve_distance_warning);
    return 0;
}

/* Both real phone link bits keep phone cards usable. The retired GPS bit does
 * not impersonate a phone; OBD and a second phone never steal the active card. */
unsigned test_single_phone_ui(void)
{
    running(0);s.dashboard.card=UI_REMOTE;
    send(UI_EVT_LINKS,2100,UI_LINK_OBD|UI_LINK_PHONE2|2U,0,0);CHECK(!s.links);
    key(UI_ENTER,2200,3000);CHECK(!s.dashboard.remote_active);
    send(UI_EVT_LINKS,6000,UI_LINK_PHONE1,0,0);
    key(UI_ENTER,2200,7000);CHECK(!s.dashboard.remote_active);
    send(UI_EVT_LINKS,10000,0,0,0);CHECK(!s.dashboard.remote_active);
    return 0;
}

/* Render conversion, missing/stale values and parent-overlay precedence use
 * the production presenter; these cases do not create synthetic runtime data. */
unsigned test_retired_card_preferences(void)
{
    UiConfig c=Ui_DefaultConfig();c.card_mask=UI_BIT(UI_RETIRED_OBD);c.preferred_card=UI_RETIRED_OBD;
    Ui_Init(&s,&c,0);CHECK(s.dashboard.card==UI_BLANK);
    CHECK(!Ui_CardAvailable(&s,UI_RETIRED_OBD));
    send(UI_EVT_LINKS,1,UI_LINK_OBD|UI_LINK_PHONE2,0,0);CHECK(!s.links&&s.dashboard.card==UI_BLANK);
    return 0;
}

unsigned test_home_button_cycle(void)
{
    running(0);s.dashboard.card=UI_BLANK;s.dashboard.selection=0;
    key(UI_DOWN,100,3000);CHECK(s.dashboard.selection==1);
    key(UI_DOWN,100,3200);CHECK(s.dashboard.selection==2);
    key(UI_DOWN,100,3400);CHECK(s.dashboard.selection==0);
    key(UI_UP,100,3600);CHECK(s.dashboard.selection==2);
    key(UI_UP,100,3800);CHECK(s.dashboard.selection==1);
    key(UI_ENTER,100,4000);CHECK(s.dashboard.card==UI_TRIP&&!s.dashboard.selection);
    return 0;
}

unsigned test_off_stage_sequence(void)
{
    static const unsigned stage[]={OFF_DISPLAY_HOLD,OFF_BT_HOLD,OFF_DEEP_SLEEP};
    for(unsigned mask=1;mask<=7;++mask)for(unsigned zero=0;zero<4;++zero){
        UiConfig c=Ui_DefaultConfig();c.off_stage_mask=mask;
        c.standby_ms=(zero&1)?0:100;c.bt_retention_ms=(zero&2)?0:200;
        unsigned expected[3],n=0;
        for(unsigned i=0;i<3;++i)if(mask&(1U<<i)){
            unsigned last=(mask>>(i+1))==0;
            if(last||i==2||(i==0?c.standby_ms:c.bt_retention_ms))expected[n++]=stage[i];
        }
        Ui_Init(&s,&c,0xffffffc0U);memset(counts,0,sizeof(counts));
        send(UI_EVT_IGN,0xffffffc0U,0,0,0);drain();
        CHECK(s.power==expected[0]&&s.config.off_stage_mask==mask);
        for(unsigned i=0;i<n;++i){
            unsigned entered=s.entered_ms;
            CHECK(s.power==expected[i]&&!s.session_open);
            CHECK(Ui_OffStages_IsOff(s.power));
            unsigned duration=s.power==OFF_DISPLAY_HOLD?c.standby_ms:c.bt_retention_ms;
            if(i+1<n){
                send(UI_EVT_TICK,entered+duration-1,0,0,0);drain();CHECK(s.power==expected[i]);
                send(UI_EVT_TICK,entered+duration,0,0,0);drain();CHECK(s.power==expected[i+1]);
                CHECK(s.entered_ms==entered+duration);
            }else{
                send(UI_EVT_TICK,entered+86400000U,0,0,0);drain();CHECK(s.power==expected[i]);
                send(UI_EVT_LINKS,entered+86400001U,UI_LINK_PHONES,0,0);drain();
                CHECK(s.entered_ms==entered&&s.power==expected[i]);
            }
        }
        CHECK(!counts[UI_FX_SESSION_START]&&!counts[UI_FX_SESSION_END]&&!counts[UI_FX_POWER_OFF]);
        s.display_asleep=s.power!=OFF_DISPLAY_HOLD;
        unsigned welcome=s.display_asleep;
        send(UI_EVT_IGN,s.now_ms+1,1,0,0);drain();
        CHECK(s.power==IGN_STARTING&&s.startup_sweep&&s.welcome_active==welcome);
        CHECK(counts[UI_FX_SESSION_START]==1);
    }
    UiConfig c=Ui_DefaultConfig();c.off_stage_mask=0;Ui_Init(&s,&c,0);
    CHECK(s.config.off_stage_mask==UI_OFF_STAGE_DISPLAY);
    CHECK(Ui_OffStages_First(&s.config)==OFF_DISPLAY_HOLD);
    CHECK(Ui_OffStages_Next(&s.config,OFF_DISPLAY_HOLD,86400000)==OFF_DISPLAY_HOLD);
    /* OFF cancellation and summary closure still precede all selected stages. */
    for(unsigned i=0;i<3;++i){
        running(0);s.config.off_stage_mask=1U<<i;
        send(UI_EVT_IGN,3000,0,0,0);send(UI_EVT_TICK,3999,0,0,0);drain();
        CHECK(s.power==IGN_STOPPING&&s.session_open&&!counts[UI_FX_SESSION_END]);
        send(UI_EVT_IGN,3999,1,0,0);drain();CHECK(s.session_open&&!counts[UI_FX_SESSION_END]);
        send(UI_EVT_TICK,4399,0,0,0);send(UI_EVT_IGN,4400,0,0,0);
        send(UI_EVT_TICK,5400,0,0,0);drain();CHECK(counts[UI_FX_SESSION_END]==1);
        send(UI_EVT_TICK,10399,0,0,0);CHECK(s.power==IGN_STOPPING);
        send(UI_EVT_TICK,10400,0,0,0);drain();CHECK(s.power==stage[i]);
        send(UI_EVT_TICK,86410400,0,0,0);drain();CHECK(s.power==stage[i]&&counts[UI_FX_SESSION_END]==1);
    }
    return 0;
}

/* Every retained OFF stage preserves category, local item and footer. Remote
 * arming/pressed actions are intentionally cancelled even when its card stays. */
unsigned test_resume_riding_context(void)
{
    const unsigned stages[]={OFF_DISPLAY_HOLD,OFF_BT_HOLD,OFF_DEEP_SLEEP};
    for(unsigned card=0;card<UI_CARD_COUNT;++card)for(unsigned item=0;item<3;++item)
        for(unsigned stage=0;stage<3;++stage){
            running(0);s.dashboard.card=card;s.dashboard.selection=item;
            s.dashboard.footer=UI_BELT;s.dashboard.remote_active=card==UI_REMOTE;
            s.config.off_stage_mask=1U<<stage;
            send(UI_EVT_IGN,3000,0,0,0);drain();
            send(UI_EVT_TICK,4000,0,0,0);drain();send(UI_EVT_TICK,9000,0,0,0);drain();
            CHECK(s.power==stages[stage]);
            CHECK(s.dashboard.card==card&&s.dashboard.selection==item&&s.dashboard.footer==UI_BELT);
            s.display_asleep=stage!=0;
            send(UI_EVT_IGN,9500,1,0,0);drain();
            CHECK(s.power==IGN_STARTING&&s.startup_sweep&&!s.dashboard.remote_active);
            CHECK(s.dashboard.card==card&&s.dashboard.selection==item&&s.dashboard.footer==UI_BELT);
            send(UI_EVT_TICK,12000,0,0,0);drain();CHECK(s.power==IGN_ON);
            CHECK(s.dashboard.card==card&&s.dashboard.selection==item&&s.dashboard.footer==UI_BELT);
            CHECK(counts[UI_FX_SESSION_START]==2&&counts[UI_FX_SESSION_END]==1);
            /* Duplicate ON must neither restart animation nor change the page. */
            unsigned epoch=s.epoch;send(UI_EVT_IGN,12500,1,0,0);drain();
            CHECK(s.epoch==epoch&&s.dashboard.card==card&&s.dashboard.selection==item);
        }
    return 0;
}
