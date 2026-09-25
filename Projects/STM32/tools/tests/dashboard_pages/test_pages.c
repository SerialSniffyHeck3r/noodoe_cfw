#include "Ui_Home.h"
#include "Dashboard_Pages.h"
#include "Page_Transition.h"
#include "BSP_Calendar.h"
#include "Page_Preview.h"
#include "Development_Data.h"
#include <string.h>
static uint32_t assertions;
#define CHECK(c) do{++assertions;if(!(c))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}
uint32_t test_call_permission_scope(void)
{
    UiState s;Ui_Init(&s,0,0);s.power=UI_RUNNING;s.dashboard.card=UI_CALLS;
    s.calls.phone=(PhoneCallsSnapshot){.epoch=1,.generation=1,.received_ms=1000,.count=1,.permissions=13,.entries={{23,0}}};
    DashboardPage p;UiDashboardPresentation shell={0};DashboardPageFacts f={.now_ms=1000};
    DashboardPages_Present(&s,&shell,&f,&p);
    CHECK(!strcmp(p.title,"Favorites"));CHECK(strcmp(p.note,"Permission Denied!"));
    s.calls.phone.permissions=14;s.calls.phone.entries[0].group=1;
    DashboardPages_Present(&s,&shell,&f,&p);CHECK(!strcmp(p.title,"Recent calls"));CHECK(strcmp(p.note,"Permission Denied!"));
    s.calls.phone.permissions=2;DashboardPages_Present(&s,&shell,&f,&p);CHECK(strcmp(p.note,"Permission Denied!"));
    return 0;
}
uint32_t test_music_buttons(void)
{
    UiState s;Ui_Init(&s,0,0);s.power=UI_RUNNING;s.dashboard.card=UI_MUSIC;
    UiEffect effect;while(Ui_TakeEffect(&s,&effect)){}
    const uint32_t keys[]={UI_UP,UI_UP,UI_DOWN,UI_ENTER};
    const uint32_t durations[]={100,UI_LONG_MS,100,UI_LONG_MS};
    const uint32_t actions[]={UI_MEDIA_TOGGLE,UI_MEDIA_PREVIOUS,UI_MEDIA_NEXT,UI_MEDIA_PHONE};
    for(uint32_t i=0;i<3;++i){
        UiEvent event={UI_EVT_BUTTON,10000*i+100,keys[i],UI_PRESS,0,0};Ui_Dispatch(&s,&event);
        event.b=UI_LONG;event.c=durations[i];Ui_Dispatch(&s,&event);CHECK(!Ui_TakeEffect(&s,&effect));
        event.b=UI_RELEASE;event.now_ms+=durations[i];Ui_Dispatch(&s,&event);
        CHECK(Ui_TakeEffect(&s,&effect)&&effect.kind==UI_FX_MEDIA&&effect.arg==actions[i]);
        CHECK(!Ui_TakeEffect(&s,&effect));CHECK(s.dashboard.footer==UI_ODO);
        event.b=UI_SHORT;Ui_Dispatch(&s,&event);CHECK(!Ui_TakeEffect(&s,&effect));
    }
    UiEvent event={UI_EVT_BUTTON,50000,UI_DOWN,UI_PRESS,0,0};Ui_Dispatch(&s,&event);
    event.b=UI_RELEASE;event.c=UI_LONG_MS;Ui_Dispatch(&s,&event);CHECK(!Ui_TakeEffect(&s,&effect));
    event=(UiEvent){UI_EVT_BUTTON,60000,UI_ENTER,UI_PRESS,0,0};Ui_Dispatch(&s,&event);
    event.b=UI_RELEASE;event.c=100;Ui_Dispatch(&s,&event);CHECK(s.dashboard.card==UI_CALLS);
    return 0;
}
uint32_t test_music_fixture_controls(void)
{
    DevelopmentData_Init();DevelopmentScratch scratch;PhoneContentSlot phone={0};DashboardPageFacts facts={0};
#if DATA_DEBUG
    CHECK(DevelopmentData_MediaAction(0,UI_MEDIA_TOGGLE,1000));
    DevelopmentData_SelectPhone(0);DevelopmentData_Resolve(&facts,&phone,&scratch,3000);
    CHECK(phone.music.playing&&phone.music.position_ms==85000);
    DevelopmentData_SelectPhone(1);DevelopmentData_Resolve(&facts,&phone,&scratch,3000);
    CHECK(phone.music.playing&&phone.music.position_ms==85000&&facts.phone_slot==0);
    CHECK(!DevelopmentData_MediaAction(1,UI_MEDIA_NEXT,3000));
    CHECK(DevelopmentData_MediaAction(0,UI_MEDIA_NEXT,3000));DevelopmentData_Resolve(&facts,&phone,&scratch,3100);
    CHECK(!strcmp(phone.music.title,"City lights")&&phone.music.position_ms==100);
    CHECK(DevelopmentData_MediaAction(0,UI_MEDIA_PREVIOUS,3100));DevelopmentData_Resolve(&facts,&phone,&scratch,3200);
    CHECK(!strcmp(phone.music.title,"Night ride"));

#else
    CHECK(!DevelopmentData_MediaAction(0,UI_MEDIA_TOGGLE,1000));
    DevelopmentData_Resolve(&facts,&phone,&scratch,3000);CHECK(!phone.music.valid);
#endif
    CHECK(!DevelopmentData_MediaAction(2,0,1000));CHECK(!DevelopmentData_MediaAction(0,3,1000));
    DevelopmentData_Init();return 0;
}
static void Event(UiState *s,uint32_t kind,uint32_t now,uint32_t a,uint32_t b,uint32_t c)
{
    UiEvent e={kind,now,a,b,c,0};Ui_Dispatch(s,&e);
    /* This presenter fixture supplies the renderer's completed wake handshake. */
    if(kind==UI_EVT_IGN&&a&&s->display_wait){
        UiEvent ready={UI_EVT_DISPLAY_READY,now,s->epoch,0,0,0};Ui_Dispatch(s,&ready);
    }
    UiEffect f;while(Ui_TakeEffect(s,&f)){};
}
static void Key(UiState *s,uint32_t button,uint32_t duration,uint32_t now)
{Event(s,UI_EVT_BUTTON,now,button,UI_PRESS,0);Event(s,UI_EVT_BUTTON,now+duration,button,UI_RELEASE,duration);}

