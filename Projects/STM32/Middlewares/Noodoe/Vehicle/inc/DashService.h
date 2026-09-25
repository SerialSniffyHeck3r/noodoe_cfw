#ifndef DASH_SERVICE_H
#define DASH_SERVICE_H
#include <stdint.h>
#include "Vehicle_Service.h"
#include "Dash_Protocol.h"

#define DASH_SERVICE_MAGIC 0x44534831UL
#define DASH_LIGHT_AUTO UINT32_MAX
typedef enum {DASH_ACCEPTED=0,DASH_BUSY,DASH_ARGUMENT,DASH_NOT_READY,DASH_CONTEXT} Dash_Status;
typedef enum {DASH_COMMAND_ENABLED=1,DASH_COMMAND_RECONNECT,DASH_COMMAND_LIGHT} Dash_Command;
typedef enum {DASH_LIGHT_UNAVAILABLE=0,DASH_LIGHT_LIVE,DASH_LIGHT_CACHED,DASH_LIGHT_OVERRIDE} Dash_LightSource;

/* One cached observation. TX byte acceptance, physical TC completion and wire
 * responses are separate counters. error is current service failure; UART and
 * checksum counters are cumulative history, not a permanently failed link. */
typedef struct {
    uint32_t magic,version,initialized,enabled,phase,link_up,error;
    uint32_t rx_bytes,rx_frames,link_frames,tx_bytes,tx_frames,tx_requests,tx_light,tx_stops;
    uint32_t uart_errors,checksum_errors,overflows,pending_replies,reply_overflows;
    uint32_t light_index,light_source,light_sample_ms,calibration_valid;
    uint32_t pending_id,completed_id,completed_result,process_count;
    uint32_t raw_light_index;int32_t light_bias;
} Dash_Snapshot;
extern volatile Dash_Snapshot g_dash_service;

/* Fixed SWD request mailbox: publish command/argument then request_seq LAST.
 * Worker commits response_seq LAST only when the accepted operation completes.
 * Commands are the same public API below; no arbitrary frame/register/write is
 * exposed. LIGHT0..9 is explicitly diagnostic;FFFFFFFF restores live policy. */
typedef struct {uint32_t magic,version,request_seq,command,argument,response_seq,
    operation_id,accept_result,result,completed_ms;} Dash_Mailbox;
extern volatile Dash_Mailbox g_dash_mailbox;

/* I/O task owns Init/Process. Init reads this module's lower-flash calibration
 * and arms stock-style startup; it does not format, reset or write flash.
 * Process drains bounded RX, performs at most one DMA submission, and handles
 * completion/timeouts. Existing BSP error recovery can use bounded HAL abort. */
uint32_t DashService_Init(uint32_t now_ms);
void DashService_Process(uint32_t now_ms);
/* Any unmasked task may enqueue one command. Immediate ACCEPTED supplies an
 * operation ID, not hardware success. Busy calls leave *id unchanged. Stop
 * completes only after CMD01=00 TC and disabling TX; reconnect after CMD01=04
 * TC. Neither result proves that a physical dashboard accepted the command. */
Dash_Status DashService_RequestEnabled(uint32_t enabled,uint32_t *id);
Dash_Status DashService_RequestReconnect(uint32_t *id);
Dash_Status DashService_RequestLight(uint32_t index_or_auto,uint32_t *id);
/* RAM-only, task-safe copies; no HAL call or serial traffic from a getter. */
void DashService_GetSnapshot(Dash_Snapshot *out);
/* RAM policy update; worker alone emits A1. Positive raises the wire index,
 * not a claim about optical brightness. Overrides bypass this bias. */
void DashService_SetLightBias(int32_t bias);
uint32_t DashService_GetVehicle(VehicleSnapshot *out);
#endif
