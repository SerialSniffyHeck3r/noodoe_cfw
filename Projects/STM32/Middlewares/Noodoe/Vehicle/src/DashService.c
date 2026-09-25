#include "DashService.h"
#include "BSP_Dash.h"
#include "AmbientService.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

volatile Dash_Snapshot g_dash_service;
volatile Dash_Mailbox g_dash_mailbox;
static Dash_Snapshot status;
static Dash_Protocol protocol;
static VehicleService vehicle;
static VehicleSnapshot published_vehicle;
static volatile struct {uint32_t id,command,argument;} request;
static uint32_t next_id,active_id,active_command,request_started;
static uint32_t in_flight,flight_started,flight_completed,flight_failed,abort_requested;
static uint32_t light_override=DASH_LIGHT_AUTO,cached_light,cached_valid,last_sample;
static int32_t requested_bias;
void DashService_SetLightBias(int32_t bias){if(bias>=-5&&bias<=5)__atomic_store_n(&requested_bias,bias,__ATOMIC_RELAXED);}
static uint32_t mailbox_seen,mailbox_operation;
static uint32_t submit_waiting,submit_started;

/* Upper callers only publish a bounded request; all device state stays with
 * the I/O owner. A full slot remains occupied until its completion is published. */
static Dash_Status Queue(uint32_t command,uint32_t argument,uint32_t *id)
{
    if(!id || command<1U || command>3U ||
       (command==DASH_COMMAND_ENABLED && argument>1U) ||
       (command==DASH_COMMAND_LIGHT && argument>9U && argument!=DASH_LIGHT_AUTO))return DASH_ARGUMENT;
    if(__get_IPSR() || __get_PRIMASK())return DASH_CONTEXT;
    taskENTER_CRITICAL();
    Dash_Status result=DASH_ACCEPTED;
    if(!status.initialized)result=DASH_NOT_READY;
    else if(request.id)result=DASH_BUSY;
    else{if(!++next_id)++next_id;request.command=command;request.argument=argument;request.id=next_id;*id=next_id;}
    taskEXIT_CRITICAL();return result;
}
Dash_Status DashService_RequestEnabled(uint32_t enabled,uint32_t *id)
{return Queue(DASH_COMMAND_ENABLED,enabled,id);}
Dash_Status DashService_RequestReconnect(uint32_t *id)
{return Queue(DASH_COMMAND_RECONNECT,0U,id);}
Dash_Status DashService_RequestLight(uint32_t index,uint32_t *id)
{return Queue(DASH_COMMAND_LIGHT,index,id);}

/* Completion is an operation result, not merely the previous queue ACK. */
static void Complete(uint32_t result)
{
    status.completed_id=active_id;status.completed_result=result;
    taskENTER_CRITICAL();request.id=0U;taskEXIT_CRITICAL();
    active_id=active_command=0U;
}
static void Fault(uint32_t reason)
{
    status.error=reason;protocol.pending_request=protocol.pending_light=protocol.pending_stop=0U;
    submit_waiting=0U;
    protocol.seen=0U;if(active_id)Complete(reason);
}

uint32_t DashService_Init(uint32_t now)
{
    if(status.initialized)return status.error;
    uint32_t thresholds[10];BSP_Dash_ReadLightThresholds(thresholds);
    memset(&status,0,sizeof(status));status.magic=DASH_SERVICE_MAGIC;status.version=1U;
    status.initialized=1U;status.calibration_valid=DashProtocol_Init(&protocol,thresholds,now);
    VehicleService_Init(&vehicle,1500U);
    status.error=BSP_Dash_Init();
    if(!status.calibration_valid)status.error=10U;
    if(!status.error)status.error=BSP_Dash_EnableTx(1U);
    status.enabled=status.error==0U;
    g_dash_mailbox.magic=DASH_SERVICE_MAGIC;g_dash_mailbox.version=1U;
    return status.error;
}

/* A stale/error sensor retains the previous successful index. Initial zero is
 * also what this donor actually transmits with ALS failure under stock. The
 * source remains UNAVAILABLE rather than inventing a live lux sample. */
