#include "AmbientService.h"
#include "bsp_diagnostics_profile.h"
#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(BSP_Ambient_Diagnostics,phase)==48U,"legacy diagnostics prefix");
_Static_assert(sizeof(Ambient_Mailbox)==AMBIENT_MAILBOX_BYTES,"SWD mailbox wire size");
volatile Ambient_Snapshot g_ambient_service;
volatile Ambient_Mailbox g_ambient_mailbox;
typedef struct {uint32_t command,argument,id,mailbox_seq;} Ambient_Request;
static Ambient_Request request;
static uint32_t initialized,busy,queued,next_id,last_mailbox_seq;
/* Worker-only policy. Each explicit request renews a finite retry budget.
 * Queue access and snapshot publication alone use short critical sections. */
static uint32_t state,requested_hz,desired_enabled,retries,next_retry;
static uint32_t diagnostic_quarantine;
uint32_t AmbientDiagnostics_Execute(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t*);
static const uint32_t retry_delays[3]={1000U,4000U,16000U};

static uint32_t TaskContext(void)
{
    return !(__get_IPSR() || __get_PRIMASK() || __get_BASEPRI() ||
             __get_FAULTMASK() || (__get_CONTROL()&1U));
}
static uint32_t Lock(void)
{
    uint32_t old=__get_PRIMASK();__disable_irq();__DMB();return old;
}
static void Unlock(uint32_t old)
{
    __DMB();__set_PRIMASK(old);
}

/* The service publishes only complete BSP observations. An upper snapshot can
 * never read half of Probe's ID/configuration update. No HAL work under lock. */
static void Publish(void)
{
    BSP_Ambient_Diagnostics driver=g_bsp_ambient;
    /* Keep raw SWD diagnostics meaningful too. GetSnapshot recomputes this
     * relative age at read time; this copy is accurate at publication time. */
    uint32_t age=HAL_GetTick()-driver.sample_ms;
    uint32_t key=Lock();
    g_ambient_service.driver=driver;
    g_ambient_service.state=state;
    g_ambient_service.requested_hz=requested_hz;
    g_ambient_service.desired_enabled=desired_enabled;
    g_ambient_service.retries=retries;
    g_ambient_service.next_retry_ms=next_retry;
    g_ambient_service.sample_age_ms=age;
    g_ambient_service.stale=!driver.valid || age>AMBIENT_SAMPLE_STALE_MS;
    Unlock(key);
}

/* One non-spinning slot includes an operation currently executing in HAL. This
 * prevents an upper request from replacing the persistent request's identity.
 * opid0 is reserved for rejection/automatic recovery,including after wrap. */
static Ambient_Status Submit(uint32_t command,uint32_t argument,uint32_t *opid,
                             uint32_t mailbox_seq)
{
    if(!opid || (command==AMBIENT_COMMAND_PROBE &&
       argument!=BSP_AMBIENT_SLOW_PROBE_HZ && argument!=100000U &&
       argument!=BSP_AMBIENT_DEFAULT_HZ) ||
       (command==AMBIENT_COMMAND_ENABLED && argument>1U) ||
       (command==AMBIENT_COMMAND_ID_DIAGNOSTIC && argument>1U) ||
       (command==AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC && argument!=0U) ||
       (command!=AMBIENT_COMMAND_PROBE && command!=AMBIENT_COMMAND_ENABLED &&
        command!=AMBIENT_COMMAND_ID_DIAGNOSTIC && command!=AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC))
        return AMBIENT_ARGUMENT;
    if(!TaskContext())return AMBIENT_CONTEXT;
#if NOODOE_BOOTSTRAP || !NOODOE_DEEP_DIAGNOSTICS
    if(command>AMBIENT_COMMAND_ENABLED)return AMBIENT_ARGUMENT;
#endif
    uint32_t key=Lock();
    if(!initialized){Unlock(key);return AMBIENT_NOT_INITIALIZED;}
    if(busy){Unlock(key);return AMBIENT_BUSY;}
    if(diagnostic_quarantine && command!=AMBIENT_COMMAND_PROBE) {
        Unlock(key);return AMBIENT_BUSY;
    }
    if(++next_id==0U)++next_id;
    request=(Ambient_Request){command,argument,next_id,mailbox_seq};
    busy=1U;queued=1U;*opid=next_id;
    g_ambient_service.pending_id=next_id;
    g_ambient_service.state=AMBIENT_STATE_QUEUED;
    Unlock(key);return AMBIENT_ACCEPTED;
}

