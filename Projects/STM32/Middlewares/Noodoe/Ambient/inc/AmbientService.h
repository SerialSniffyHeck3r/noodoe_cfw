#ifndef AMBIENT_SERVICE_H
#define AMBIENT_SERVICE_H
#include <stdint.h>
#include "BSP_Ambient.h"
#include "BSP_AmbientBitbang.h"
#include "BSP_AmbientAddress.h"

#define AMBIENT_SERVICE_VERSION 1U
#define AMBIENT_MAILBOX_MAGIC 0x414D4231U
#define AMBIENT_MAILBOX_BYTES 124U
#define AMBIENT_SAMPLE_STALE_MS 2500U
typedef enum {AMBIENT_ACCEPTED=0,AMBIENT_BUSY,AMBIENT_ARGUMENT,
    AMBIENT_NOT_INITIALIZED,AMBIENT_CONTEXT} Ambient_Status;
typedef enum {AMBIENT_STATE_OFF=0,AMBIENT_STATE_QUEUED,AMBIENT_STATE_RUNNING,
    AMBIENT_STATE_READY,AMBIENT_STATE_DISABLED,AMBIENT_STATE_BACKOFF,
    AMBIENT_STATE_FAILED} Ambient_State;
typedef enum {AMBIENT_COMMAND_PROBE=1,AMBIENT_COMMAND_ENABLED=2,
    AMBIENT_COMMAND_ID_DIAGNOSTIC=3,AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC=4} Ambient_Command;

/* Task-safe cached observation. completed_result is the BSP result of the
 * identified request, not acceptance status. Automatic retries never overwrite
 * that request's completion. age/stale are recomputed without bus IO on Get. */
typedef struct {
    uint32_t magic,version,initialized,state,pending_id,completed_id;
    uint32_t completed_result,requested_hz,desired_enabled,retries,next_retry_ms;
    uint32_t sample_age_ms,stale;
    BSP_Ambient_Diagnostics driver;
} Ambient_Snapshot;
extern volatile Ambient_Snapshot g_ambient_service;

/* Fixed124-byte SWD mailbox,31little-endian u32 words. Host exclusively writes
 * command@12 and argument@16 then nonzero request_seq@8 LAST. One request must
 * finish before the host changes these fields. Worker freezes result/driver
 * evidence, then commits response_seq@20 LAST. Re-read response_seq around a
 * full read and match request_seq. operation_id@24,request_status@28,result@32,
 * completed_ms@36,state@40,reserved@44,driver76bytes@48. No raw register write,
 * arbitrary pin, reboot or storage operation is exposed. A rejected request has
 * operation_id0 and request_status!=ACCEPTED; result is meaningful only after
 * accepted completion. command1 argument80000/100000/400000;command2 argument0/1.
 * command3 argument0 performs one20kHz NOPULL ID-only diagnostic;argument1
 * explicitly selects a one-shot PC9-only weak pull-up,restored afterwards. Its result uses
 * BSP_AmbientBitbang_Result; read the separate versioned diagnostic with matching
 * operation_id/request_seq. command4 argument0 checks the fixed TI ADDR set
 * with temporary PC9 weak pull-up; read g_bsp_ambient_address's separate288B
 * record. The legacy driver below is not either bitbang diagnostic's evidence. */
typedef struct {
    uint32_t magic,version,request_seq,command,argument,response_seq;
    uint32_t operation_id,request_status,result,completed_ms,state,reserved;
    BSP_Ambient_Diagnostics driver;
} Ambient_Mailbox;
extern volatile Ambient_Mailbox g_ambient_mailbox;

/* Owner startup,once before concurrent callers: RAM-only,queues default400kHz
 * probe. No HAL bus operation occurs until Process. No auto-format/reboot. */
void AmbientService_Init(void);
/* The storage worker alone calls Process. At most one explicit/automatic
 * operation is performed per call. BSP polling is an intermediate bounded
 * synchronous implementation:20ms transfer timeout plus HAL's25ms BUSY
 * preflight,tick granularity and scheduling latency. A normal probe uses at
 * most3sensor transfers (4when disabling afterwards). Never run in UI/ISR or
 * with interrupts masked.
 * Failed recovery is retried after1/4/16seconds then stops until an explicit
 * request. This is an asynchronous upper API,not an interrupt-based I2C driver. */
void AmbientService_Process(uint32_t now_ms);
/* Storage owner sleep edge; returns0 only while a queued request needs draining. */
uint32_t AmbientService_SetSleeping(uint32_t sleeping);
/* Copies a request immediately into one slot; ACCEPTED means queued. A running
 * operation occupies that slot until completion. No waiting or hardware IO.
 * Reject ISR/masked callers. opid must be nonnull; it changes only on ACCEPTED. */
Ambient_Status AmbientService_RequestProbe(uint32_t hz,uint32_t *opid);
Ambient_Status AmbientService_RequestEnabled(uint32_t enabled,uint32_t *opid);
/* Explicit one-shot ID-only measurement; never automatic, never configures the
 * sensor, and does not change the normal speed/enable/retry policy. A failed
 * restoration forces FAILED until an explicit normal probe repairs the bus;
 * other requests return BUSY during this quarantine. No automatic diagnostic. */
Ambient_Status AmbientService_RequestIDDiagnostic(uint32_t *opid);
/* Explicit electrical bias experiment; accepted only when the ordinary
 * diagnostic would be allowed. No normal probe/retry inherits this pull-up. */
Ambient_Status AmbientService_RequestIDDiagnosticWithPullup(uint32_t *opid);
/* Fixed TI0x44..47 address/identity diagnostic only. No caller-selected address
 * or register; same one-shot bias/restore/quarantine policy as the ID variant. */
Ambient_Status AmbientService_RequestAddressDiagnostic(uint32_t *opid);
uint32_t AmbientService_GetAddressDiagnosticSnapshot(BSP_AmbientAddress_Diagnostics *out);
/* Returns1 for a completed stable record,0 while none/running or null. On0 the
 * output is unchanged. No hardware access; caller matches its operation_id. */
uint32_t AmbientService_GetIDDiagnosticSnapshot(BSP_AmbientBitbang_Diagnostics *out);
/* Atomic copy only; caller may reuse its buffer immediately. null is ignored.
 * Snapshot.ready is driver.ready; inspect valid/stale before using millilux. */
void AmbientService_GetSnapshot(Ambient_Snapshot *out);
#endif
