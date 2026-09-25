#include "SpeedHome_Model.h"
#include "SpeedHome_Startup.h"
#include <stdint.h>
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x))return __LINE__;}while(0)
static int same(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
unsigned test_speed_model(void)
{
    SpeedHomeModel m;SpeedHome_InitModel(&m,0,200);SpeedHomeInput i={0};
    SpeedHome_UpdateModel(&m,&i);CHECK(!m.speed_valid&&same(m.odo,"------")&&same(m.clock,"--:--"));
    i.valid_mask=3;i.speed_kph=100;i.odometer_km=36475;i.clock_valid=1;i.hour=23;i.minute=59;i.now_ms=1;
    SpeedHome_UpdateModel(&m,&i);CHECK(m.target==5000&&m.arc_value==0&&same(m.clock,"23:59")&&same(m.odo,"36475"));
    i.now_ms=76;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==2500);
    i.now_ms=151;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==5000);
    i.speed_kph=200;i.now_ms=201;SpeedHome_UpdateModel(&m,&i);
    i.now_ms=276;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==7500);
    i.speed_kph=50;i.now_ms=277;SpeedHome_UpdateModel(&m,&i);CHECK(m.from>7500&&m.target==2500);
    i.now_ms=427;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==2500);
    i.valid_mask=0;i.now_ms=428;SpeedHome_UpdateModel(&m,&i);CHECK(!m.speed_valid&&m.arc_color==0x65717C);
    i.now_ms=10000;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==2500&&same(m.odo,"------"));
    i.valid_mask=3;i.speed_kph=UINT32_MAX;i.now_ms=10001;SpeedHome_UpdateModel(&m,&i);CHECK(m.target==10000);
    i.now_ms=10151;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==10000&&m.arc_color==0xFF4040);
    i.units=1;i.odometer_km=1000;SpeedHome_UpdateModel(&m,&i);CHECK(same(m.odo,"621")&&same(m.unit,"mile"));
    i.units=0;i.odometer_km=1000000;SpeedHome_UpdateModel(&m,&i);CHECK(same(m.odo,"######"));
    i.hour=24;SpeedHome_UpdateModel(&m,&i);CHECK(!m.clock_valid&&same(m.clock,"--:--"));
    SpeedHome_InitModel(&m,0xfffffff0,200);i.now_ms=0xfffffff0;i.speed_kph=200;SpeedHome_UpdateModel(&m,&i);
    i.now_ms=134;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==10000);
    /* Monotonic sweep covers every inputkm/h and both display-unit settings. */
    for(unsigned k=0;k<=400;k++){
        i.speed_kph=k;i.units=k&1;i.now_ms+=200;SpeedHome_UpdateModel(&m,&i);
        i.now_ms+=150;SpeedHome_UpdateModel(&m,&i);CHECK(m.arc_value==(k>=200?10000:k*50));
    }
    return 0;
}
unsigned get_speed_assertions(void){return assertions;}

/*100ms UART changes are consumed between30fps render samples. New packets
 * must not restart from old endpoints, overshoot, or stall for one degree. */
unsigned test_speed_packet_interpolation(void)
{
    SpeedHomeModel m;SpeedHome_InitModel(&m,0,200);
    SpeedHomeInput in={.valid_mask=1};uint32_t previous=0,subdegree_steps=0;
    for(uint32_t t=0;t<=20000;t+=10){
        in.now_ms=t;in.speed_kph=t/100+1;if(in.speed_kph>200)in.speed_kph=200;
        SpeedHome_UpdateModel(&m,&in);
        CHECK(m.arc_value>=previous&&m.arc_value<=m.target);
        if(m.arc_value>previous&&m.arc_value-previous<37)++subdegree_steps;
        previous=m.arc_value;
    }
    CHECK(subdegree_steps>1000);
    in.speed_kph=1;in.now_ms+=10;SpeedHome_UpdateModel(&m,&in);previous=m.arc_value;
    for(uint32_t t=0;t<150;t+=10){in.now_ms+=10;SpeedHome_UpdateModel(&m,&in);CHECK(m.arc_value<=previous&&m.arc_value>=50);previous=m.arc_value;}
    CHECK(m.arc_value==50);
    return 0;
}

/* Real endpoint scanout is independent of time or command submission. Old
 * telemetry remains valid to the domain, but cannot take the ring over. */