Ambient_Status AmbientService_RequestProbe(uint32_t hz,uint32_t *opid)
{
    return Submit(AMBIENT_COMMAND_PROBE,hz,opid,0U);
}
Ambient_Status AmbientService_RequestEnabled(uint32_t enabled,uint32_t *opid)
{
    return Submit(AMBIENT_COMMAND_ENABLED,enabled,opid,0U);
}
Ambient_Status AmbientService_RequestIDDiagnostic(uint32_t *opid)
{
    return Submit(AMBIENT_COMMAND_ID_DIAGNOSTIC,0U,opid,0U);
}
Ambient_Status AmbientService_RequestIDDiagnosticWithPullup(uint32_t *opid)
{
    return Submit(AMBIENT_COMMAND_ID_DIAGNOSTIC,1U,opid,0U);
}
Ambient_Status AmbientService_RequestAddressDiagnostic(uint32_t *opid)
{
    return Submit(AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC,0U,opid,0U);
}
/* Startup only,before other tasks can call this service. Initialization itself
 * touches RAM only. Default probing is a real queued operation with an ID. */
void AmbientService_Init(void)
{
    if(!TaskContext() || initialized)return;
    memset((void *)&g_ambient_service,0,sizeof(g_ambient_service));
    memset((void *)&g_ambient_mailbox,0,sizeof(g_ambient_mailbox));
    g_ambient_service.magic=AMBIENT_MAILBOX_MAGIC;
    g_ambient_service.version=AMBIENT_SERVICE_VERSION;
    g_ambient_service.initialized=1U;
    g_ambient_mailbox.magic=AMBIENT_MAILBOX_MAGIC;
    g_ambient_mailbox.version=AMBIENT_SERVICE_VERSION;
    requested_hz=BSP_AMBIENT_DEFAULT_HZ;desired_enabled=1U;
    state=AMBIENT_STATE_OFF;initialized=1U;
    Publish();
    uint32_t id;(void)AmbientService_RequestProbe(requested_hz,&id);
}

/* Rejected mailbox requests also get a stable response,without touching the
 * device or consuming an operation ID. driver is frozen per response even if
 * a later automatic retry changes the live diagnostic snapshot. */
static void CompleteMailbox(uint32_t seq,uint32_t id,Ambient_Status accepted,
                            uint32_t result)
{
    if(!seq)return;
    g_ambient_mailbox.operation_id=id;
    g_ambient_mailbox.request_status=(uint32_t)accepted;
    g_ambient_mailbox.result=result;
    g_ambient_mailbox.completed_ms=HAL_GetTick();
    g_ambient_mailbox.state=g_ambient_service.state;
    g_ambient_mailbox.driver=g_ambient_service.driver;
    __DMB();g_ambient_mailbox.response_seq=seq;
}

/* A host commits request_seq last. Double-read that commit word to avoid a
 * torn command/argument pair. Host must wait for response before reusing slot;
 * no debugger halt or arbitrary MCU register access is required by this wire. */
static void PollMailbox(void)
{
    uint32_t seq=g_ambient_mailbox.request_seq;
    if(!seq || seq==last_mailbox_seq)return;
    __DMB();uint32_t command=g_ambient_mailbox.command;
    uint32_t argument=g_ambient_mailbox.argument;
    __DMB();if(seq!=g_ambient_mailbox.request_seq)return;
    last_mailbox_seq=seq;
    uint32_t id=0U;
    Ambient_Status status=Submit(command,argument,&id,seq);
    if(status!=AMBIENT_ACCEPTED)CompleteMailbox(seq,0U,status,0U);
}

/* Recovery never hides the original explicit operation's result. A failure
 * after the third automatic retry remains FAILED until an explicit request. */
static void Failed(uint32_t now)
{
    if(retries<3U){state=AMBIENT_STATE_BACKOFF;next_retry=now+retry_delays[retries];}
    else {state=AMBIENT_STATE_FAILED;next_retry=0U;}
}

/* Worker only. There is at most one hardware operation group per call,with no
 * deliberate sleep. BSP's bounded polling still stalls this low-priority worker
 * (20ms transfer timeout;HAL has a separate25ms BUSY preflight). Probe performs
 * at most3sensor transfers,4when disabling after success. Tick granularity and
 * task preemption add wall time. UI/caller threads never perform that work. */