uint32_t test_trip(void)
{
    TripComputer s;TripComputer_Init(&s);CHECK(!s.records[TRIP_REFUEL].valid);
    TripComputer_Tick(&s,0,1,1,36,20260913);
    for(uint32_t t=1000;t<=100000;t+=1000)TripComputer_Tick(&s,t,1,1,36,20260913);
    CHECK(s.records[TRIP_A].distance_mm==1000000ULL);CHECK(s.records[TRIP_A].moving_ms==100000U);
    CHECK(s.records[TRIP_A].max_kph==36&&s.records[TRIP_B].distance_mm==1000000U);
    TripComputer_Tick(&s,100000,1,1,0,20260913);
    for(uint32_t t=101000;t<=200000;t+=1000)TripComputer_Tick(&s,t,1,1,0,20260913);
    CHECK(s.records[TRIP_A].stopped_ms==100000U);
    UiState ui;Ui_Init(&ui,0,0);ui.dashboard.card=UI_TRIP;
    UiDashboardPresentation shell={0};DashboardPage page;DashboardPageFacts facts={.trips=&s};
    DashboardPages_Present(&ui,&shell,&facts,&page);
    CHECK(!strcmp(page.numbers[2],"36")&&!strcmp(page.numbers[3],"18.0"));
    CHECK(!strcmp(page.numbers[4],"1.0")&&!page.numbers[5][0]);CHECK(page.ratio_permille==500);
    facts.units=1;DashboardPages_Present(&ui,&shell,&facts,&page);
    CHECK(!strcmp(page.numbers[2],"22")&&!strcmp(page.numbers[3],"11.1")&&!strcmp(page.numbers[4],"0.6"));
    TripComputer_Tick(&s,201000,1,0,0,20260913);CHECK(s.records[0].unknown_ms==1000U&&s.records[0].stopped_ms==100000U);
    TripComputer_Tick(&s,205000,1,1,36,20260913);CHECK(s.records[0].unknown_ms==5000U);
    TripComputer_Tick(&s,206000,0,1,36,20260913);CHECK(s.records[0].distance_mm==1000000U);
    TripComputer_Tick(&s,207000,1,1,36,20260914);CHECK(s.records[TRIP_TODAY].distance_mm==0U);
    CHECK(TripComputer_Reset(&s,TRIP_A));CHECK(!s.records[0].distance_mm&&s.records[1].distance_mm==1000000U);
    CHECK(!TripComputer_Reset(&s,TRIP_REFUEL));
    TripComputer_Init(&s);TripComputer_Tick(&s,0xfffffff0U,1,1,36,20260913);
    TripComputer_Tick(&s,984U,1,1,36,20260913);CHECK(s.records[0].distance_mm==10000U);
    TripComputer_Init(&s);TripComputer_Tick(&s,0,1,1,1,20260913);
    for(uint32_t t=1;t<=3600U;++t)TripComputer_Tick(&s,t,1,1,1,20260913);
    CHECK(s.records[0].distance_mm==1000U&&s.records[0].stopped_ms==3600U);
    /* Every supported threshold: strict less-than, with stationary-only zero.
     * Slow motion still accumulates distance; a setting never rewrites history. */
    for(uint32_t threshold=0;threshold<=10U;++threshold){
        for(uint32_t kph=0;kph<=11U;++kph){
            TripComputer_Init(&s);CHECK(s.stop_speed_kph==5U);
            CHECK(TripComputer_SetStopSpeed(&s,threshold));
            TripComputer_Tick(&s,0,1,1,kph,20260917);
            TripComputer_Tick(&s,1000,1,1,kph,20260917);
            uint32_t stopped=threshold?kph<threshold:kph==0;
            CHECK(s.records[0].stopped_ms==(stopped?1000U:0U));
            CHECK(s.records[0].moving_ms==(stopped?0U:1000U));
            CHECK(s.records[0].distance_mm==(uint64_t)kph*1000U*5U/18U);
            CHECK(!TripComputer_SetStopSpeed(&s,11)&&s.stop_speed_kph==threshold);
        }
    }
    TripComputer_Init(&s);TripComputer_Tick(&s,0,1,1,4,20260917);
    CHECK(TripComputer_SetStopSpeed(&s,0));TripComputer_Tick(&s,1000,1,1,4,20260917);
    CHECK(s.records[0].stopped_ms==1000&&!s.records[0].moving_ms);
    TripComputer_Tick(&s,2000,1,1,4,20260917);
    CHECK(s.records[0].stopped_ms==1000&&s.records[0].moving_ms==1000);
    TripComputer_Tick(&s,3000,1,0,0,20260917);
    CHECK(s.records[0].unknown_ms==1000&&s.records[0].stopped_ms==1000);
    return 0;
}

