#ifndef NOODOE_CONTROL_H
#define NOODOE_CONTROL_H
#include <stdint.h>
#include "GNSS_Service.h"
#include "Update_Service.h"
enum { NOODOE_CONTROL_OK=0,NOODOE_CONTROL_ARGUMENT=1,NOODOE_CONTROL_NOT_READY=2,
       NOODOE_CONTROL_BUSY=3,NOODOE_CONTROL_UNSUPPORTED=4,NOODOE_CONTROL_DENIED=5,
       NOODOE_CONTROL_TRANSPORT=6 };
typedef uint32_t (*NoodoeControl_PhoneGPSCallback)(void *context,const GnssFix *fix,uint32_t now_ms);
typedef struct {
    uint32_t magic,version,initialized,connected,link_generation,process_count;
    uint32_t requests,responses,errors,queue_full,rx_bytes,tx_bytes,tx_failures;
    uint32_t parser_crc_errors,parser_header_errors,parser_timeouts,last_opcode;
    uint32_t last_sequence,last_result,queued_replies,phone_updates,authorizations;
} NoodoeControl_Diagnostics;
extern volatile NoodoeControl_Diagnostics g_noodoe_control;
/* One I/O task owns all functions after Init. The updater object/platform must
 * already be initialized and outlive control. StorageTask owns Update_Process;
 * Control only enqueues its requests and drains complete replies. NULL updater
 * keeps ordinary control available and rejects update authorization/requests. */
void NoodoeControl_Init(UpdateService *updater);
void NoodoeControl_SetPhoneGPSCallback(NoodoeControl_PhoneGPSCallback callback,void *context);
/* One nonblocking PHONE transport with its own parser/replies/epoch.
 * Call every few milliseconds. Slot0 owns the updater and phone GPS input.
 * Retired device/discovery commands return UNSUPPORTED; IDs are not reused.
 * No filesystem, SPI, metadata write, device reset or graphics call happens here. */
void NoodoeControl_Process(uint32_t now_ms);
/* I/O task only, copied diagnostics for slot0. Other slots return0. */
uint32_t NoodoeControl_GetPhoneDiagnostics(uint32_t slot,NoodoeControl_Diagnostics *out);
#endif
