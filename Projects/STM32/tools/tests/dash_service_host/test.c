/* Actual protocol, parser and service C. Mock only time-sliced hardware and
 * cached sensor inputs. This proves no physical UART or RTOS preemption. */
#include <stddef.h>
#include <string.h>
#include "Dash_Protocol.c"
#include "Vehicle_Service.c"
#include "DashService.c"
volatile BSP_Dash_Diagnostics g_bsp_dash;
volatile uint32_t g_assertions,g_failure_line;
unsigned mock_locks;
static Ambient_Snapshot ambient;
static uint32_t submit_result,abort_mode,abort_calls,init_result,tx_count,tx_length,rx_length;
static uint8_t sent[6],incoming[256];
static const uint32_t calibration[10]={8103,2981,1097,403,148,55,20,7,3,0};
static const uint8_t fuel0[]={0xF5,0x21,9,0,0,0,0x50,0x7B,0x8E,0,0,0,0x78};
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *d,const void *s,size_t n){for(size_t i=0;i<n;i++)((char*)d)[i]=((const char*)s)[i];return d;}
void *memset(void *d,int v,size_t n){for(size_t i=0;i<n;i++)((char*)d)[i]=(char)v;return d;}
void *memmove(void *d,const void *s,size_t n){if(d<s)return memcpy(d,s,n);while(n){--n;((char*)d)[n]=((const char*)s)[n];}return d;}
void AmbientService_GetSnapshot(Ambient_Snapshot *out){*out=ambient;}
void BSP_Dash_ReadLightThresholds(uint32_t out[10]){memcpy(out,calibration,40);}
uint32_t BSP_Dash_Init(void){g_bsp_dash.ready=!init_result;return init_result;}
uint32_t BSP_Dash_EnableTx(uint32_t en){if(g_bsp_dash.tx_busy)return 1;g_bsp_dash.tx_enabled=en;return 0;}
void BSP_Dash_Process(void){}
uint32_t BSP_Dash_Read(uint8_t *out,uint32_t size){uint32_t n=rx_length<size?rx_length:size;memcpy(out,incoming,n);rx_length=0;g_bsp_dash.rx_bytes+=n;return n;}
uint32_t BSP_Dash_Send(const uint8_t *data,uint32_t length){
    if(submit_result)return submit_result;
    memcpy(sent,data,length);tx_length=length;++tx_count;g_bsp_dash.tx_busy=1;g_bsp_dash.tx_bytes+=length;return 0;
}
void BSP_Dash_RequestTxAbort(void){
    ++abort_calls;if(abort_mode==2U)return;
    ++g_bsp_dash.tx_failed;g_bsp_dash.tx_last_result=abort_mode?3U:2U;
    if(!abort_mode)g_bsp_dash.tx_busy=0;
}
static void TC(void){g_bsp_dash.tx_busy=0;++g_bsp_dash.tx_completed;}
static void Rx(uint32_t now){memcpy(incoming,fuel0,13);rx_length=13;DashService_Process(now);}
static void Fresh(void){
    memset(&status,0,sizeof(status));memset(&protocol,0,sizeof(protocol));
    memset((void*)&request,0,sizeof(request));memset((void*)&g_bsp_dash,0,sizeof(g_bsp_dash));
    memset((void*)&g_dash_mailbox,0,sizeof(g_dash_mailbox));memset(&ambient,0,sizeof(ambient));
    next_id=active_id=active_command=request_started=in_flight=flight_started=flight_completed=flight_failed=abort_requested=0;
    cached_light=cached_valid=last_sample=mailbox_seen=mailbox_operation=submit_waiting=submit_started=0;
    submit_result=abort_mode=abort_calls=init_result=tx_count=tx_length=rx_length=0;light_override=DASH_LIGHT_AUTO;requested_bias=0;
}
static int TestProtocol(void){
    Dash_Protocol p;uint8_t f[6];uint32_t n;
    CHECK(!DashProtocol_Init(&p,NULL,0));
    uint32_t bad[10];memcpy(bad,calibration,40);bad[9]=1;CHECK(!DashProtocol_Init(&p,bad,0));
    memcpy(bad,calibration,40);bad[3]=bad[2];CHECK(!DashProtocol_Init(&p,bad,0));
    CHECK(DashProtocol_Init(&p,calibration,UINT32_MAX-100U));
    DashProtocol_OnFrames(&p,8,0);CHECK(!p.pending_light);
    DashProtocol_Process(&p,2899);CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_NONE);
    DashProtocol_Process(&p,2900);CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_REQUEST);
    CHECK(n==5 && f[0]==0xf5 && f[3]==4 && f[4]==0xf1);
    CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_REQUEST);
    DashProtocol_Accepted(&p,DASH_TX_REQUEST,2900);
    DashProtocol_OnFrames(&p,3,3000);CHECK(!p.pending_light && p.seen);
    DashProtocol_OnFrames(&p,1,3100);CHECK(p.pending_light==1);
    CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_LIGHT && n==6 && f[4]==0 && f[5]==0x57);
    DashProtocol_Accepted(&p,DASH_TX_LIGHT,3100);CHECK(!p.pending_light);
    DashProtocol_Process(&p,3500);CHECK(p.phase==DASH_PHASE_ACTIVE);
    DashProtocol_Process(&p,3501);CHECK(p.phase==DASH_PHASE_GAP && !p.seen);
    DashProtocol_OnFrames(&p,4,3502);CHECK(!p.pending_light);
    DashProtocol_Process(&p,4301);CHECK(!p.pending_request);
    DashProtocol_Process(&p,4302);CHECK(p.pending_request);
    DashProtocol_Accepted(&p,DASH_TX_REQUEST,4302);DashProtocol_OnFrames(&p,100,4303);
    CHECK(p.pending_light==8 && p.overflowed_replies==17);
    DashProtocol_Stop(&p);CHECK(!p.pending_light && p.phase==DASH_PHASE_OFF);
    CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_STOP && f[3]==0 && f[4]==0xf5);
    DashProtocol_Accepted(&p,DASH_TX_STOP,4304);CHECK(DashProtocol_Peek(&p,f,&n)==DASH_TX_NONE);
    for(uint32_t i=0;i<10;i++){
        CHECK(DashProtocol_LightIndex(&p,calibration[i]*1000U)==i);
        if(i<9)CHECK(DashProtocol_LightIndex(&p,calibration[i]*1000U-1U)==i+1U);
    }
    CHECK(DashProtocol_LightIndex(&p,UINT32_MAX)==0);return 0;
}
static int TestService(void){
    uint32_t id,other;Dash_Snapshot d;VehicleSnapshot v;
    Fresh();CHECK(DashService_RequestReconnect(&id)==DASH_NOT_READY);
    CHECK(DashService_Init(0)==0);DashService_Process(3000);CHECK(!tx_count);
    DashService_Process(3001);CHECK(tx_count==1 && !status.tx_frames);
    TC();DashService_Process(3002);CHECK(status.tx_frames==1 && status.tx_requests==1);
    Rx(3100);Rx(3200);Rx(3300);CHECK(tx_count==1);
    Rx(3400);CHECK(tx_count==2 && sent[1]==0xa1 && sent[4]==0);
    CHECK(status.light_source==DASH_LIGHT_UNAVAILABLE && status.link_up);
    TC();DashService_Process(3401);CHECK(status.tx_light==1);
    CHECK(DashService_GetVehicle(&v) && v.odometer_km==36475 && !v.stale);
    DashService_Process(3801);CHECK(!status.link_up && status.phase==DASH_PHASE_GAP);
    DashService_Process(4602);CHECK(tx_count==3 && sent[3]==4);TC();DashService_Process(4603);
    /* Task-facing ID/Busy, then actual TC, then line tri-state. */
    CHECK(DashService_RequestEnabled(0,&id)==DASH_ACCEPTED);
    CHECK(DashService_RequestReconnect(&other)==DASH_BUSY);
    DashService_Process(4610);CHECK(status.pending_id==id && status.completed_id!=id && sent[3]==0);
    TC();DashService_Process(4611);CHECK(status.completed_id==id && !status.enabled && !g_bsp_dash.tx_enabled);
    Rx(4700);Rx(4800);Rx(4900);Rx(5000);CHECK(tx_count==4);
    CHECK(DashService_RequestEnabled(1,&id)==0);DashService_Process(5010);TC();DashService_Process(5011);
    CHECK(status.completed_id==id && status.completed_result==0 && status.enabled);
    /* Live threshold, explicit stale cache, diagnostic override and AUTO. */
    ambient.driver.valid=1;ambient.driver.millilux=55000;ambient.driver.sample_ms=5012;
    DashService_Process(5012);CHECK(status.light_index==5 && status.light_source==DASH_LIGHT_LIVE);
    ambient.stale=1;DashService_Process(5013);CHECK(status.light_index==5 && status.light_source==DASH_LIGHT_CACHED);
    CHECK(DashService_RequestLight(9,&id)==0);DashService_Process(5014);
    CHECK(status.light_index==9 && status.light_source==DASH_LIGHT_OVERRIDE && status.completed_id==id);
    CHECK(DashService_RequestLight(DASH_LIGHT_AUTO,&id)==0);DashService_Process(5015);
    CHECK(status.light_index==5 && status.light_source==DASH_LIGHT_CACHED);
    CHECK(status.raw_light_index==5&&status.light_bias==0);
    for(int32_t bias=-5;bias<=5;bias++){
        DashService_SetLightBias(bias);DashService_Process(5015);
        CHECK(status.raw_light_index==5&&status.light_index==(uint32_t)(bias==5?9:5+bias)&&status.light_bias==bias);
        uint8_t frame[6];uint32_t bytes;protocol.pending_light=1;
        CHECK(DashProtocol_Peek(&protocol,frame,&bytes)==DASH_TX_LIGHT&&bytes==6&&frame[4]==status.light_index);
    }
    DashService_SetLightBias(6);DashService_Process(5015);CHECK(status.light_bias==5);
    DashService_SetLightBias(0);DashService_Process(5015);CHECK(status.light_index==5);
    protocol.pending_light=0;TC();DashService_Process(5015);
    CHECK(DashService_RequestLight(10,&id)==DASH_ARGUMENT && DashService_RequestReconnect(NULL)==DASH_ARGUMENT);
    g_dash_mailbox.command=DASH_COMMAND_LIGHT;g_dash_mailbox.argument=2;g_dash_mailbox.request_seq=1;
    DashService_Process(5016);CHECK(g_dash_mailbox.response_seq==1 && !g_dash_mailbox.result && status.light_index==2);
    DashService_GetSnapshot(&d);CHECK(d.process_count==status.process_count && mock_locks==0);
    /* A DMA timeout abort is failure, never a sent frame or fake completion. */
    for(uint32_t mode=0;mode<3;mode++){
        Fresh();CHECK(DashService_Init(0)==0);abort_mode=mode;
        CHECK(DashService_RequestReconnect(&id)==0);DashService_Process(1);
        DashService_Process(52);CHECK(abort_calls==1 && !status.tx_frames);
        DashService_Process(103);CHECK(status.completed_id==id && status.completed_result!=0);
        CHECK(!status.link_up && !status.tx_frames && status.error);
        CHECK(DashService_RequestReconnect(&other)==0);DashService_Process(104);
        if(mode==2)CHECK(status.completed_id==other && status.completed_result==106);
    }
    Fresh();CHECK(DashService_Init(0)==0);submit_result=2;
    DashService_Process(3001);DashService_Process(3052);CHECK(status.error==108 && !tx_count);
    Fresh();CHECK(DashService_Init(0)==0);submit_result=3;
    DashService_Process(3001);CHECK(status.error==113 && !status.tx_frames);
    return 0;
}
int test_main(void){if(TestProtocol())return 1;if(TestService())return 1;return 0;}