uint32_t test_refuel(void)
{
    TripComputer s;TripComputer_Init(&s);
    for(uint32_t t=0;t<=4000;t+=500)CHECK(!TripComputer_Fuel(&s,t,1,1));
    CHECK(s.fuel_known&&s.fuel_base==1);
    for(uint32_t t=4500;t<=8500;t+=500)CHECK(!TripComputer_Fuel(&s,t,1,2));
    CHECK(!s.refuel_count&&!s.records[3].valid);
    CHECK(!TripComputer_Fuel(&s,9000,1,3));CHECK(!TripComputer_Fuel(&s,9500,0,3));
    for(uint32_t t=10000;t<13000;t+=500)CHECK(!TripComputer_Fuel(&s,t,1,3));
    CHECK(TripComputer_Fuel(&s,13000,1,3));CHECK(s.refuel_count==1&&s.records[3].valid);
    for(uint32_t t=13500;t<20000;t+=500)CHECK(!TripComputer_Fuel(&s,t,1,3));
    CHECK(s.refuel_count==1);CHECK(!TripComputer_Fuel(&s,20000,1,6));
    /* Invalid/raw unknown levels cannot cause a synthetic fill. */
    CHECK(!TripComputer_Fuel(&s,20500,0,5));return 0;
}

uint32_t test_pages_and_locks(void)
{
    UiState s;Ui_Init(&s,0,0);CHECK(s.dashboard.card==UI_BLANK);
    Event(&s,UI_EVT_IGN,0,1,0,0);Event(&s,UI_EVT_TICK,2400,0,0,0);CHECK(s.power==IGN_ON);
    const uint32_t cards[7]={UI_BLANK,UI_TRIP,UI_NOTIFICATIONS,UI_MUSIC,UI_CALLS,UI_PHONE_GPS,UI_SYSTEM};
    for(uint32_t i=1;i<=7;++i){Key(&s,UI_ENTER,100,3000+i*200);CHECK(s.dashboard.card==cards[i%7]);}
    s.dashboard.card=UI_NOTIFICATIONS;Event(&s,UI_EVT_PHONE_COUNT,5200,4,0,0);
    Event(&s,UI_EVT_SPEED,5300,1,50,0);Key(&s,UI_DOWN,100,5400);CHECK(s.notification_open&&s.dashboard.selection==1);Key(&s,UI_DOWN,100,5450);CHECK(s.dashboard.selection==2);
    Event(&s,UI_EVT_SPEED,5500,1,51,0);CHECK(!s.dashboard.selection);Key(&s,UI_DOWN,100,5600);CHECK(!s.dashboard.selection);
    Event(&s,UI_EVT_SPEED,5800,0,0,0);Key(&s,UI_DOWN,100,5900);CHECK(!s.dashboard.selection);
    s.dashboard.card=UI_SYSTEM;Event(&s,UI_EVT_SPEED,6100,1,3,0);Key(&s,UI_ENTER,2200,6200);CHECK(!s.menu);
    Event(&s,UI_EVT_SPEED,8500,1,2,0);Key(&s,UI_ENTER,2200,8600);CHECK(!s.menu);
    /* Stationary hold and editor navigation are tested through SettingsUI;
     * the riding child must not open an independent duplicate settings menu. */
    UiDashboardPresentation shell={0};DashboardPage page;DashboardPageFacts f={.date=20260913,.weekday=7};
    s.menu=0;s.dashboard.card=UI_BLANK;s.dashboard.selection=1;DashboardPages_Present(&s,&shell,&f,&page);CHECK(!page.title[0]&&!strcmp(page.note,"Sep. 13 Sun"));
    BSP_CalendarDateTime utc={2026,12,31,0,16,0,0},local;CHECK(BSP_Calendar_AddSeconds(&utc,32400,&local));CHECK(local.year==2027&&local.month==1&&local.day==1&&local.hour==1);
    return 0;
}

