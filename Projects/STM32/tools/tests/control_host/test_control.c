#include <stdint.h>
#include <stddef.h>
#include "NoodoeControl.h"
#include "NoodoeBluetooth.h"
#include "NoodoeRuntime.h"
#include "MediaControl.h"
#include "stm32f4xx_hal.h"
/* Security/self-test are outside this transport fixture; connected mock links
 * are authenticated and the optional diagnostic opcode is unsupported. */
int Bluetooth_LinkSecure(Bluetooth_Role role,const Bluetooth_LinkState *expected){(void)role;return expected&&expected->status==BLUETOOTH_LINK_UP;}
uint32_t RadioSelfTest_Handle(const uint8_t *p,uint32_t n,uint32_t epoch,uint32_t now,uint8_t *reply,uint32_t *bytes){(void)p;(void)n;(void)epoch;(void)now;(void)reply;*bytes=0;return 1;}
volatile uint32_t g_assertions,g_failure_line,g_mock_error;
static Bluetooth_LinkState link,link2;
static uint8_t input2[4096],sent2[4096];
static uint32_t input2_used,input2_read,sent2_used;
static UpdateService update;
static uint8_t input[4096],sent[4096];
static uint32_t input_used,input_read,sent_used,now,seq;
static uint32_t update_reply,update_notified,update_handled,update_result;
static uint32_t discover_calls,connect_calls,disconnect_calls,phone_calls;
static uint32_t max_send,send_count;
static uint32_t rx_calls,reconnect_on_rx,reconnect_on_tx;
static uint8_t replacement[128];
static uint32_t replacement_used;
static int32_t observed_result;
static uint32_t observed_sequence,observed_opcode,observed_length,observed_frames;
static uint32_t capabilities[3];
static uint8_t observed_payload[NDCP_PAYLOAD_MAX];
static NDCP_Parser response_parser;
#define CHECK(x) do{++g_assertions;if(!(x)){g_failure_line=__LINE__;return 1;}}while(0)
void *memcpy(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;for(size_t i=0;i<n;++i)a[i]=b[i];return d;}
void *memset(void *d,int x,size_t n){uint8_t *a=d;for(size_t i=0;i<n;++i)a[i]=(uint8_t)x;return d;}
void *memmove(void *d,const void *s,size_t n){uint8_t *a=d;const uint8_t *b=s;if(a<b){for(size_t i=0;i<n;++i)a[i]=b[i];}else{while(n){--n;a[n]=b[n];}}return d;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;++i)if(x[i]!=y[i])return x[i]-y[i];return 0;}
static uint32_t Get(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int Bluetooth_GetLinkState(Bluetooth_Role role,Bluetooth_LinkState *out){if(role==BLUETOOTH_PHONE2){*out=link2;return BLUETOOTH_OK;}if(role!=BLUETOOTH_PHONE)g_mock_error=1;*out=link;return BLUETOOTH_OK;}
/* Unbound byte APIs deliberately fail: production control must never regress
 * to a separate status check followed by unchecked receive/send. */
int Bluetooth_Receive(Bluetooth_Role role,void *data,size_t cap){(void)role;(void)data;(void)cap;g_mock_error=20U;return BLUETOOTH_INVALID;}
int Bluetooth_Send(Bluetooth_Role role,const void *data,size_t length){(void)role;(void)data;(void)length;g_mock_error=21U;return BLUETOOTH_INVALID;}
static uint32_t SameSession(const Bluetooth_LinkState *expected)
{return expected && link.status==BLUETOOTH_LINK_UP && expected->status==BLUETOOTH_LINK_UP && link.cid==expected->cid && link.reconnects==expected->reconnects && !memcmp(link.address,expected->address,6U);}
int Bluetooth_ReceiveSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,void *data,size_t cap)
{
    if(role==BLUETOOTH_PHONE2){
        if(!expected||expected->cid!=link2.cid||expected->reconnects!=link2.reconnects||link2.status!=BLUETOOTH_LINK_UP)return BLUETOOTH_NOT_READY;
        if(cap!=1U)g_mock_error=22;
        if(input2_read==input2_used)return 0;
        *(uint8_t*)data=input2[input2_read++];return 1;
    }
    if(role!=BLUETOOTH_PHONE || cap!=1U)g_mock_error=2;
    if(++rx_calls==reconnect_on_rx){++link.reconnects;input_read=0U;input_used=replacement_used;memcpy(input,replacement,replacement_used);}
    if(!SameSession(expected))return BLUETOOTH_NOT_READY;
    if(input_read==input_used)return 0;
    *(uint8_t*)data=input[input_read++];return 1;
}
int Bluetooth_SendSession(Bluetooth_Role role,const Bluetooth_LinkState *expected,const void *data,size_t length)
{
    if(role==BLUETOOTH_PHONE2){
        if(!expected||expected->cid!=link2.cid||expected->reconnects!=link2.reconnects||link2.status!=BLUETOOTH_LINK_UP)return BLUETOOTH_NOT_READY;
        if(link2.tx_queued+length>BLUETOOTH_TX_CAPACITY)return BLUETOOTH_QUEUE_FULL;
        if(sent2_used+length>sizeof(sent2))return BLUETOOTH_QUEUE_FULL;
        memcpy(sent2+sent2_used,data,length);sent2_used+=length;link2.tx_queued+=length;return BLUETOOTH_OK;
    }
    if(reconnect_on_tx){reconnect_on_tx=0U;++link.reconnects;link.tx_queued=0U;}
    if(!SameSession(expected))return BLUETOOTH_NOT_READY;
    if(link.tx_queued+length>BLUETOOTH_TX_CAPACITY)return BLUETOOTH_QUEUE_FULL;
    if(role!=BLUETOOTH_PHONE || length>512U || sent_used+length>sizeof(sent)){g_mock_error=3;return BLUETOOTH_INVALID;}
    memcpy(sent+sent_used,data,length);sent_used+=(uint32_t)length;link.tx_queued+=(uint32_t)length;++send_count;if(length>max_send)max_send=(uint32_t)length;return BLUETOOTH_OK;
}
void Bluetooth_GetDiagnostics(Bluetooth_Diagnostics *out){memset(out,0,sizeof(*out));
    for(uint32_t i=0;i<17;i++){uint32_t value=100+i;memcpy((uint8_t*)out+4*i,&value,4);}}