void AmbientService_Process(uint32_t now_ms)
{
    if(!initialized || !TaskContext())return;
    PollMailbox();
    Ambient_Request active={0};
    uint32_t explicit_request=0U,automatic=0U,key=Lock();
    if(queued){active=request;queued=0U;explicit_request=1U;}
    else if(!busy && state==AMBIENT_STATE_BACKOFF &&
            (int32_t)(now_ms-next_retry)>=0) {
        busy=1U;automatic=1U;
    }
    else if(!busy && (state==AMBIENT_STATE_READY || state==AMBIENT_STATE_DISABLED)) {
        busy=1U;
    }
    else {Unlock(key);return;}
    g_ambient_service.state=AMBIENT_STATE_RUNNING;
    Unlock(key);

    uint32_t result;
    if(explicit_request && (active.command==AMBIENT_COMMAND_ID_DIAGNOSTIC ||
                           active.command==AMBIENT_COMMAND_ADDRESS_DIAGNOSTIC)) {
        /* Separate result namespace and evidence. One diagnostic must not
         * silently restart normal initialization or write sensor config. */
        uint32_t restore;
#if NOODOE_BOOTSTRAP || !NOODOE_DEEP_DIAGNOSTICS
        result=9U;restore=0U;
#else
        result=AmbientDiagnostics_Execute(active.command,active.argument,active.id,active.mailbox_seq,&restore);
#endif
        if(restore==1U) {
            g_bsp_ambient.ready=0U;g_bsp_ambient.valid=0U;
            state=AMBIENT_STATE_FAILED;retries=3U;next_retry=0U;
            diagnostic_quarantine=1U;
        }
    }
    else if(explicit_request || automatic) {
        if(explicit_request) {
            retries=0U;
            if(active.command==AMBIENT_COMMAND_PROBE)requested_hz=active.argument;
            else desired_enabled=active.argument;
        }
        else ++retries;
        if(automatic || active.command==AMBIENT_COMMAND_PROBE) {
            result=BSP_Ambient_Probe(requested_hz);
            if(!result && !desired_enabled)result=BSP_Ambient_SetEnabled(0U);
        }
        else result=BSP_Ambient_SetEnabled(desired_enabled);
        if(result)Failed(HAL_GetTick());
        else {
            state=desired_enabled?AMBIENT_STATE_READY:AMBIENT_STATE_DISABLED;next_retry=0U;
            diagnostic_quarantine=0U;
        }
    }
    else {
        result=BSP_Ambient_Poll();
        /* An invalid optical sample is reported without reinitializing a live
         * bus. A transport failure clears BSP ready and starts bounded recovery. */
        if(result && !g_bsp_ambient.ready){retries=0U;Failed(HAL_GetTick());}
    }
    Publish();
    key=Lock();
    if(explicit_request) {
        g_ambient_service.completed_id=active.id;
        g_ambient_service.completed_result=result;
        g_ambient_service.pending_id=0U;
    }
    /* Complete the mailbox while the request slot is still owned; a concurrent
     * upper request cannot change its frozen state/result before seq commit. */
    Unlock(key);
    if(explicit_request)CompleteMailbox(active.mailbox_seq,active.id,AMBIENT_ACCEPTED,result);
    key=Lock();busy=0U;Unlock(key);
}

/* Storage owner only: stop sensor conversions without changing the user's
 * enabled policy. One bounded transaction per edge; a failed sensor cannot
 * create an endless OFF retry loop or prevent the rest of the board sleeping. */
uint32_t AmbientService_SetSleeping(uint32_t sleeping)
{
    static uint32_t asleep;
    if(!initialized||!TaskContext())return 0;
    uint32_t key=Lock(),pending=busy||queued;Unlock(key);
    if(pending)return 0;
    sleeping=!!sleeping;if(sleeping==asleep&&(!sleeping||!g_bsp_ambient.enabled))return 1;
    asleep=sleeping;
    if(g_bsp_ambient.ready){
        if(BSP_Ambient_SetEnabled(sleeping?0U:desired_enabled))Failed(HAL_GetTick());
        else state=sleeping||!desired_enabled?AMBIENT_STATE_DISABLED:AMBIENT_STATE_READY;
    }
    Publish();return 1;
}

/* No bus reads. The atomic copy is bounded to128bytes; age uses unsigned
 * subtraction and remains correct across the32-bit millisecond tick wrap. */
void AmbientService_GetSnapshot(Ambient_Snapshot *out)
{
    if(!out)return;
    uint32_t key=Lock();*out=g_ambient_service;Unlock(key);
    out->sample_age_ms=HAL_GetTick()-out->driver.sample_ms;
    out->stale=!out->driver.valid || out->sample_age_ms>AMBIENT_SAMPLE_STALE_MS;
}