unsigned test_startup_sweep(void)
{
    SpeedHomeStartup s;SpeedHomeStartup_Init(&s);
    SpeedHomeStartup_Ignition(&s,1,1,1);
    CHECK(s.phase==SPEED_START_ARMED&&s.starts==1);
    SpeedHomeStartup_Ignition(&s,1,1,1);CHECK(s.starts==1);
    CHECK(SpeedHomeStartup_Step(&s,100,0,9,1,1,100)&&s.value==0&&s.phase==SPEED_START_ARMED);
    CHECK(SpeedHomeStartup_Step(&s,200,1,9,1,1,200)&&s.phase==SPEED_START_HOME_FRAME);
    CHECK(SpeedHomeStartup_Step(&s,210,1,9,1,1,210)&&s.phase==SPEED_START_HOME_FRAME);
    CHECK(SpeedHomeStartup_Step(&s,215,1,10,0,1,215)&&s.phase==SPEED_START_HOME_FRAME);
    CHECK(SpeedHomeStartup_Step(&s,220,1,10,1,1,220)&&s.phase==SPEED_START_UP&&s.up_ms==220);
    uint32_t previous=0;
    for(uint32_t dt=1;dt<=800;++dt){
        CHECK(SpeedHomeStartup_Step(&s,220+dt,1,10,1,1,220+dt));
        CHECK(s.value>=previous&&s.value<=10000);previous=s.value;
        if(dt==400)CHECK(s.value==5000);
    }
    CHECK(s.phase==SPEED_START_PEAK_FRAME&&s.value==10000);
    SpeedHomeStartup_Step(&s,1100,1,10,1,1,1100);CHECK(s.phase==SPEED_START_PEAK_FRAME);
    SpeedHomeStartup_Step(&s,1120,1,11,0,1,1120);CHECK(s.phase==SPEED_START_PEAK_FRAME);
    SpeedHomeStartup_Step(&s,1130,1,11,1,1,1130);CHECK(s.phase==SPEED_START_DOWN&&s.down_ms==1130);
    for(uint32_t dt=1;dt<=800;++dt){
        CHECK(SpeedHomeStartup_Step(&s,1130+dt,1,11,1,1,1130+dt));
        CHECK(s.value<=previous&&s.value<=10000);previous=s.value;
        if(dt==400)CHECK(s.value==5000);
    }
    CHECK(s.phase==SPEED_START_ZERO_FRAME&&s.value==0);
    SpeedHomeStartup_Step(&s,1950,1,11,1,1,1950);CHECK(s.phase==SPEED_START_ZERO_FRAME);
    SpeedHomeStartup_Step(&s,1960,1,12,0,1,1960);CHECK(s.phase==SPEED_START_ZERO_FRAME);
    SpeedHomeStartup_Step(&s,2000,1,12,1,1,2000);CHECK(s.phase==SPEED_START_PACKET&&s.cutoff_ms==2000&&s.completed==1);
    CHECK(SpeedHomeStartup_Step(&s,2001,1,13,1,1,1999));
    CHECK(SpeedHomeStartup_Step(&s,2002,1,14,1,1,2000));
    CHECK(SpeedHomeStartup_Step(&s,2003,1,15,1,0,2003));
    CHECK(SpeedHomeStartup_Step(&s,5000,1,16,1,1,2000));
    SpeedHomeModel m;SpeedHome_InitModel(&m,0,200);
    SpeedHomeInput in={.now_ms=5000,.valid_mask=3,.speed_kph=120,.odometer_km=36475};
    SpeedHome_UpdateModel(&m,&in);SpeedHome_OverrideArc(&m,0,in.now_ms);
    CHECK(m.speed_valid&&m.odo_valid&&same(m.odo,"36475")&&m.target==0&&m.arc_value==0);
    CHECK(!SpeedHomeStartup_Step(&s,5100,1,17,1,1,5050)&&s.phase==SPEED_START_LIVE&&s.accepted_telemetry_ms==5050);
    in.now_ms=5100;SpeedHome_UpdateModel(&m,&in);CHECK(m.arc_value==0&&m.target==6000);
    in.now_ms=5175;SpeedHome_UpdateModel(&m,&in);CHECK(m.arc_value==3000);
    in.now_ms=5250;SpeedHome_UpdateModel(&m,&in);CHECK(m.arc_value==6000);
    SpeedHomeStartup_Ignition(&s,2,0,0);SpeedHomeStartup_Ignition(&s,3,1,0);
    CHECK(s.phase==SPEED_START_LIVE&&s.starts==1&&!s.cancelled);
    SpeedHomeStartup_Ignition(&s,4,1,1);CHECK(s.phase==SPEED_START_ARMED&&s.starts==2);
    SpeedHomeStartup_Ignition(&s,5,0,0);CHECK(s.phase==SPEED_START_LIVE&&s.cancelled==1);
    SpeedHomeStartup_Ignition(&s,6,1,0);CHECK(s.phase==SPEED_START_LIVE&&s.starts==2);
    return 0;
}