static void Light(void)
{
    Ambient_Snapshot a;AmbientService_GetSnapshot(&a);
    if(status.calibration_valid && a.driver.valid && !a.stale && (!cached_valid || a.driver.sample_ms!=last_sample)){
        cached_light=DashProtocol_LightIndex(&protocol,a.driver.millilux);
        cached_valid=1U;last_sample=a.driver.sample_ms;
    }
    status.light_source=light_override!=DASH_LIGHT_AUTO?DASH_LIGHT_OVERRIDE:
        (!cached_valid?DASH_LIGHT_UNAVAILABLE:(a.driver.valid && !a.stale?DASH_LIGHT_LIVE:DASH_LIGHT_CACHED));
    status.raw_light_index=cached_light;status.light_bias=__atomic_load_n(&requested_bias,__ATOMIC_RELAXED);
    int32_t adjusted=(int32_t)cached_light+(cached_valid?status.light_bias:0);
    if(adjusted<0)adjusted=0;
    if(adjusted>9)adjusted=9;
    protocol.light_index=light_override!=DASH_LIGHT_AUTO?light_override:(uint32_t)adjusted;
    status.light_index=protocol.light_index;status.light_sample_ms=last_sample;
}

/* Raw SWD caller writes only one request while response_seq differs. Snapshot
 * result is frozen until completion and response_seq is the final store. */
static void Mailbox(uint32_t now)
{
    uint32_t seq=g_dash_mailbox.request_seq;
    if(seq && seq!=mailbox_seen && !mailbox_operation){
        uint32_t command=g_dash_mailbox.command,arg=g_dash_mailbox.argument,id=0U;
        __DMB();if(seq!=g_dash_mailbox.request_seq)return;
        Dash_Status accepted=Queue(command,arg,&id);mailbox_seen=seq;
        g_dash_mailbox.operation_id=id;g_dash_mailbox.accept_result=accepted;
        if(accepted==DASH_ACCEPTED)mailbox_operation=id;
        else{g_dash_mailbox.result=accepted;g_dash_mailbox.completed_ms=now;__DMB();g_dash_mailbox.response_seq=seq;}
    }
    if(mailbox_operation && status.completed_id==mailbox_operation){
        g_dash_mailbox.result=status.completed_result;g_dash_mailbox.completed_ms=now;
        mailbox_operation=0U;__DMB();g_dash_mailbox.response_seq=mailbox_seen;
    }
}

/* Finish a previous DMA before accepting commands that change TX ownership.
 * Success is the actual TC counter, not HAL submission or DMA remaining zero. */
static void Completion(uint32_t now)
{
    if(!in_flight)return;
    if(g_bsp_dash.tx_failed!=flight_failed){in_flight=0U;Fault(100U+g_bsp_dash.tx_last_result);return;}
    if(!g_bsp_dash.tx_busy){
        uint32_t kind=in_flight;in_flight=0U;
        if(g_bsp_dash.tx_completed==flight_completed){Fault(104U);return;}
        ++status.tx_frames;
        if(kind==DASH_TX_REQUEST)++status.tx_requests;
        else if(kind==DASH_TX_LIGHT)++status.tx_light;
        else{
            ++status.tx_stops;status.enabled=0U;
            if(BSP_Dash_EnableTx(0U)){Fault(105U);return;}
        }
        if(active_id && ((kind==DASH_TX_STOP && active_command==DASH_COMMAND_ENABLED) ||
           (kind==DASH_TX_REQUEST && active_command!=DASH_COMMAND_LIGHT)))Complete(0U);
    }else if(now-flight_started>100U && status.error!=106U)Fault(106U);
    else if(now-flight_started>50U && !abort_requested){abort_requested=1U;BSP_Dash_RequestTxAbort();}
}

/* Start/reconnect preserves an already good UART transport. Fault recovery
 * asks BSP to reinitialize; a poisoned DMA owner is explicitly refused there. */