uint32_t test_phone(void)
{
    static PhoneContent s;static PhoneStatus status;static PhoneMusic music;
    PhoneContent_Init(&s);PhoneContent_Links(&s,3);uint32_t a=s.slots[0].token;
    CHECK(a&&PHONE_CONTENT_SLOTS==1U);status=(PhoneStatus){.battery_valid=1,.battery_percent=68,.notifications_valid=1,.count=2};
    status.header_key=1;status.notifications[0].visual_key=2;status.notifications[1].visual_key=3;strcpy(status.notifications[0].title,"Newest");strcpy(status.notifications[1].title,"Older");
    CHECK(PhoneContent_Status(&s,0,a,&status,100));CHECK(!PhoneContent_Status(&s,1,a,&status,100));
    status.battery_percent=101;CHECK(!PhoneContent_Status(&s,0,a,&status,101));
    CHECK(s.slots[0].status.battery_percent==68);
    music=(PhoneMusic){.valid=1,.playing=1,.position_ms=10000,.duration_ms=20000};strcpy(music.title,"Track");
    CHECK(PhoneContent_Music(&s,0,a,&music,100));
    UiState ui;Ui_Init(&ui,0,0);ui.dashboard.card=UI_NOTIFICATIONS;ui.speed_valid=1;ui.speed_kph=50;ui.dashboard.selection=1;
    UiDashboardPresentation shell={0};DashboardPage p;DashboardPageFacts f={.phone=&s.slots[0],.now_ms=100};
    DashboardPages_Present(&ui,&shell,&f,&p);CHECK(p.visual_key==3);ui.notification_open=1;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(p.visual_key==3);
    s.slots[0].status.count=0;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(!strcmp(p.lines[0],"No notification"));s.slots[0].status.count=2;
    ui.speed_kph=51;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(p.visual_key==2);
    ui.dashboard.card=UI_MUSIC;f.now_ms=15100;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(p.known&&p.ratio_permille==1000);
    f.now_ms=15101;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(!p.known);
    PhoneContent_Links(&s,2);PhoneContent_Links(&s,3);CHECK(s.slots[0].token!=a&&!s.slots[0].status_revision);
    CHECK(!PhoneContent_Status(&s,0,a,&status,200));CHECK(!PhoneContent_Status(&s,1,a,&status,200));return 0;
}

