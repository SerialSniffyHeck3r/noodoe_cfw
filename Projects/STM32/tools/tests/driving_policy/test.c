#include "InputMode.h"
#include "Ignition_Session.h"
#include "BSP_Buttons.h"
#include "Phone_Trail.h"
#include "Wallpaper.h"
#include <string.h>
static uint32_t assertions;
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
uint32_t get_assertions(void){return assertions;}
uint32_t test_input(void)
{
    CHECK(BSP_BUTTONS_LONG_PRESS_MS==800U);
    for(uint32_t ign=0;ign<2;ign++)for(uint32_t high=0;high<2;high++)for(uint32_t b=0;b<3;b++){
        InputMode_Init(0);InputMode_Sample(high,ign,0,0);
        uint32_t allowed=!ign||high;
        CHECK(g_input_mode.allowed==allowed);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_PRESS)==(allowed?INPUT_MODE_ACCEPT:INPUT_MODE_NOTIFY));
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_PRESS)==INPUT_MODE_DROP);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_LONG_PRESS)==(allowed?INPUT_MODE_ACCEPT:INPUT_MODE_DROP));
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_RELEASE)==(allowed?INPUT_MODE_ACCEPT:INPUT_MODE_DROP));
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_SHORT_PRESS)==INPUT_MODE_DROP);
        CHECK(g_input_mode.blocked_presses==!allowed);
    }
    for(uint32_t b=0;b<3;b++){
        uint32_t bit=1U<<b;InputMode_Init(0);InputMode_Sample(1,1,0,0);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_PRESS)==INPUT_MODE_ACCEPT);
        CHECK(InputMode_Sample(0,1,bit,1)==0);
        CHECK(InputMode_Sample(0,1,bit,100)==0);
        CHECK(InputMode_Sample(0,1,bit,101)==bit);CHECK(InputMode_Sample(0,1,bit,102)==0);
        InputMode_Sample(1,1,bit,103);InputMode_Sample(1,1,bit,203);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_LONG_PRESS)==INPUT_MODE_DROP);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_RELEASE)==INPUT_MODE_DROP);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_PRESS)==INPUT_MODE_ACCEPT);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_RELEASE)==INPUT_MODE_ACCEPT);
        InputMode_Init(bit);InputMode_Sample(1,1,bit,0);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_RELEASE)==INPUT_MODE_DROP);
        CHECK(InputMode_Event(b,BSP_BUTTON_EVENT_PRESS)==INPUT_MODE_ACCEPT);
    }return 0;
}
uint32_t test_selector_chatter(void)
{
    InputMode_Init(0);InputMode_Sample(1,1,0,0xfffffff0U);
    for(uint32_t i=0;i<1000;i++){
        InputMode_Sample(i&1,1,0,0xfffffff1U+i*5U);
        CHECK(g_input_mode.allowed&&g_input_mode.stable_high);
    }
    InputMode_Sample(0,1,0,6000);InputMode_Sample(0,1,0,6099);CHECK(g_input_mode.allowed);
    InputMode_Sample(0,1,0,6100);CHECK(!g_input_mode.allowed);
    InputMode_Sample(0,0,0,6101);CHECK(g_input_mode.allowed);
    InputMode_Sample(0,1,0,6102);CHECK(!g_input_mode.allowed);
    return 0;
}
uint32_t test_session_markers(void)
{
    IgnitionSession s={0};
    IgnitionSession_Tick(&s,0,1,1,0);
    for(uint32_t t=1000;t<=10000;t+=1000)IgnitionSession_Tick(&s,t,1,1,t/100);
    CHECK(s.peak_kph==100&&s.have_speed&&s.known_ms==10000&&s.speed_ms2==1000000);
    IgnitionSession_Tick(&s,11000,1,1,0);CHECK(s.peak_kph==100);
    IgnitionSession_Tick(&s,12000,1,1,0);CHECK(s.known_ms==12000&&s.speed_ms2==1100000);
    IgnitionSession_Tick(&s,16000,1,0,400);CHECK(s.peak_kph==100&&s.known_ms==12000);
    IgnitionSession_Tick(&s,17000,0,0,400);CHECK(!s.have_speed&&!s.known_ms&&!s.peak_kph);
    IgnitionSession_Tick(&s,18000,1,1,20);CHECK(s.peak_kph==20&&s.have_speed&&!s.known_ms);
    IgnitionSession_Tick(&s,19000,1,1,401);CHECK(s.peak_kph==20&&!s.known_ms);
    return 0;
}
uint32_t test_off_photo(void)
{
    BackgroundImage pics[3],out,album={.pixels=(void*)0xD0100000,.width=32,.height=32,.bytes=2048,.revision=9};
    Wallpaper_Init();Wallpaper_SelectPhoto(1);
    for(uint32_t i=0;i<3;i++){
        pics[i]=(BackgroundImage){.pixels=(void*)(0xD0000000+i*0x100000),.width=480,.height=480,.bytes=460800,.revision=i+1};
        CHECK(Wallpaper_SetPhoto(i,&pics[i]));
    }
    for(uint32_t choice=0;choice<4;choice++){
        CHECK(Wallpaper_SetOffPhoto(choice));
        CHECK(Wallpaper_Resolve(1,&album,&out)&&out.revision==9);
        CHECK(Wallpaper_Resolve(2,&album,&out)&&out.revision==2); /* Summary always normal photo. */
        CHECK(Wallpaper_Resolve(3,&album,&out)&&out.revision==(choice?choice:2));
    }
    CHECK(!Wallpaper_SetOffPhoto(4));Wallpaper_SetPhoto(2,0);
    CHECK(Wallpaper_Resolve(3,&album,&out)&&out.revision==2);
    WallpaperState s;Wallpaper_GetState(&s);CHECK(s.off_photo==3&&s.fallback==1);
    Wallpaper_SetPhoto(1,0);CHECK(!Wallpaper_Resolve(3,&album,&out));
    Wallpaper_GetState(&s);CHECK(s.off_photo==3&&s.fallback==2);return 0;
}
uint32_t test_camera(void)
{
    PhoneTrail s;PhoneTrail_Init(&s);
    PhoneTrail_FeedHeading(&s,1000,1,370000000,1270000000,1,359000);
    PhoneTrail_RenderStep(&s,1300);int32_t old=s.display.lat;
    PhoneTrail_FeedHeading(&s,2000,1,370000900,1270000000,1,1000);
    CHECK(s.display.lat==old);PhoneTrail_RenderStep(&s,2150);
    CHECK(s.display.lat>old&&s.display.lat<s.target.lat);
    CHECK(s.display_course<2000||s.display_course>358000);
    int32_t middle=s.display.lat;PhoneTrailPoint saved=s.points[1];
    PhoneTrail_FeedHeading(&s,2150,1,370001800,1270000000,1,2000);
    CHECK(s.display.lat==middle);PhoneTrail_RenderStep(&s,2450);
    CHECK(s.display.lat==s.target.lat);CHECK(!memcmp(&saved,&s.points[1],sizeof(saved)));
    int16_t grid[PHONE_TRAIL_GRID_LINES][4],before[PHONE_TRAIL_GRID_LINES][4];
    CHECK(PhoneTrail_ProjectGrid(&s,grid,312,200,0,2)==PHONE_TRAIL_GRID_LINES);memcpy(before,grid,sizeof(grid));
    PhoneTrail_FeedHeading(&s,3000,1,370002700,1270000000,1,2000);PhoneTrail_RenderStep(&s,3300);
    CHECK(PhoneTrail_ProjectGrid(&s,grid,312,200,0,2)==PHONE_TRAIL_GRID_LINES);CHECK(memcmp(before,grid,sizeof(grid)));
    uint32_t count=s.count;PhoneTrail_Feed(&s,2999,1,370003000,1270000000);CHECK(s.count==count);
    PhoneTrail_Feed(&s,3400,1,500000000,1270000000);CHECK(s.count==count&&s.rejected);
    PhoneTrail_Feed(&s,20000,1,500000000,1270000000);CHECK(s.display.lat==500000000&&s.points[s.count-1].gap);
    return 0;
}

