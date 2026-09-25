#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "NoodoeControl.h"
#include "RoutineUpdate.h"
#include "NoodoeBluetooth.h"
#include "NoodoeRuntime.h"
#include "MediaControl.h"
#include "RadioSelfTest.h"
#if NOODOE_PRODUCT
#include "App_Recovery.h"
#include "Uninstall_Expected.h"
#include "CompanionControl.h"
#include "BootStore.h"
#include "DeviceLog.h"
#include "ResourceStore.h"
#endif
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stddef.h>

//// BOP IT !!! TWIST IT!!!!!!!!

#define CONTROL_QUEUE_DEPTH 2U
typedef struct {uint32_t length;uint8_t bytes[NDCP_FRAME_MAX];} ControlReply;
volatile NoodoeControl_Diagnostics g_noodoe_control;
static UpdateService *update;
static NoodoeControl_PhoneGPSCallback phone_callback;
static void *phone_context;
static uint32_t now_ms;
typedef struct {
    NDCP_Parser parser;
    ControlReply replies[CONTROL_QUEUE_DEPTH];
    uint32_t reply_write,reply_read;
    uint8_t tx[NDCP_FRAME_MAX];
    uint32_t tx_length,tx_offset,tx_sequence,tx_is_update,tx_generation;
    uint32_t link_cid,link_epoch,deferred_disconnect,deferred_sequence;
    uint8_t link_address[6];
    Bluetooth_Role role;
    NoodoeControl_Diagnostics diagnostics;
} ControlSession;
static ControlSession sessions[BLUETOOTH_PHONE_CAPACITY];
static uint8_t payload[NDCP_PAYLOAD_MAX]; /* Callback scratch only; never queued by reference. */
/* Static workspace keeps the I/O task stack bounded. Snapshot getters copy
 * atomically in Runtime; no pointer to parser/workspace escapes a callback. */
static union {Bluetooth_Diagnostics bt;VehicleSnapshot vehicle;GnssSnapshot gnss;GnssFix phone;} snapshot;