uint32_t test_transition(void)
{
    PageTransition t={0};PageTransitionFrame f;CHECK(PageTransition_Request(&t,0,0)==2);
    CHECK(PageTransition_Request(&t,1,100)==1);CHECK(!PageTransition_Step(&t,100,&f));CHECK(f.outgoing_alpha==255&&f.incoming_alpha==0&&f.incoming_x==28);
    uint32_t prev=0;for(uint32_t ms=100;ms<340;++ms){CHECK(!PageTransition_Step(&t,ms,&f));CHECK(f.incoming_alpha>=prev&&f.incoming_alpha+f.outgoing_alpha==255);prev=f.incoming_alpha;CHECK(f.outgoing_x<=0&&f.outgoing_x>=-28&&f.incoming_x>=0&&f.incoming_x<=28);}
    CHECK(!PageTransition_Request(&t,7,200));CHECK(t.target==1&&t.pending==7);
    CHECK(PageTransition_Step(&t,340,&f)&&t.current==1);
    CHECK(PageTransition_Request(&t,7,340)==1);CHECK(PageTransition_Step(&t,580,&f)&&t.current==7);
    CHECK(PageTransition_Request(&t,2,0xfffffff0U)==1);CHECK(PageTransition_Step(&t,224U,&f));return 0;
}

uint32_t test_trail(void)
{
    PhoneTrail s;int16_t xy[PHONE_TRAIL_POINTS][2];PhoneTrail_Init(&s);
    PhoneTrail_Feed(&s,0,1,370000000,1270000000);PhoneTrail_Feed(&s,1000,1,370005000,1270005000);
    CHECK(s.display.lat!=s.target.lat);PhoneTrail_RenderStep(&s,1300);
    CHECK(PhoneTrail_Project(&s,xy,288,168)==2);CHECK(xy[1][0]==144&&xy[1][1]==84);CHECK(xy[1][0]>xy[0][0]&&xy[1][1]<xy[0][1]);
    for(uint32_t i=2;i<200;++i)PhoneTrail_Feed(&s,i*1000,1,370000000+(int32_t)i*1000,1270000000+(int32_t)i*1000);
    PhoneTrail_RenderStep(&s,199300);CHECK(s.count<=PHONE_TRAIL_POINTS);uint32_t n=PhoneTrail_Project(&s,xy,288,168);
    CHECK(n&&xy[n-1][0]==144&&xy[n-1][1]==84); /* Fixed zoom clips history; never rescales. */
    PhoneTrail_Feed(&s,200000,0,0,0);CHECK(!s.live&&s.count);
    PhoneTrail_Feed(&s,211000,1,0,1799999000);CHECK(s.count>1);
    PhoneTrail_Init(&s);CHECK(s.count==0&&!s.live);
    PhoneTrail_Feed(&s,211000,1,0,1799999000);CHECK(s.count==1);
    PhoneTrail_Feed(&s,212000,1,0,-1799999000);PhoneTrail_RenderStep(&s,212300);CHECK(PhoneTrail_Project(&s,xy,288,168)==2&&xy[1][0]>xy[0][0]);return 0;
}

uint32_t test_preview_isolation(void)
{
    UiState s,original;Ui_Init(&s,0,0);original=s;
    UiDashboardPresentation shell={0};DashboardPage p={0};DashboardPageFacts facts={0};
    PagePreview_Init();g_page_preview.card=UI_MUSIC;g_page_preview.flags=2;
    g_page_preview.ttl_ms=1000;g_page_preview.request_id=1;
    CHECK(PagePreview_Apply(&s,&shell,&facts,&p,100)==100);
#if DATA_DEBUG
    CHECK(g_page_preview.ack_id==1&&!p.art_valid&&!strcmp(p.lines[0],"No phone connected"));
    CHECK(PagePreview_Apply(&s,&shell,&facts,&p,500)==220);
    CHECK(!memcmp(&s,&original,sizeof(s)));
    CHECK(PagePreview_Apply(&s,&shell,&facts,&p,1100)==1100&&!g_page_preview.active_id);
    g_page_preview.card=99;g_page_preview.request_id=2;
    CHECK(PagePreview_Apply(&s,&shell,&facts,&p,1200)==1200&&g_page_preview.result==2);
    g_page_preview.card=UI_TRIP;g_page_preview.flags=0;g_page_preview.request_id=3;
    (void)PagePreview_Apply(&s,&shell,&facts,&p,1300);CHECK(g_page_preview.active_id==3);
    PagePreview_Cancel();CHECK(!g_page_preview.active_id);
#else
    CHECK(g_page_preview.ack_id==1&&g_page_preview.result==2&&!g_page_preview.active_id);
    CHECK(!p.key&&!p.lines[0][0]&&!memcmp(&s,&original,sizeof(s)));
#endif
    return 0;
}