static void Commands(uint32_t now)
{
    if(in_flight){
        /* An indeterminate DMA stop retains the BSP buffer. Requests queued
         * meanwhile still receive a failure instead of waiting forever. */
        if(status.error && request.id && !active_id){
            taskENTER_CRITICAL();active_id=request.id;taskEXIT_CRITICAL();Complete(status.error);
        }
        return;
    }
    if(!active_id && request.id){
        /* Take the fixed-size publication under the same lock as Queue. The
         * slot remains occupied throughout execution; no caller can replace it. */
        taskENTER_CRITICAL();
        active_id=request.id;active_command=request.command;
        uint32_t arg=request.argument;taskEXIT_CRITICAL();request_started=now;
        if(active_command==DASH_COMMAND_LIGHT){light_override=arg;Complete(0U);return;}
        if(active_command==DASH_COMMAND_ENABLED && !arg){
            if(!status.enabled){Complete(0U);return;}
            if(status.error){Complete(status.error);return;}
            DashProtocol_Stop(&protocol);
        }else{
            if(!status.calibration_valid){Complete(10U);return;}
            if(status.error){uint32_t e=BSP_Dash_Init();if(e){Complete(e);return;}status.error=0U;}
            uint32_t e=BSP_Dash_EnableTx(1U);if(e){Complete(e);return;}
            status.enabled=1U;DashProtocol_Reconnect(&protocol);
        }
    }
    if(active_id && now-request_started>250U)Fault(107U);
}

void DashService_Process(uint32_t now)
{
    if(!status.initialized)return;
    BSP_Dash_Process();Completion(now);Mailbox(now);Commands(now);
    uint8_t data[256];uint32_t count=BSP_Dash_Read(data,sizeof(data));
    uint32_t previous=vehicle.link_frames_ok;
    if(count)VehicleService_Feed(&vehicle,data,count,now);
    VehicleService_Poll(&vehicle,now);
    DashProtocol_OnFrames(&protocol,vehicle.link_frames_ok-previous,now);
    Light();
    if(status.enabled && !status.error){
        DashProtocol_Process(&protocol,now);
        if(!in_flight){
            uint8_t frame[6];uint32_t length=0U;Dash_TxKind kind=DashProtocol_Peek(&protocol,frame,&length);
            if(kind!=DASH_TX_NONE){
                if(!submit_waiting){submit_waiting=1U;submit_started=now;}
                flight_completed=g_bsp_dash.tx_completed;flight_failed=g_bsp_dash.tx_failed;
                uint32_t e=BSP_Dash_Send(frame,length);
                if(!e){submit_waiting=0U;in_flight=kind;flight_started=now;abort_requested=0U;DashProtocol_Accepted(&protocol,kind,now);}
                else if(e!=2U)Fault(110U+e);
                else if(now-submit_started>50U)Fault(108U);
            }else submit_waiting=0U;
        }
    }
    status.phase=protocol.phase;status.link_up=status.enabled && !status.error && protocol.phase==DASH_PHASE_ACTIVE && protocol.seen;
    status.rx_bytes=g_bsp_dash.rx_bytes;status.rx_frames=vehicle.frames_ok;status.link_frames=vehicle.link_frames_ok;
    status.tx_bytes=g_bsp_dash.tx_bytes;status.uart_errors=g_bsp_dash.errors;status.checksum_errors=vehicle.checksum_errors;
    status.overflows=g_bsp_dash.overflows;status.pending_replies=protocol.pending_light;status.reply_overflows=protocol.overflowed_replies;
    status.pending_id=request.id;++status.process_count;
    VehicleSnapshot v;VehicleService_GetSnapshot(&vehicle,now,&v);
    taskENTER_CRITICAL();g_dash_service=status;published_vehicle=v;taskEXIT_CRITICAL();
    Mailbox(now);
}
void DashService_GetSnapshot(Dash_Snapshot *out)
{if(out){taskENTER_CRITICAL();*out=g_dash_service;taskEXIT_CRITICAL();}}
uint32_t DashService_GetVehicle(VehicleSnapshot *out)
{if(!out)return 0U;taskENTER_CRITICAL();*out=published_vehicle;uint32_t ok=status.initialized;taskEXIT_CRITICAL();return ok;}