static uint32_t U32(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
static void Put(uint8_t *p,uint32_t value){memcpy(p,&value,4);}
static uint32_t Words(uint32_t offset,const uint32_t *words,uint32_t count){memcpy(payload+offset,words,count*4U);return offset+count*4U;}
/* These are validated scalar slices, not a struct dump. Every wire field's
 * width/order/stride is asserted, and only the STM32 little-endian build is
 * accepted. Native layout changes fail compilation rather than changing NDCP.
 * Byte-addressed memcpy avoids array-pointer arithmetic across struct members. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "NDCP scalar slice requires little-endian target"
#endif
#define WIRE_FIELD(t,start,f,i) _Static_assert(sizeof(((t*)0)->f)==4 && offsetof(t,f)==offsetof(t,start)+4*(i),"NDCP field layout")
WIRE_FIELD(Bluetooth_Diagnostics,magic,magic,0);
WIRE_FIELD(Bluetooth_Diagnostics,magic,version,1);
WIRE_FIELD(Bluetooth_Diagnostics,magic,state,2);
WIRE_FIELD(Bluetooth_Diagnostics,magic,last_error,3);
WIRE_FIELD(Bluetooth_Diagnostics,magic,heartbeat,4);
WIRE_FIELD(Bluetooth_Diagnostics,magic,manufacturer,5);
WIRE_FIELD(Bluetooth_Diagnostics,magic,lmp_subversion,6);
WIRE_FIELD(Bluetooth_Diagnostics,magic,hci_revision,7);
WIRE_FIELD(Bluetooth_Diagnostics,magic,patch_bytes,8);
WIRE_FIELD(Bluetooth_Diagnostics,magic,baud,9);
WIRE_FIELD(Bluetooth_Diagnostics,magic,commands,10);
WIRE_FIELD(Bluetooth_Diagnostics,magic,command_rejected,11);
WIRE_FIELD(Bluetooth_Diagnostics,magic,hci_errors,12);
WIRE_FIELD(Bluetooth_Diagnostics,magic,pairings,13);
WIRE_FIELD(Bluetooth_Diagnostics,magic,key_generation,14);
WIRE_FIELD(Bluetooth_Diagnostics,magic,key_persisted_generation,15);
WIRE_FIELD(Bluetooth_Diagnostics,magic,stack_low_words,16);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,rx_bytes,0);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,tx_bytes,1);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,rx_overflow,2);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,tx_rejected,3);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,reconnects,4);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,last_error,5);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,rx_queued,6);
WIRE_FIELD(Bluetooth_LinkState,rx_bytes,tx_queued,7);
WIRE_FIELD(VehicleSnapshot,sequence,sequence,0);
WIRE_FIELD(VehicleSnapshot,sequence,valid_fields,1);
WIRE_FIELD(VehicleSnapshot,sequence,stale,2);
WIRE_FIELD(VehicleSnapshot,sequence,telemetry_ms,3);
WIRE_FIELD(VehicleSnapshot,sequence,age_ms,4);
WIRE_FIELD(VehicleSnapshot,sequence,speed_kph,5);
WIRE_FIELD(VehicleSnapshot,sequence,odometer_km,6);
WIRE_FIELD(VehicleSnapshot,sequence,fuel_observed,7);
WIRE_FIELD(VehicleSnapshot,sequence,payload2_raw,8);
WIRE_FIELD(VehicleSnapshot,sequence,status_raw,9);
WIRE_FIELD(VehicleSnapshot,sequence,status_high,10);
WIRE_FIELD(VehicleSnapshot,sequence,status_low,11);
WIRE_FIELD(VehicleSnapshot,sequence,temperature_candidate_c,12);
WIRE_FIELD(VehicleSnapshot,sequence,extended_raw,13);
WIRE_FIELD(VehicleSnapshot,sequence,extended_present,14);
WIRE_FIELD(VehicleSnapshot,sequence,frame_ms,15);
WIRE_FIELD(VehicleSnapshot,sequence,frame_command,16);
WIRE_FIELD(VehicleSnapshot,sequence,frame_length,17);
WIRE_FIELD(GnssSnapshot,source,source,0);
WIRE_FIELD(GnssSnapshot,source,external_connected,1);
WIRE_FIELD(GnssSnapshot,source,valid,2);
WIRE_FIELD(GnssSnapshot,source,stale,3);
WIRE_FIELD(GnssSnapshot,source,age_ms,4);
WIRE_FIELD(GnssFix,fields,fields,0);
WIRE_FIELD(GnssFix,fields,sample_ms,1);
WIRE_FIELD(GnssFix,latitude_e7,latitude_e7,0);
WIRE_FIELD(GnssFix,latitude_e7,longitude_e7,1);
WIRE_FIELD(GnssFix,latitude_e7,speed_mm_s,2);
WIRE_FIELD(GnssFix,latitude_e7,course_mdeg,3);
WIRE_FIELD(GnssFix,latitude_e7,utc_ms,4);
WIRE_FIELD(GnssFix,latitude_e7,date_yyyymmdd,5);
WIRE_FIELD(GnssFix,latitude_e7,altitude_mm,6);
WIRE_FIELD(GnssFix,latitude_e7,satellites,7);
WIRE_FIELD(GnssFix,latitude_e7,hdop_milli,8);
WIRE_FIELD(GnssFix,latitude_e7,quality,9);
#undef WIRE_FIELD
static uint32_t ScalarSlice(uint32_t offset,const void *object,uint32_t first,uint32_t count)
{memcpy(payload+offset,(const uint8_t*)object+first,count*4U);return offset+count*4U;}
/* Encoded frames are immutable in the bounded reply ring until copied to TX.
 * A full ring rejects the request before side effects; normal RX backpressure
 * prevents reaching this path, including when a peer pipelines commands. */
static void Reply(ControlSession *s,const NDCP_Frame *request,int32_t result,uint32_t extra)
{
    if(s->reply_write-s->reply_read>=CONTROL_QUEUE_DEPTH){++s->diagnostics.queue_full;return;}
    Put(payload,(uint32_t)result);ControlReply *reply=&s->replies[s->reply_write%CONTROL_QUEUE_DEPTH];
    reply->length=(uint32_t)NDCP_Encode(reply->bytes,sizeof(reply->bytes),request->opcode,
        NDCP_FLAG_RESPONSE|(result?NDCP_FLAG_ERROR:0U),request->sequence,payload,extra+4U);
    if(reply->length){++s->reply_write;s->diagnostics.queued_replies=s->reply_write-s->reply_read;}
    s->diagnostics.last_result=(uint32_t)result;if(result)++s->diagnostics.errors;
}
/* BT diagnostics are serialized explicitly; C padding and native struct ABI
 * do not become the phone protocol. Signed API results use LE two's complement. */
static uint32_t BluetoothInfo(void)
{
    Bluetooth_GetDiagnostics(&snapshot.bt);Bluetooth_Diagnostics *d=&snapshot.bt;
    uint32_t end=ScalarSlice(4U,d,offsetof(Bluetooth_Diagnostics,magic),17U);memcpy(payload+end,d->local_address,6U);payload[end+6U]=d->discovered_count;payload[end+7U]=0U;end+=8U;
    for(uint32_t i=0;i<3U;++i){Bluetooth_LinkState *s=&d->links[i];
        memcpy(payload+end,s->address,6U);payload[end+6U]=s->status;payload[end+7U]=s->server_channel;end+=8U;
        Put(payload+end,(uint32_t)s->cid|((uint32_t)s->mtu<<16));end+=4U;
        end=ScalarSlice(end,s,offsetof(Bluetooth_LinkState,rx_bytes),8U);
    }
    return end-4U;
}
static uint32_t VehicleInfo(void)
{
    VehicleSnapshot *s=&snapshot.vehicle;if(!NoodoeRuntime_GetVehicle(s))return 0U;
    uint32_t raw=s->raw_length>VEHICLE_FRAME_MAX?VEHICLE_FRAME_MAX:s->raw_length;
    uint32_t end=ScalarSlice(4U,s,offsetof(VehicleSnapshot,sequence),18U);Put(payload+end,raw);end+=4;memcpy(payload+end,s->raw,raw);return end+raw-4U;
}
static uint32_t GnssInfo(void)
{
    GnssSnapshot *s=&snapshot.gnss;if(!NoodoeRuntime_GetGnss(s))return 0U;
    GnssFix *f=&s->fix;uint32_t raw=f->raw_length>=GNSS_SENTENCE_MAX?GNSS_SENTENCE_MAX-1U:f->raw_length;
    uint32_t end=ScalarSlice(4U,s,offsetof(GnssSnapshot,source),5U);end=ScalarSlice(end,f,offsetof(GnssFix,fields),2U);Put(payload+end,f->has_sample);end+=4;end=Words(end,f->field_ms,8U);
    end=ScalarSlice(end,f,offsetof(GnssFix,latitude_e7),10U);Put(payload+end,raw);end+=4;memcpy(payload+end,f->raw,raw);return end+raw-4U;
}
/* Updater queue rejection uses its own standard20-byte result prefix, so a
 * host can parse UPDATE_BUSY exactly as it parses a StorageTask response. */
static void UpdateRejected(ControlSession *s,const NDCP_Frame *frame,uint32_t result)
{
    Put(payload+4U,update?update->state:UPDATE_IDLE);Put(payload+8U,update?update->transaction:0U);
    Put(payload+12U,update?update->received:0U);Put(payload+16U,update?update->verified:0U);
    Reply(s,frame,(int32_t)result,16U);
}
static void OnFrame(void *context,const NDCP_Frame *frame)
{
    ControlSession *s=context;++s->diagnostics.requests;s->diagnostics.last_opcode=frame->opcode;s->diagnostics.last_sequence=frame->sequence;
    if((frame->opcode==MEDIA_CONTROL_OPCODE||frame->opcode==PHONE_REPLY_OPCODE||frame->opcode==PHONE_CALL_OPCODE)&&(frame->flags==NDCP_FLAG_RESPONSE||frame->flags==(NDCP_FLAG_RESPONSE|NDCP_FLAG_ERROR))&&frame->length==4U){
        MediaControl_Acknowledge(0U,frame->sequence,U32(frame->payload));return;
    }
    if(s->reply_write-s->reply_read>=CONTROL_QUEUE_DEPTH){++s->diagnostics.queue_full;return;}
    if(frame->flags){
        if(frame->opcode>=UPDATE_OP_BEGIN && frame->opcode<=UPDATE_OP_RESET)UpdateRejected(s,frame,UPDATE_ARGUMENT);
        else Reply(s,frame,NOODOE_CONTROL_ARGUMENT,0U);
        return;
    }
    if(frame->opcode>=UPDATE_OP_BEGIN && frame->opcode<=UPDATE_OP_RESET){
        uint32_t result=s->role!=BLUETOOTH_PHONE?UPDATE_BUSY:update?UpdateService_Handle(update,frame):UPDATE_NOT_CONNECTED;
        if(result!=UPDATE_OK)UpdateRejected(s,frame,result);
        return;
    }
    const uint8_t *p=frame->payload;uint32_t n=frame->length,extra=0U;int32_t result=NOODOE_CONTROL_OK;
    switch(frame->opcode){
    case 0x92U:{
        Bluetooth_LinkState link;Bluetooth_GetLinkState(BLUETOOTH_PHONE,&link);
        if((update&&InstallSession_Active(&update->install))||!Bluetooth_LinkSecure(BLUETOOTH_PHONE,&link))result=NOODOE_CONTROL_BUSY;
        else result=RadioSelfTest_Handle(p,n,s->diagnostics.link_generation,now_ms,payload+4,&extra);
        break;}
    case 0x00U: /* PING echoes at most64bytes so it cannot monopolize the link. */
        if(n>64U)result=NOODOE_CONTROL_ARGUMENT;else{if(n)memcpy(payload+4U,p,n);extra=n;}break;
    case 0x01U:{
        if(n){result=NOODOE_CONTROL_ARGUMENT;break;}
        Put(payload+4U,NDCP_VERSION);Put(payload+8U,HAL_GetUIDw0());Put(payload+12U,HAL_GetUIDw1());Put(payload+16U,HAL_GetUIDw2());
        const volatile uint32_t *meta=(const volatile uint32_t*)0x08008000U;
        for(uint32_t i=0;i<5U;++i)Put(payload+20U+i*4U,meta[i]);
        memset(payload+40U,0,32U);memcpy(payload+40U,"NOODOE-CFW-integrated-v1",24U);extra=68U;break;}
    case 0x03U:case 0x04U:case 0x05U:case 0x07U:case 0x0CU:
        result=NOODOE_CONTROL_UNSUPPORTED;break;
    case 0x0DU: /* Explicit versioned capability mask; no unsupported features. */
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else {
#if NOODOE_PRODUCT
            Put(payload+4U,2U);Put(payload+8U,2U);Put(payload+12U,251U|256U|512U|1024U|2048U|4096U|8192U|16384U|32768U|65536U|131072U|262144U|524288U);
#else
            Put(payload+4U,1U);Put(payload+8U,1U);Put(payload+12U,7U);
#endif
            extra=12U;}break;
#if NOODOE_PRODUCT
    case 0x96:
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else {extern uint32_t RuntimeUpdate_DiagnosticSupported(void);Put(payload+4,1);Put(payload+8,RuntimeUpdate_DiagnosticSupported());extra=8;}break;
#endif
    case 0x02U:if(n)result=NOODOE_CONTROL_ARGUMENT;else extra=BluetoothInfo();break;
    case 0x06U:
        if(n!=1U || p[0]>=BLUETOOTH_ROLE_COUNT)result=NOODOE_CONTROL_ARGUMENT;
        else if(p[0]==(uint8_t)s->role){s->deferred_disconnect=1U;s->deferred_sequence=frame->sequence;}
        else if(Bluetooth_IsPhoneRole(p[0]))result=NOODOE_CONTROL_DENIED;
        else result=Bluetooth_Disconnect((Bluetooth_Role)p[0]);
        break;
    case 0x08U:result=(n!=4U || U32(p)>120U)?NOODOE_CONTROL_ARGUMENT:Bluetooth_SetPairingWindow(U32(p));break;
    case 0x09U:{
        if(s->role!=BLUETOOTH_PHONE){result=NOODOE_CONTROL_BUSY;break;}
        if(n!=44U){result=NOODOE_CONTROL_ARGUMENT;break;}
        if(!phone_callback){result=NOODOE_CONTROL_NOT_READY;break;}
        GnssFix *fix=&snapshot.phone;memset(fix,0,sizeof(*fix));fix->fields=U32(p);fix->latitude_e7=(int32_t)U32(p+4U);fix->longitude_e7=(int32_t)U32(p+8U);
        fix->speed_mm_s=U32(p+12U);fix->course_mdeg=U32(p+16U);fix->altitude_mm=(int32_t)U32(p+20U);fix->utc_ms=U32(p+24U);fix->date_yyyymmdd=U32(p+28U);fix->satellites=U32(p+32U);fix->hdop_milli=U32(p+36U);fix->quality=U32(p+40U);
        result=phone_callback(phone_context,fix,now_ms)?NOODOE_CONTROL_OK:NOODOE_CONTROL_ARGUMENT;if(!result)++s->diagnostics.phone_updates;break;}
    case 0x0AU:case 0x0BU:
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else{extra=frame->opcode==0x0AU?VehicleInfo():GnssInfo();if(!extra)result=NOODOE_CONTROL_NOT_READY;}break;
    case 0x1FU:{
#if NOODOE_PRODUCT
        /* Product authorization comes only from the read-only on-device
         * recovery audit (0x62/63), not a host-supplied "backup done" token. */
        result=NOODOE_CONTROL_UNSUPPORTED;break;
#else
        if(s->role!=BLUETOOTH_PHONE){result=NOODOE_CONTROL_BUSY;break;}
        const volatile uint32_t *meta=(const volatile uint32_t*)0x08008000U;
        if(n!=16U || U32(p)!=HAL_GetUIDw0() || U32(p+4U)!=HAL_GetUIDw1() || U32(p+8U)!=HAL_GetUIDw2() || U32(p+12U)!=0x42414B32U){result=NOODOE_CONTROL_DENIED;break;}
        if(!update || !RECOVERY_VERSION_SUPPORTED(meta[0]) || meta[4]){result=NOODOE_CONTROL_DENIED;break;}
        UpdateService_Authorize(update,UPDATE_STAGE_ARM);++s->diagnostics.authorizations;break;
#endif
    }
#if NOODOE_PRODUCT
    case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x94:case 0x95:case 0x97:case 0x98:case 0x99:case 0x9a:case 0x9b:
        if(update&&update->state!=UPDATE_IDLE&&update->state!=UPDATE_FAILED)result=NOODOE_CONTROL_BUSY;
        else result=CompanionControl_Handle(frame->opcode,p,n,s->diagnostics.link_generation,now_ms,payload+4,&extra);
        break;
    case 0x5BU:{ /* Boot/rollback state: exact Product identity, not legacy448KiB. */
        GateJournalRecord j;
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else if(!BootStore_GetBootInfo(&j))result=NOODOE_CONTROL_NOT_READY;
        else {const uint32_t w[]={2,j.sequence,j.state,j.flags,j.active,j.previous,j.failed_version,j.restored_version,j.failure_reason,j.transaction,j.reset_epoch};
            Words(4,w,11);memcpy(payload+48,j.active_sha,32);extra=76;}break;}
    case 0x9CU:
        if(n==0){Put(payload+4,AppRecovery_TrialRemaining(now_ms));extra=4;}
        else if(n!=36)result=NOODOE_CONTROL_ARGUMENT;
        else if(!AppRecovery_VisualConfirm(U32(p),p+4,s->diagnostics.link_generation))result=NOODOE_CONTROL_DENIED;
        break;
    case 0x5CU:
        if(n!=36)result=NOODOE_CONTROL_ARGUMENT;
        else if(!AppRecovery_ConfirmTrial(U32(p),p+4,s->diagnostics.link_generation))result=NOODOE_CONTROL_DENIED;
        break;
    case 0x61U:
        if(n!=4)result=NOODOE_CONTROL_ARGUMENT;
        else if(!BootStore_RequestResultAck(U32(p)))result=NOODOE_CONTROL_DENIED;
        break;
    case 0x62U:
        if(n||!update)result=NOODOE_CONTROL_ARGUMENT;
        else if(!RoutineUpdate_Request(s->diagnostics.link_generation))result=NOODOE_CONTROL_BUSY;
        break;
    case 0x63U:{
        uint32_t error=0,state=RoutineUpdate_Status(s->diagnostics.link_generation,&error);
        if(n||!update)result=NOODOE_CONTROL_ARGUMENT;
        else {Put(payload+4,state);Put(payload+8,error);extra=8;
            if(state==3)UpdateService_Authorize(update,UPDATE_STAGE_ARM);}
        break;}
    case 0x8CU:
        if(n||!update)result=NOODOE_CONTROL_ARGUMENT;
        else{uint32_t snapshot[20],mask=__get_PRIMASK();__disable_irq();InstallSession copy=update->install;__set_PRIMASK(mask);
            InstallSession_Snapshot(&copy,snapshot);memcpy(payload,snapshot,sizeof(snapshot));extra=76;}break;
    case 0x8d:case 0x8e:case 0x8f:case 0x90:case 0x91:{
        uint32_t audit_error=0;
        if(s->role!=BLUETOOTH_PHONE||!update)result=NOODOE_CONTROL_DENIED;
        else if(frame->opcode!=0x8d&&(!update->authorization||RoutineUpdate_Status(s->diagnostics.link_generation,&audit_error)!=3||audit_error||
                (update->state!=UPDATE_IDLE&&update->state!=UPDATE_FAILED)))result=NOODOE_CONTROL_DENIED;
        else {result=ResourceStore_Transfer(frame->opcode,p,n,payload+4);if(!result)extra=92;}
        break;}
    case 0x86U:
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else{const uint32_t caps[]={2,960,4096,24};extra=Words(4,caps,4)-4;}break;
    case 0x93U:
        if(n||s->role!=BLUETOOTH_PHONE)result=NOODOE_CONTROL_ARGUMENT;
        else{const uint32_t v[]={1,3,HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2(),UPDATE_APP_BYTES,UNINSTALL_TRANSPORT_VERSION};
            static const uint8_t sha[32]=UNINSTALL_IMAGE_SHA;Words(4,v,7);memcpy(payload+32,sha,32);extra=60;}break;
    case 0x64U:
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else if(!AppRecovery_GateIdentity(payload+8))result=NOODOE_CONTROL_NOT_READY;
        else{Put(payload+4,2);extra=36;}break;
    case 0x5EU:
        if(n!=12)result=NOODOE_CONTROL_ARGUMENT;
        else result=DeviceLog_RequestRead(U32(p),U32(p+4),U32(p+8));
        break;
    case 0x5FU:
        if(n!=4)result=NOODOE_CONTROL_ARGUMENT;
        else{result=DeviceLog_ReadResult(U32(p),payload+4);if(!result)extra=256;}
        break;
    case 0x5DU:{ /* Payload-free logging health; file I/O belongs to StorageTask. */
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else{const uint32_t w[]={1,g_device_log.state,g_device_log.error,g_device_log.queued,g_device_log.dropped,g_device_log.coalesced,g_device_log.sequence,g_device_log.written,g_device_log.boot_id};extra=Words(4,w,9)-4;}break;}
    case APP_RECOVERY_OPCODE:{
        uint32_t action=n==8U?U32(p):UINT32_MAX;
        if(s->role!=BLUETOOTH_PHONE)result=NOODOE_CONTROL_BUSY;
        else if(n!=8U)result=NOODOE_CONTROL_ARGUMENT;
        /* An already authenticated stock-return command cannot borrow an
         * active updater transaction or silently bypass its backup gate. */
        else if(action==1U||action==2U){
            if(!update||!__atomic_load_n(&update->authorization,__ATOMIC_ACQUIRE))result=NOODOE_CONTROL_DENIED;
            else if(update->state!=UPDATE_IDLE&&update->state!=UPDATE_FAILED)result=NOODOE_CONTROL_BUSY;
        }
        if(!result)result=(int32_t)AppRecovery_Control(action,U32(p+4),frame->sequence,now_ms);
        Put(payload+4,g_app_recovery.state);Put(payload+8,g_app_recovery.source_ready);
        Put(payload+12,g_app_recovery.verified);Put(payload+16,RECOVERY_STOCK_BYTES);
        Put(payload+20,g_app_recovery.error);Put(payload+24,3U);Put(payload+28,0U);extra=28U;break;}
    case 0x58U:
        if(n)result=NOODOE_CONTROL_ARGUMENT;
        else if(!AppRecovery_Identity(payload+4U,2U))result=NOODOE_CONTROL_NOT_READY;
        else extra=84U;
        break;
#endif
    default:result=NOODOE_CONTROL_UNSUPPORTED;break;
    }
    Reply(s,frame,result,extra);
}
/* A link epoch prevents an old response/reset ACK leaking into a reconnect.
 * Bluetooth increments PHONE reconnects on each successful incoming open. */
static void NewSession(ControlSession *s,uint32_t connected,const Bluetooth_LinkState *link)
{
#if NOODOE_PRODUCT
    if(s->role==BLUETOOTH_PHONE)AppRecovery_ControlDisconnected();
#endif
    if(s->role==BLUETOOTH_PHONE&&update)UpdateService_SetConnected(update,0U);
    NDCP_Init(&s->parser,OnFrame,s);s->reply_read=s->reply_write=0U;s->tx_length=s->tx_offset=0U;s->deferred_disconnect=0U;
    s->diagnostics.connected=connected;++s->diagnostics.link_generation;s->diagnostics.queued_replies=0U;
    if(connected){s->link_cid=link->cid;s->link_epoch=link->reconnects;memcpy(s->link_address,link->address,6U);if(s->role==BLUETOOTH_PHONE&&update)UpdateService_SetConnected(update,1U);}
}
/* The sole phone context owns framing, backpressure, epochs and deferred
 * disconnect acknowledgements. Storage/update keeps its separate owner. */
void NoodoeControl_Init(UpdateService *updater)
{
    memset(sessions,0,sizeof(sessions));memset((void*)&g_noodoe_control,0,sizeof(g_noodoe_control));
    update=updater;phone_callback=NULL;phone_context=NULL;
    for(unsigned i=0;i<BLUETOOTH_PHONE_CAPACITY;++i){ControlSession *s=&sessions[i];
        s->role=Bluetooth_PhoneRole(i);s->diagnostics.magic=0x4E435431U;s->diagnostics.version=2U;
        NewSession(s,0U,NULL);s->diagnostics.initialized=1U;
    }
    g_noodoe_control=sessions[0].diagnostics;
}
void NoodoeControl_SetPhoneGPSCallback(NoodoeControl_PhoneGPSCallback callback,void *context){phone_callback=callback;phone_context=context;}
static void ProcessSession(ControlSession *s,uint32_t now)
{
    if(!s->diagnostics.initialized)return;
    now_ms=now;++s->diagnostics.process_count;Bluetooth_LinkState link;
    uint32_t connected=Bluetooth_GetLinkState(s->role,&link)==BLUETOOTH_OK && link.status==BLUETOOTH_LINK_UP;
    if(!connected){if(s->diagnostics.connected)NewSession(s,0U,NULL);return;}
    if(!s->diagnostics.connected || link.cid!=s->link_cid || link.reconnects!=s->link_epoch || memcmp(link.address,s->link_address,6U))NewSession(s,1U,&link);
    /* Completion means every frame byte reached Bluetooth's local transmit
     * path. tx_queued=0 is not peer receipt; updater adds its own reset delay. */
    if(s->tx_length && s->tx_offset==s->tx_length && !link.tx_queued){
        if(s->tx_generation==s->diagnostics.link_generation){
            if(s->tx_is_update && update)UpdateService_NotifyReplyTransmitted(update,s->tx_sequence,now);
#if NOODOE_PRODUCT
            if(s->role==BLUETOOTH_PHONE&&s->tx[5]==APP_RECOVERY_OPCODE)AppRecovery_ReplySent(s->tx_sequence,now);
#endif
            if(s->deferred_disconnect && s->tx[5]==0x06U && s->tx_sequence==s->deferred_sequence){(void)Bluetooth_Disconnect(s->role);NewSession(s,0U,NULL);return;}
        }
        s->tx_length=s->tx_offset=0U;++s->diagnostics.responses;
    }
    if(!s->tx_length){
        if(s->reply_read!=s->reply_write){ControlReply *reply=&s->replies[s->reply_read%CONTROL_QUEUE_DEPTH];s->tx_length=reply->length;memcpy(s->tx,reply->bytes,s->tx_length);++s->reply_read;s->tx_is_update=0U;}
        else if(s->role==BLUETOOTH_PHONE&&update){s->tx_length=(uint32_t)UpdateService_TakeReply(update,s->tx,sizeof(s->tx));s->tx_is_update=1U;}
        if(!s->tx_length){s->tx_length=MediaControl_Take(0U,s->tx,sizeof(s->tx));s->tx_is_update=0;}
        if(s->tx_length){s->tx_offset=0U;s->tx_sequence=U32(s->tx+8U);s->tx_generation=s->diagnostics.link_generation;}
        s->diagnostics.queued_replies=s->reply_write-s->reply_read;
    }
    /* A media command that never entered the BT queue expires locally. This
     * check does not truncate replies/update frames or recall accepted bytes. */
    if(s->tx_length&&!s->tx_offset&&(s->tx[5]==MEDIA_CONTROL_OPCODE||s->tx[5]==PHONE_REPLY_OPCODE||s->tx[5]==PHONE_CALL_OPCODE)&&!s->tx[6]&&
       !MediaControl_IsPending(0U,s->tx_sequence))s->tx_length=0;
    if(s->tx_length && s->tx_offset<s->tx_length){uint32_t length=s->tx_length-s->tx_offset;if(length>512U)length=512U;
        /* Snapshot validation and enqueue occur under one BT critical section.
         * A close/open between our poll and this call cannot send the remainder
         * of an old response into a newly connected peer's byte stream. */
        int result=Bluetooth_SendSession(s->role,&link,s->tx+s->tx_offset,length);
        if(result==BLUETOOTH_OK){s->tx_offset+=length;s->diagnostics.tx_bytes+=length;}
        else if(result!=BLUETOOTH_QUEUE_FULL && result!=BLUETOOTH_BUSY){++s->diagnostics.tx_failures;NewSession(s,0U,NULL);return;}
    }
    NDCP_Poll(&s->parser,now);
    /* Read one byte at a time to stop exactly when the small reply queue fills.
     * Budget256bytes/process bounds s->parser work and avoids consuming another
     * command whose response cannot yet be preserved. */
    for(uint32_t i=0;i<256U && !s->deferred_disconnect && s->reply_write-s->reply_read<CONTROL_QUEUE_DEPTH;++i){uint8_t byte;
        /* Session-bound dequeue consumes nothing on an epoch/CID/address
         * mismatch. Discard our partial parser/replies immediately; the next
         * Process poll establishes the new session and its fresh authorization. */
        int count=Bluetooth_ReceiveSession(s->role,&link,&byte,1U);
        if(count==BLUETOOTH_NOT_READY){NewSession(s,0U,NULL);return;}
        if(count<=0)break;
        ++s->diagnostics.rx_bytes;NDCP_Feed(&s->parser,&byte,1U,now);
    }
    s->diagnostics.parser_crc_errors=s->parser.crc_errors;s->diagnostics.parser_header_errors=s->parser.header_errors;s->diagnostics.parser_timeouts=s->parser.timeouts;
}

/* Round-robin bounded work: each phone gets up to256 RX bytes and one512-byte
 * TX slice per invocation. A congested peer cannot hold another peer's queue.
 * The legacy diagnostic record remains the primary slot, with version2. */
void NoodoeControl_Process(uint32_t now)
{
    MediaControl_Poll(now);
    for(unsigned i=0;i<BLUETOOTH_PHONE_CAPACITY;++i)ProcessSession(&sessions[i],now);
    g_noodoe_control=sessions[0].diagnostics;
}
/* I/O owner only; callers on other tasks use Runtime's cached connection bits.
 * No key bytes, mutable parser storage, or persistent pairing data are exposed. */
uint32_t NoodoeControl_GetPhoneDiagnostics(uint32_t slot,NoodoeControl_Diagnostics *out)
{
    if(slot>=BLUETOOTH_PHONE_CAPACITY||!out)return 0U;
    *out=sessions[slot].diagnostics;return out->initialized;
}