uint32_t test_easing_axes(void)
{
    CHECK(PageTransition_Ease(0)==0&&PageTransition_Ease(120)==512&&PageTransition_Ease(240)==1024);
    CHECK(PageTransition_Ease(500)==1024);
    uint32_t slow1=PageTransition_Ease(30)-PageTransition_Ease(0),fast=PageTransition_Ease(135)-PageTransition_Ease(105);
    uint32_t slow2=PageTransition_Ease(240)-PageTransition_Ease(210);CHECK(fast>slow1*3&&fast>slow2*3);
    PageTransition t={0};PageTransitionFrame f;PageTransitionPose h,v,u;
    PageTransition_Request(&t,0,0);PageTransition_Request(&t,1,100);
    for(uint32_t ms=100;ms<=340;++ms){PageTransition_Step(&t,ms,&f);
        PageTransition_Project(&f,PAGE_AXIS_HORIZONTAL,1,&h);PageTransition_Project(&f,PAGE_AXIS_VERTICAL,1,&v);
        PageTransition_Project(&f,PAGE_AXIS_VERTICAL,-1,&u);
        CHECK(!h.incoming_y&&!h.outgoing_y&&!v.incoming_x&&!v.outgoing_x);
        CHECK(h.incoming_x==v.incoming_y&&h.outgoing_x==v.outgoing_y);
        CHECK(u.incoming_y==-v.incoming_y&&u.outgoing_y==-v.outgoing_y);
    }
    UiState s;Ui_Init(&s,0,0);Event(&s,UI_EVT_IGN,0,1,0,0);Event(&s,UI_EVT_TICK,2400,0,0,0);CHECK(s.power==IGN_ON);
    s.dashboard.card=UI_TRIP;Key(&s,UI_UP,100,3000);CHECK(s.dashboard.selection==3&&s.dashboard.item_direction==2);
    Key(&s,UI_DOWN,100,3300);CHECK(!s.dashboard.selection&&s.dashboard.item_direction==1);
    Key(&s,UI_ENTER,100,3600);CHECK(s.dashboard.card==UI_NOTIFICATIONS&&!s.dashboard.selection);
    return 0;
}
uint32_t test_development_path(void)
{
    static PhoneContent live,original;static PhoneContentSlot scratch;
    DevelopmentSample sample;DevelopmentScratch work;DevelopmentData_Init();DevelopmentData_Default(&sample);
    PhoneContent_Init(&live);PhoneContent_Links(&live,1);original=live;
    TripComputer real;TripComputer_Init(&real);TripComputer_Tick(&real,0,1,1,60,20260913);
    TripComputer before=real;DashboardPageFacts f={.trips=&real,.phone=&live.slots[0],.date=20260913,.weekday=7};
#if DATA_DEBUG
    CHECK(g_product_data.version==2&&g_product_data.active_mask==1);
    /* Automatic fixtures stay available across minutes without a host request. */
    DevelopmentData_Poll(180001);DevelopmentData_Resolve(&f,&scratch,&work,180001);
    CHECK(f.development_mask==1&&scratch.music.valid&&scratch.music.art_valid&&f.trips==&real);
    sample.mask|=DEVELOPMENT_TRIPS; /* Explicit trip fixture only. */
    CHECK(DevelopmentData_Request(&sample,1000));CHECK(!DevelopmentData_Request(&sample,1000));
    DevelopmentData_Poll(100);CHECK(g_product_data.active_mask==3&&g_product_data.result==0);
    DevelopmentData_Resolve(&f,&scratch,&work,100);CHECK(f.phone==&scratch&&f.trips!=&real&&f.trail);
    CHECK(f.development_mask==3&&scratch.status.battery_percent==68&&scratch.music.art_valid==1);
    CHECK(!memcmp(&live,&original,sizeof(live))&&!memcmp(&real,&before,sizeof(real)));
    UiState ui;Ui_Init(&ui,0,0);ui.dashboard.card=UI_TRIP;UiDashboardPresentation shell={0};DashboardPage page;
    DashboardPages_Present(&ui,&shell,&f,&page);CHECK(!strcmp(page.numbers[0],"1:23")&&!strcmp(page.numbers[2],"112"));
    CHECK(!strcmp(page.lines[4],"km")&&!strcmp(page.note,"TEST DATA / not saved"));
    ui.dashboard.selection=1;DashboardPages_Present(&ui,&shell,&f,&page);CHECK(!strcmp(page.numbers[0],"1:40"));
    sample.mask=DEVELOPMENT_REMOTE;CHECK(DevelopmentData_Request(&sample,1000));DevelopmentData_Poll(200);
    f=(DashboardPageFacts){.trips=&real,.phone=&live.slots[0]};DevelopmentData_Resolve(&f,&scratch,&work,200);
    CHECK(f.trips==&real&&f.phone==&scratch&&f.development_mask==1);
    DevelopmentData_Poll(1200);CHECK(g_product_data.active_mask==1&&!g_product_data.expires_ms);
    f=(DashboardPageFacts){.trips=&real,.phone=&live.slots[0]};DevelopmentData_Resolve(&f,&scratch,&work,1200);
    CHECK(f.phone==&scratch&&f.trips==&real&&f.development_mask==1);
    CHECK(!memcmp(&live,&original,sizeof(live))&&!memcmp(&real,&before,sizeof(real)));
    sample.battery_percent=101;CHECK(!DevelopmentData_Request(&sample,1000));sample.battery_percent=68;
    CHECK(!DevelopmentData_Request(&sample,999)&&!DevelopmentData_Request(&sample,180001));
    CHECK(DevelopmentData_Request(&sample,1000));DevelopmentData_Poll(0xfffffff0U);DevelopmentData_Poll(983);
    CHECK(g_product_data.active_mask==1);DevelopmentData_Poll(984);CHECK(g_product_data.active_mask==1);
    CHECK(DevelopmentData_Request(0,0));DevelopmentData_Poll(985);CHECK(g_product_data.active_mask==1);
    /* Data fixture is no longer a side effect of selecting a preview page. */
    PagePreview_Init();g_page_preview.card=UI_TRIP;g_page_preview.flags=1;g_page_preview.ttl_ms=1000;g_page_preview.request_id=1;
    (void)PagePreview_Apply(&ui,&shell,&f,&page,1000);CHECK(g_page_preview.result==2&&g_product_data.active_mask==1);
#else
    CHECK(g_product_data.version==2&&!g_product_data.active_mask);
    CHECK(!DevelopmentData_Request(&sample,1000)&&!DevelopmentData_Request(0,0));
    /* Even a raw mailbox mask cannot bypass the global live-source build. */
    g_product_data.active_mask=3;g_product_data.command=2;g_product_data.request_id=1;
    DevelopmentData_Resolve(&f,&scratch,&work,100);
    CHECK(f.phone==&live.slots[0]&&f.trips==&real&&!f.development_mask);
    DevelopmentData_Poll(100);CHECK(!g_product_data.active_mask&&g_product_data.ack_id==1&&g_product_data.result==2);
    CHECK(!memcmp(&live,&original,sizeof(live))&&!memcmp(&real,&before,sizeof(real)));
#endif
    return 0;
}