#include "BacklightPolicy.h"
uint32_t test_backlight(void)
{
    for(uint32_t i=0;i<10;i++){
        CHECK(BacklightPolicy_Target(i,0)==(i==9?99:(i+1)*10));
        CHECK(BacklightPolicy_Target(i,2)>=BacklightPolicy_Target(i,0));
        CHECK(BacklightPolicy_Target(i,-2)<=BacklightPolicy_Target(i,0));
    }
    BacklightPolicy p={0};uint32_t current=50;
    CHECK(BacklightPolicy_Step(&p,100,1,1,1,9,0,25,current)==50);
    CHECK(BacklightPolicy_Step(&p,1099,1,1,1,9,0,25,current)==50);
    CHECK(BacklightPolicy_Step(&p,1200,1,1,1,9,0,25,current)==52);
    CHECK(BacklightPolicy_Step(&p,1300,0,1,1,9,0,25,0)==0);
    CHECK(BacklightPolicy_Step(&p,1400,1,0,1,9,0,25,50)==25);
    CHECK(BacklightPolicy_Step(&p,1410,1,0,1,9,0,100,25)==100);
    CHECK(BacklightPolicy_Step(&p,1520,1,1,0,9,0,25,50)==48);
    CHECK(!p.source_valid&&p.target==25);
    CHECK(BacklightPolicy_Step(&p,1630,1,1,1,10,0,25,50)==48);
    CHECK(!p.source_valid);
    return 0;
}