int Bluetooth_Discover(void){++discover_calls;return BLUETOOTH_OK;}
size_t Bluetooth_GetDiscovered(Bluetooth_DiscoveredDevice *out,size_t cap){(void)out;(void)cap;return 0;}
int Bluetooth_Connect(Bluetooth_Role role,const uint8_t address[6],uint8_t channel){(void)address;(void)channel;if(role!=BLUETOOTH_ELM)g_mock_error=4;++connect_calls;return BLUETOOTH_OK;}
int Bluetooth_Disconnect(Bluetooth_Role role){++disconnect_calls;if(role==BLUETOOTH_PHONE)link.status=BLUETOOTH_LINK_OFF;else if(role==BLUETOOTH_PHONE2)link2.status=BLUETOOTH_LINK_OFF;return BLUETOOTH_OK;}
int Bluetooth_Pair(const uint8_t address[6],const char *pin){(void)address;(void)pin;return BLUETOOTH_OK;}
int Bluetooth_SetPairingWindow(uint32_t seconds){return seconds<=120U?BLUETOOTH_OK:BLUETOOTH_INVALID;}
uint32_t NoodoeRuntime_GetVehicle(VehicleSnapshot *out){memset(out,0,sizeof(*out));
    for(uint32_t i=0;i<18;i++){uint32_t value=200+i;memcpy((uint8_t*)out+4*i,&value,4);}out->temperature_candidate_c=-30;return 1;}
uint32_t NoodoeRuntime_GetGnss(GnssSnapshot *out){memset(out,0,sizeof(*out));
    out->source=301;out->external_connected=302;out->valid=303;out->stale=304;out->age_ms=305;
    out->fix.fields=306;out->fix.sample_ms=307;out->fix.has_sample=308;
    for(uint32_t i=0;i<8;i++)out->fix.field_ms[i]=400+i;
    out->fix.latitude_e7=-1;out->fix.longitude_e7=-2;out->fix.speed_mm_s=503;out->fix.course_mdeg=504;
    out->fix.utc_ms=505;out->fix.date_yyyymmdd=506;out->fix.altitude_mm=-7;
    out->fix.satellites=508;out->fix.hdop_milli=509;out->fix.quality=510;return 1;}