uint32_t test_home_views(void)
{
    UiState s={0};DashboardPageFacts f={.date=20260916,.weekday=3};DashboardPage p={0};
    UiDashboardPresentation shell={0};s.dashboard.card=UI_BLANK;s.speed_valid=1;s.speed_kph=97;
    DashboardPages_Present(&s,&shell,&f,&p);CHECK(!p.note[0]&&!p.numbers[0][0]);
    s.dashboard.selection=1;DashboardPages_Present(&s,&shell,&f,&p);
    CHECK(!strcmp(p.note,"Sep. 16 Wed")&&!p.numbers[0][0]&&p.key==256);
    s.dashboard.selection=2;DashboardPages_Present(&s,&shell,&f,&p);
    CHECK(!strcmp(p.numbers[0],"97")&&!strcmp(p.lines[0],"km/h")&&p.key==512);
    f.units=1;DashboardPages_Present(&s,&shell,&f,&p);CHECK(!strcmp(p.numbers[0],"60")&&!strcmp(p.lines[0],"mph"));
    s.speed_valid=0;f.date=0;DashboardPages_Present(&s,&shell,&f,&p);CHECK(!strcmp(p.numbers[0],"---")&&!strcmp(p.note,"Date unavailable"));
    CHECK(UiHome_StripVisible(UI_MUSIC,100));CHECK(UiHome_StripVisible(UI_BLANK,200));
    CHECK(UiHome_StripVisible(UI_BLANK,5199));CHECK(!UiHome_StripVisible(UI_BLANK,5200));
    CHECK(!UiHome_StripVisible(UI_BLANK,9000));CHECK(UiHome_StripVisible(UI_TRIP,9001));
    CHECK(UiHome_StripVisible(UI_BLANK,0xFFFFFF00U));CHECK(!UiHome_StripVisible(UI_BLANK,4744));
    return 0;
}