unsigned test_startup_tick_wrap(void)
{
    SpeedHomeStartup s;SpeedHomeStartup_Init(&s);SpeedHomeStartup_Ignition(&s,1,1,1);
    SpeedHomeStartup_Step(&s,UINT32_MAX-100U,1,UINT32_MAX,0,0,0);
    SpeedHomeStartup_Step(&s,UINT32_MAX-90U,1,0,1,0,0);CHECK(s.phase==SPEED_START_UP);
    SpeedHomeStartup_Step(&s,709,1,1,0,0,0);CHECK(s.phase==SPEED_START_PEAK_FRAME&&s.value==10000);
    /* Finish the full animation just before tick wrap in a second run. */
    SpeedHomeStartup_Init(&s);SpeedHomeStartup_Ignition(&s,1,1,1);
    uint32_t t=UINT32_MAX-1700U;
    SpeedHomeStartup_Step(&s,t,1,0,0,0,0);
    SpeedHomeStartup_Step(&s,t+1,1,1,1,0,0);
    SpeedHomeStartup_Step(&s,t+801,1,1,1,0,0);
    SpeedHomeStartup_Step(&s,t+802,1,2,1,0,0);
    SpeedHomeStartup_Step(&s,t+1602,1,2,1,0,0);
    SpeedHomeStartup_Step(&s,UINT32_MAX-5U,1,3,1,0,0);
    CHECK(s.phase==SPEED_START_PACKET&&s.cutoff_ms==UINT32_MAX-5U);
    CHECK(SpeedHomeStartup_Step(&s,1,1,4,1,1,UINT32_MAX-6U));
    CHECK(!SpeedHomeStartup_Step(&s,2,1,4,1,1,1)&&s.accepted_telemetry_ms==1);
    return 0;
}

/* Every in-flight phase can be interrupted by IGN OFF. A warm return never
 * resumes an old animation, consumes an old completion, or rearms a sweep. */
unsigned test_startup_cancel(void)
{
    for(uint32_t phase=SPEED_START_ARMED;phase<=SPEED_START_PACKET;++phase){
        SpeedHomeStartup s;SpeedHomeStartup_Init(&s);SpeedHomeStartup_Ignition(&s,1,1,1);
        uint32_t now=0,frame=0;
        while(s.phase!=phase&&now<10000U){
            SpeedHomeStartup_Step(&s,now,1,frame,1,0,0);now+=33U;++frame;
        }
        CHECK(s.phase==phase);
        SpeedHomeStartup_Ignition(&s,2,0,0);CHECK(s.phase==SPEED_START_LIVE&&s.cancelled==1);
        SpeedHomeStartup_Ignition(&s,3,1,0);
        CHECK(!SpeedHomeStartup_Step(&s,now,1,frame,1,1,now)&&s.starts==1);
        SpeedHomeStartup_Ignition(&s,4,1,1);CHECK(s.phase==SPEED_START_ARMED&&s.value==0&&s.starts==2);
    }
    return 0;
}


/* A one-packet peak must never teleport its marker ahead of the displayed
 * arc. The domain peak stays200 even if its visual sweep never reaches200. */
unsigned test_visual_peak(void)
{
    SpeedHomeModel m;SpeedHome_InitModel(&m,0,200);
    SpeedHomeInput in={.valid_mask=1,.session_valid=1,.session_peak_kph=200,.speed_kph=200};
    SpeedHome_UpdateModel(&m,&in);SpeedHome_RecordDisplayed(&m);
    CHECK(m.peak_ratio==0&&m.peak_color==m.arc_color);
    for(unsigned t=30;t<=90;t+=30){
        in.now_ms=t;SpeedHome_UpdateModel(&m,&in);SpeedHome_RecordDisplayed(&m);
        CHECK(m.peak_ratio==m.arc_value&&m.peak_ratio<10000&&m.peak_color==m.arc_color);
    }
    uint32_t peak=m.peak_ratio,color=m.peak_color;
    in.speed_kph=0;SpeedHome_UpdateModel(&m,&in);
    for(unsigned t=120;t<=300;t+=30){
        in.now_ms=t;SpeedHome_UpdateModel(&m,&in);SpeedHome_RecordDisplayed(&m);
        CHECK(m.peak_ratio==peak&&m.peak_color==color);
    }
    CHECK(m.arc_value==0&&in.session_peak_kph==200);
    /* Synthetic startup, invalid UART and an uncomposed model update do not
     * create records. Confirmed session change resets; provisional OFF does not. */
    SpeedHome_OverrideArc(&m,10000,in.now_ms);SpeedHome_RecordDisplayed(&m);
    CHECK(m.peak_ratio==peak&&m.peak_color==color);
    in.valid_mask=0;in.now_ms+=30;SpeedHome_UpdateModel(&m,&in);SpeedHome_RecordDisplayed(&m);
    CHECK(m.peak_ratio==peak);
    in.session_generation++;in.session_valid=0;SpeedHome_UpdateModel(&m,&in);
    CHECK(m.peak_ratio==0);
    SpeedHome_OverrideArc(&m,0,in.now_ms);
    in.session_valid=1;in.valid_mask=1;in.speed_kph=170;SpeedHome_UpdateModel(&m,&in);
    in.now_ms+=150;SpeedHome_UpdateModel(&m,&in);CHECK(m.peak_ratio==0);
    SpeedHome_RecordDisplayed(&m);CHECK(m.peak_ratio==8500&&m.peak_color==0xFF4040);
    return 0;
}