void UpdateService_SetConnected(UpdateService *s,uint32_t connected){if(s->connected!=connected)++s->link_generation;s->connected=connected;if(!connected)s->authorization=0;}
void UpdateService_Authorize(UpdateService *s,uint32_t token){s->authorization=token==UPDATE_STAGE_ARM && s->connected;}
uint32_t UpdateService_Handle(UpdateService *s,const NDCP_Frame *request){(void)s;(void)request;++update_handled;return update_result;}
size_t UpdateService_TakeReply(UpdateService *s,uint8_t *out,size_t cap){(void)s;if(!update_reply)return 0;update_reply=0;uint8_t payload[1024];memset(payload,0,sizeof(payload));return NDCP_Encode(out,cap,UPDATE_OP_RESET,NDCP_FLAG_RESPONSE,0x777U,payload,sizeof(payload));}
void UpdateService_NotifyReplyTransmitted(UpdateService *s,uint32_t sequence,uint32_t when){(void)s;(void)when;if(sequence!=0x777U)g_mock_error=5;++update_notified;}
static uint32_t Phone(void *ctx,const GnssFix *fix,uint32_t when){(void)ctx;(void)when;if(fix->latitude_e7!=123456789 || fix->longitude_e7!=-987654321)g_mock_error=6;++phone_calls;return 1;}
static void Observe(void *context,const NDCP_Frame *frame){(void)context;++observed_frames;observed_result=(int32_t)Get(frame->payload);observed_opcode=frame->opcode;observed_sequence=frame->sequence;observed_length=frame->length;
    memcpy(observed_payload,frame->payload,frame->length);
    if(frame->opcode==0x0DU&&frame->length==16U)for(unsigned i=0;i<3;++i)capabilities[i]=Get(frame->payload+4U+4U*i);
}
static void Pump(void){for(uint32_t i=0;i<40;++i){NoodoeControl_Process(now+=2U);link.tx_queued=0;}NDCP_Feed(&response_parser,sent,sent_used,now);sent_used=0;}
static void Request(uint32_t opcode,const void *payload,uint32_t length){input_used=(uint32_t)NDCP_Encode(input,sizeof(input),opcode,0,++seq,payload,length);input_read=0;Pump();}
int Control_TestMain(void)
{
    link.status=BLUETOOTH_LINK_UP;link.cid=10;link.reconnects=1;
    NoodoeControl_Init(&update);NoodoeControl_SetPhoneGPSCallback(Phone,NULL);NDCP_Init(&response_parser,Observe,NULL);
    Request(0,NULL,0);CHECK(observed_result==0 && observed_opcode==0 && observed_sequence==seq);CHECK(g_noodoe_control.connected==1);
    Request(1,NULL,0);CHECK(observed_result==0 && observed_length==72);
    Request(2,NULL,0);CHECK(observed_result==0 && observed_length==212);
    for(uint32_t i=0;i<17;i++)CHECK(Get(observed_payload+4+4*i)==100+i);
    Request(0x0A,NULL,0);CHECK(observed_result==0 && observed_length==80);
    for(uint32_t i=0;i<18;i++)CHECK(Get(observed_payload+4+4*i)==(i==12?(uint32_t)-30:200+i));
    Request(0x0B,NULL,0);CHECK(observed_result==0 && observed_length==112);
    for(uint32_t i=0;i<8;i++)CHECK(Get(observed_payload+4+4*i)==301+i);
    for(uint32_t i=0;i<8;i++)CHECK(Get(observed_payload+36+4*i)==400+i);
    const uint32_t expected_fix[]={-1,-2,503,504,505,506,-7,508,509,510,0};
    for(uint32_t i=0;i<11;i++)CHECK(Get(observed_payload+68+4*i)==expected_fix[i]);
    Request(0x0C,NULL,0);CHECK(observed_result==NOODOE_CONTROL_UNSUPPORTED && observed_length==4);
    Request(3,NULL,0);CHECK(!discover_calls&&observed_result==NOODOE_CONTROL_UNSUPPORTED);
    uint8_t connect[8]={0,0,1,2,3,4,5,6};Request(5,connect,8);CHECK(observed_result==NOODOE_CONTROL_UNSUPPORTED && !connect_calls);
    connect[0]=1;Request(5,connect,8);CHECK(observed_result==NOODOE_CONTROL_UNSUPPORTED && !connect_calls);
    uint32_t fix[11]={1U,123456789U,(uint32_t)-987654321,0,0,0,0,0,0,0,1};Request(9,fix,sizeof(fix));CHECK(observed_result==0 && phone_calls==1);
    uint32_t authorization[4]={0,0,0,0x42414B32U};Request(0x1F,authorization,16);CHECK(observed_result==NOODOE_CONTROL_DENIED && !update.authorization);
    authorization[0]=HAL_GetUIDw0();authorization[1]=HAL_GetUIDw1();authorization[2]=HAL_GetUIDw2();Request(0x1F,authorization,16);CHECK(observed_result==0 && update.authorization);
    ++link.reconnects;NoodoeControl_Process(now+=2);CHECK(!update.authorization);CHECK(g_noodoe_control.link_generation>=3U);
    update_result=UPDATE_BUSY;Request(UPDATE_OP_STATUS,NULL,0);CHECK(update_handled==1 && observed_result==UPDATE_BUSY && observed_length==20);
    /* A maximum NDCP frame needs512+512+20bytes. It must remain stable until
     * the final local queue drain, and no ACK is published on mere enqueue. */
    update_reply=1;send_count=max_send=0;NoodoeControl_Process(now+=2);CHECK(send_count==1 && !update_notified);
    NoodoeControl_Process(now+=2);CHECK(send_count==2 && !update_notified);
    NoodoeControl_Process(now+=2);CHECK(send_count==2 && !update_notified && max_send==512);
    link.tx_queued=0;NoodoeControl_Process(now+=2);CHECK(send_count==3 && !update_notified);
    link.tx_queued=0;NoodoeControl_Process(now+=2);CHECK(update_notified==1);sent_used=0;
    /* Same CID with a new incoming-open epoch invalidates old ACK ownership. */
    update_reply=1;NoodoeControl_Process(now+=2);NoodoeControl_Process(now+=2);NoodoeControl_Process(now+=2);
    ++link.reconnects;link.tx_queued=0;NoodoeControl_Process(now+=2);CHECK(update_notified==1);sent_used=0;
    /* Reconnect inside dequeue, after the initial poll, replaces a partial old
     * frame with a complete new-peer request. It must consume no new byte on
     * mismatch, reset the parser/authorization, then accept the new frame. */
    Request(0x1F,authorization,16);CHECK(update.authorization);
    uint32_t old_discovers=discover_calls;
    input_used=(uint32_t)NDCP_Encode(input,sizeof(input),9,0,++seq,(const uint8_t*)fix,sizeof(fix));input_read=0;
    replacement_used=(uint32_t)NDCP_Encode(replacement,sizeof(replacement),3,0,++seq,NULL,0);
    reconnect_on_rx=rx_calls+10U;NoodoeControl_Process(now+=2);
    CHECK(input_read==0U && !g_noodoe_control.connected && !update.authorization);
    CHECK(discover_calls==old_discovers && phone_calls==1U);
    Pump();CHECK(discover_calls==old_discovers && observed_opcode==3U && observed_result==NOODOE_CONTROL_UNSUPPORTED);
    /* Reconnect during second enqueue cannot append an old response's tail to
     * the new peer or falsely notify updater completion. Old prefix bytes were
     * already sent on the old peer and are merely discarded by this fixture. */
    update_reply=1U;send_count=0U;NoodoeControl_Process(now+=2);CHECK(send_count==1U && sent_used==512U);
    reconnect_on_tx=1U;NoodoeControl_Process(now+=2);
    CHECK(send_count==1U && sent_used==512U && !g_noodoe_control.connected && update_notified==1U);
    sent_used=0U;Pump();CHECK(send_count==1U && update_notified==1U);
    uint32_t before=discover_calls;input_used=(uint32_t)NDCP_Encode(input,sizeof(input),3,0,++seq,NULL,0);input[input_used-1]^=1;input_read=0;Pump();CHECK(discover_calls==before && g_noodoe_control.parser_crc_errors==1);
    /* Reserved second-phone data must remain unread. The sole session keeps
     * authorization, framing, reply order and media backpressure. */
    Request(0x1F,authorization,16);CHECK(update.authorization);
    link2.status=BLUETOOTH_LINK_UP;link2.cid=20;link2.reconnects=1;
    input2_used=NDCP_Encode(input2,sizeof(input2),0,0,0x222,NULL,0);input2_read=0;
    Request(0,NULL,0);CHECK(observed_sequence==seq&&update.authorization);
    CHECK(!sent2_used&&!input2_read);
    NoodoeControl_Diagnostics secondary;CHECK(!NoodoeControl_GetPhoneDiagnostics(1,&secondary));
    Request(0x0D,NULL,0);CHECK(observed_result==0&&observed_length==16);
    CHECK(capabilities[0]==1&&capabilities[1]==1&&capabilities[2]==7);
    uint8_t bad_cap=0;Request(0x0D,&bad_cap,1);CHECK(observed_result==NOODOE_CONTROL_ARGUMENT);
    Request(4,NULL,0);CHECK(observed_result==NOODOE_CONTROL_UNSUPPORTED);
    Request(7,NULL,0);CHECK(observed_result==NOODOE_CONTROL_UNSUPPORTED);
    CHECK(MEDIA_CONTROL_OPCODE==0x10U&&MEDIA_CONTROL_OPCODE<UPDATE_OP_BEGIN);
    CHECK(MediaControl_Request(1,0,now)==1&&MediaControl_Request(2,0,now)==1);
    CHECK(MediaControl_Request(0,3,now)==1);
    sent_used=0;link.tx_queued=0;
    CHECK(MediaControl_Request(0,0,now)==0);
    uint32_t id0=g_media_control.last_sequence;
    CHECK(MediaControl_Request(0,1,now)==3);
    NoodoeControl_Process(now+=2);CHECK(sent_used==24&&!sent2_used);
    CHECK(sent[5]==MEDIA_CONTROL_OPCODE&&!sent[6]&&Get(sent+8)==id0&&Get(sent+NDCP_HEADER_SIZE)==0);
    uint32_t result=0;sent_used=0;link.tx_queued=0;
    input_used=NDCP_Encode(input,sizeof(input),MEDIA_CONTROL_OPCODE,NDCP_FLAG_RESPONSE,id0+1,(uint8_t*)&result,4);input_read=0;
    Pump();CHECK(!g_media_control.acknowledged&&MediaControl_IsPending(0,id0));
    input_used=NDCP_Encode(input,sizeof(input),MEDIA_CONTROL_OPCODE,NDCP_FLAG_RESPONSE,id0,(uint8_t*)&result,4);input_read=0;
    Pump();CHECK(g_media_control.acknowledged==1&&!MediaControl_IsPending(0,id0));
    CHECK(MediaControl_Request(0,1,now)==0);++link.reconnects;sent_used=0;
    NoodoeControl_Process(now+=2);CHECK(g_media_control.failed==1&&!sent_used);
    CHECK(MediaControl_Request(0,2,now)==0);link.tx_queued=BLUETOOTH_TX_CAPACITY;
    NoodoeControl_Process(now+=2);CHECK(!sent_used);
    NoodoeControl_Process(now+=3001);link.tx_queued=0;NoodoeControl_Process(now+=2);
    CHECK(g_media_control.failed==2&&!sent_used);
    CHECK(MediaControl_Request(0,0,now)==0);NoodoeControl_Process(now+=2);CHECK(sent_used==24);
    uint32_t lost_ack_id=g_media_control.last_sequence;
    sent_used=0;link.tx_queued=0;NoodoeControl_Process(now+=3001);
    CHECK(g_media_control.failed==3&&!sent_used);
    MediaControl_Acknowledge(0,lost_ack_id,0);CHECK(g_media_control.acknowledged==1);
    uint32_t reply_words[5]={42,100,200,300,4};sent_used=0;link.tx_queued=0;
    CHECK(MediaControl_Reply(reply_words,now)==0);uint32_t reply_id=g_media_control.last_sequence;
    NoodoeControl_Process(now+=2);CHECK(sent_used==40&&sent[5]==PHONE_REPLY_OPCODE&&Get(sent+8)==reply_id);
    for(uint32_t i=0;i<5;i++)CHECK(Get(sent+NDCP_HEADER_SIZE+4*i)==reply_words[i]);
    CHECK(MediaControl_Reply(reply_words,now)==3);
    input_used=NDCP_Encode(input,sizeof(input),PHONE_REPLY_OPCODE,NDCP_FLAG_RESPONSE,reply_id,(uint8_t*)&result,4);input_read=0;
    Pump();CHECK(g_media_control.acknowledged==2&&!MediaControl_IsPending(0,reply_id));
    sent_used=0;link.tx_queued=BLUETOOTH_TX_CAPACITY;CHECK(MediaControl_Reply(reply_words,now)==0);
    NoodoeControl_Process(now+=2);NoodoeControl_Process(now+=3001);link.tx_queued=0;NoodoeControl_Process(now+=2);CHECK(!sent_used);
    uint8_t role=0;Request(6,&role,1);CHECK(disconnect_calls==1 && !g_noodoe_control.connected);
    CHECK(g_mock_error==0);return 0;
}