uint32_t test_trail_controls_and_jitter(void)
{
 PhoneTrail s;PhoneTrail_Init(&s);int16_t xy[PHONE_TRAIL_POINTS][2];
 PhoneTrail_FeedHeading(&s,1000,1,370000000,1270000000,1,359000);
 PhoneTrail_FeedHeading(&s,2000,1,370000001,1270000001,1,1000);
 CHECK(s.north>1000&&s.east>-30&&s.east<30&&s.count==1);
 int32_t e=s.east,n=s.north;
 PhoneTrail_FeedHeading(&s,2000,1,370000001,1270000001,1,180000);CHECK(s.east==e&&s.north==n);
 PhoneTrail_Feed(&s,3000,1,370000002,1270000002);CHECK(s.count==1&&s.east==e&&s.north==n);
 PhoneTrail_Feed(&s,4000,1,380000000,1280000000);CHECK(s.count==1&&s.rejected==1);
 PhoneTrail_FeedHeading(&s,5000,1,370000000,1270010000,1,90000);
 PhoneTrail_RenderStep(&s,5300);
 CHECK(s.count==2&&s.east>700);
 CHECK(PhoneTrail_ProjectView(&s,xy,288,168,1,2)==2);CHECK(xy[0][1]>84);
 int16_t previous=xy[0][1];PhoneTrail_ProjectView(&s,xy,288,168,1,1);CHECK(xy[0][1]>previous);
 PhoneTrail_Feed(&s,16000,1,370010000,1270010000);CHECK(s.count==3&&s.points[2].gap);
 UiState ui;Ui_Init(&ui,0,0);Event(&ui,UI_EVT_IGN,0,1,0,0);Event(&ui,UI_EVT_TICK,2400,0,0,0);
 ui.dashboard.card=UI_PHONE_GPS;CHECK(ui.dashboard.gps_zoom==2);
 Key(&ui,UI_UP,100,3000);CHECK(ui.dashboard.gps_zoom==1);
 Key(&ui,UI_DOWN,100,3300);CHECK(ui.dashboard.gps_zoom==2);
 Key(&ui,UI_ENTER,2200,3600);CHECK(ui.dashboard.gps_heading_up==1&&ui.dashboard.card==UI_PHONE_GPS);
 DashboardPage p;UiDashboardPresentation shell={0};DashboardPageFacts f={.trail=&s};DashboardPages_Present(&ui,&shell,&f,&p);CHECK(!p.note[0]&&p.map_scale==100);
 s.live=0;DashboardPages_Present(&ui,&shell,&f,&p);CHECK(!strcmp(p.note,"Waiting for phone GPS"));return 0;
}
uint32_t test_odometer_calibration(void)
{
 TripComputer s;TripComputer_Init(&s);
 /* Dash speed72km/h, actual68km/h. Two complete ODO edges train independently
  * of the user's reported13.6/14.4 example; a third km matches within10m. */
 for(uint32_t t=0;t<=180000;t+=100){TripComputer_Tick(&s,t,1,1,72,20260923);TripComputer_Odometer(&s,1,100+t*68/3600000);}
 CHECK(s.distance_q16>61000&&s.distance_q16<63000);
 uint64_t before=s.records[0].distance_mm;
 for(uint32_t t=180100;t<=360000;t+=100){TripComputer_Tick(&s,t,1,1,72,20260923);TripComputer_Odometer(&s,1,100+t*68/3600000);}
 uint64_t delta=s.records[0].distance_mm-before;CHECK(delta>3390000&&delta<3410000);
 uint32_t scale=s.distance_q16;TripComputer_Odometer(&s,1,1);CHECK(s.distance_q16==scale);
 TripComputer_Odometer(&s,0,0);CHECK(!s.odo_anchor);
 CHECK(TripComputer_Reset(&s,0)&&s.records[0].distance_mm==0&&s.distance_q16==scale);return 0;
}
