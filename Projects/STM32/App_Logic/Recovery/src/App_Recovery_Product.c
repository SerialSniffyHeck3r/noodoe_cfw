#if NOODOE_PRODUCT
#include "App_Recovery.h"
#include "gate_abi.h"
#include "gate_policy.h"
#include "BootStore.h"
#include "FreeRTOS.h"
#include "stm32f4xx.h"
#include "bsp_fault.h"
#include "DeviceLog.h"
#include "NoodoeBluetooth.h"
#include "NoodoeControl.h"
#include "Config_Store.h"
#include "Photo_Store.h"
#include "SystemError.h"
#include "RuntimeUpdate.h"
#include "ResourceStore.h"
#include "Health_Service.h"
_Static_assert(HEALTH_STABLE_BOOT_MS==GATE_CONFIRM_MS,"trial health interval");
#include <string.h>
/* Candidate contract is in a fixed read-only location inspected before any
 * installation. Older APPs cannot silently disable trial acknowledgement. */
const uint32_t g_product_contract[4] __attribute__((section(".product_contract"),used))={0x3250554EU,2,2,0x60000};
/* Gate owns pre-kernel recovery. The Product keeps only request/gesture and
 * storage-owner confirmation, with no duplicate recovery renderer or engine. */
#if configAPPLICATION_ALLOCATED_HEAP == 1
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((aligned(8)));
#endif
volatile GateMailbox g_recovery_mailbox __attribute__((section(".gate_mailbox"),aligned(8)));
volatile AppRecoveryStatus g_app_recovery;
static uint32_t reply_sequence,reply_sent_ms,reply_sent,request_ms;
static uint32_t local_request,buttons_initialized;
static GateGesture runtime_buttons;
static UpdateSha256 identity_hash;
static uint8_t identity_digests[96];
static uint32_t identity_phase,identity_offset;
static volatile uint32_t identity_ready,healthy_pending;
static volatile uint32_t trial_epoch,visual_epoch;
static uint32_t trial_started,trial_known,trial_required;
/* Trial still needs BT and a running deadline after key OFF. Preserve the
 * requested power policy; the middleware releases this veto on confirmation. */
uint32_t PowerService_DeepAllowed(void){return !trial_required;}
uint32_t PowerService_RunRequired(void){UpdateService *u=RuntimeUpdate_GetService();return trial_required||ResourceStore_TransferActive()||(u&&InstallSession_Active(&u->install));}
/* A visual acknowledgement is scoped to the live SPP generation and exact
 * boot candidate. It cannot refresh the device-owned rollback deadline. */
uint32_t AppRecovery_TrialRemaining(uint32_t now)
{uint32_t elapsed=now-trial_started;return trial_required&&trial_known&&elapsed<GATE_APP_CONFIRM_DEADLINE_MS?GATE_APP_CONFIRM_DEADLINE_MS-elapsed:0;}
uint32_t AppRecovery_VisualConfirm(uint32_t sequence,const uint8_t sha[32],uint32_t epoch)
{if(!epoch||!BootStore_MatchTrial(sequence,sha))return 0;__atomic_store_n(&visual_epoch,epoch,__ATOMIC_RELEASE);return 1;}
uint32_t AppRecovery_ConfirmTrial(uint32_t sequence,const uint8_t sha[32],uint32_t epoch)
{if(!epoch||__atomic_load_n(&visual_epoch,__ATOMIC_ACQUIRE)!=epoch||!BootStore_MatchTrial(sequence,sha))return 0;
 __atomic_store_n(&trial_epoch,epoch,__ATOMIC_RELEASE);return 1;}
uint32_t AppRecovery_TrialPhase(void)
{
 if(g_system_error.active||g_boot_store.error)return TRIAL_ERROR;
 if(g_bluetooth.state!=BLUETOOTH_STATE_READY)return TRIAL_RADIO;
 if(!g_noodoe_control.connected)return TRIAL_PHONE;
 uint32_t epoch=__atomic_load_n(&trial_epoch,__ATOMIC_ACQUIRE);
 if(!epoch||epoch!=g_noodoe_control.link_generation)return TRIAL_SCREEN;
 return __atomic_load_n(&healthy_pending,__ATOMIC_ACQUIRE)?TRIAL_SAVING:TRIAL_HEALTH;
}
static uint32_t RuntimeState(void){return __atomic_load_n(&g_app_recovery.state,__ATOMIC_ACQUIRE);}
static void RuntimeStateSet(uint32_t state){__atomic_store_n(&g_app_recovery.state,state,__ATOMIC_RELEASE);}
/* Publish the retained CRC-protected request with magic last. A reset during
 * publication cannot authorize stock restore; durable gate boot policy applies. */
static void MailboxRequest(uint32_t reason)
{
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 GateRetained_Request(&r,reason);g_recovery_mailbox.request.magic=0;__DMB();
 memcpy((void*)((uintptr_t)&g_recovery_mailbox.request+4),(const uint8_t*)&r+4,sizeof(r)-4);
 __DMB();g_recovery_mailbox.request.magic=r.magic;__DSB();
}
uint32_t AppRecovery_IsFaultBoot(void)
{GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));return GateRetained_Valid(&r)&&r.reason==GATE_REASON_FAULT;}
void AppRecovery_FaultReset(uint32_t code)
{g_app_recovery.error=code;
 GateFault f={0};f.boot=g_recovery_mailbox.request.sequence;f.code=code;
 const volatile uint32_t *source=&g_bsp_fault.ipsr;
 for(uint32_t i=0;i<8;i++)f.registers[i]=source[i];
 /* These are observed MSP/PSP, not falsely advertised as stacked PC/LR. */
 GateFault_Seal(&f);memcpy((void*)&g_recovery_mailbox.fault,&f,sizeof(f));__DSB();
 MailboxRequest(GATE_REASON_FAULT);NVIC_SystemReset();}
void BSP_FaultRecoveryRequested(uint32_t code){AppRecovery_FaultReset(code);}
void AppRecovery_RequestLocal(void){MailboxRequest(GATE_REASON_WAIT);NVIC_SystemReset();}
void AppRecovery_EarlyRun(void)
{/* Gate already validated the executable and took the boot-attempt record. */
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));
 if(!GateRetained_Valid(&r)){MailboxRequest(GATE_REASON_WAIT);NVIC_SystemReset();}
 g_app_recovery.reason=r.reason;
 GateBootContext b;memcpy(&b,(const void*)&g_recovery_mailbox.boot,sizeof(b));
 /* A legacy Gate cannot understand v2 rollback journals. Never let this APP
  * silently upgrade durable metadata under that older recovery executable. */
 if(!GateBootContext_Valid(&b)||b.sequence!=r.sequence){MailboxRequest(GATE_REASON_WAIT);NVIC_SystemReset();return;}
 trial_required=(b.flags&GATE_F_TRIAL)!=0;
 ConfigStore_SetTrialView((b.flags&(GATE_F_TRIAL|GATE_F_RESET_PENDING))?(b.transaction==GATE_RESTORE_RETAINED&&b.generation==1?2:1):0);
}
/* Health task only posts; SPI/NOR remain exclusively in StorageTask. */
void HealthService_StableBoot(void){__atomic_store_n(&healthy_pending,1U,__ATOMIC_RELEASE);}

/* These requests never touch storage or reset in the parser callback. New
 * link epochs cancel old confirmations before an ACK can leak across peers. */
uint32_t AppRecovery_Control(uint32_t action,uint32_t token,uint32_t sequence,uint32_t now)
{
 if(action>3U||(action==0U||action==3U?token!=0U:token!=RECOVERY_CONFIRM_TOKEN))return RECOVERY_ARGUMENT;
 if(!action)return 0;
 if(action==3U){AppRecovery_ControlDisconnected();return 0;}
 uint32_t state=RuntimeState();
 if(action==1U){if(state==APP_RECOVERY_RESET_WAIT||state==APP_RECOVERY_RESET_EXECUTING)return RECOVERY_STATE;
  g_app_recovery.result=0;request_ms=now;RuntimeStateSet(APP_RECOVERY_WAIT_CONFIRM);return 0;}
 if(state!=APP_RECOVERY_WAIT_CONFIRM||now-request_ms>30000U)return RECOVERY_STATE;
 reply_sequence=sequence;__atomic_store_n(&reply_sent,0U,__ATOMIC_RELEASE);
 RuntimeStateSet(APP_RECOVERY_RESET_WAIT);return 0;
}
void AppRecovery_ControlDisconnected(void)
{
 if(local_request)return;
 uint32_t state=RuntimeState();
 /* Once the storage owner claims reset, cancellation has reached its final
  * boundary. Before that claim a disconnect wins atomically and prevents it. */
 while(state!=APP_RECOVERY_RESET_EXECUTING){
  if(__atomic_compare_exchange_n(&g_app_recovery.state,&state,APP_RECOVERY_NORMAL,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
   __atomic_store_n(&reply_sent,0U,__ATOMIC_RELEASE);break;}
 }
}
void AppRecovery_ReplySent(uint32_t sequence,uint32_t now)
{if(RuntimeState()==APP_RECOVERY_RESET_WAIT&&sequence==reply_sequence){reply_sent_ms=now;__atomic_store_n(&reply_sent,1U,__ATOMIC_RELEASE);}}
uint32_t AppRecovery_RuntimeRequested(void)
{uint32_t state=RuntimeState();return state==APP_RECOVERY_RESET_WAIT||state==APP_RECOVERY_RESET_EXECUTING;}
void AppRecovery_RuntimeProcess(uint32_t now,uint32_t drained)
{
 if(!drained||!__atomic_load_n(&reply_sent,__ATOMIC_ACQUIRE)||now-reply_sent_ms<1500U)return;
 uint32_t expected=APP_RECOVERY_RESET_WAIT;
 if(__atomic_compare_exchange_n(&g_app_recovery.state,&expected,APP_RECOVERY_RESET_EXECUTING,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
  MailboxRequest(GATE_REASON_STOCK);NVIC_SystemReset();}
}
/* Poll physical levels before the deep-sleep branch; OFF must be observed.
 * The user gesture is identical in Gate WAIT and a healthy running Product. */
void AppRecovery_RuntimeButtons(uint32_t now)
{
 if(local_request)return;
 uint32_t enter=(GPIOA->IDR&GPIO_PIN_15)==0;
 if(!buttons_initialized){GateGesture_Init(&runtime_buttons,now,enter);buttons_initialized=1;}
 if(GateGesture_Process(&runtime_buttons,now,(GPIOG->IDR&GPIO_PIN_13)==0,enter)){
  local_request=1;reply_sent_ms=now-1500U;
  __atomic_store_n(&reply_sent,1U,__ATOMIC_RELEASE);RuntimeStateSet(APP_RECOVERY_RESET_WAIT);
 }
}

/* Only the storage/bootstrap owner advances this read-only computation. The
 * release word publishes both finished hashes to the independent I/O owner. */
void AppRecovery_IdentityProcess(void)
{
 if(__atomic_load_n(&identity_ready,__ATOMIC_ACQUIRE))return;
 if(!identity_offset)UpdateSha256_Init(&identity_hash);
 uint32_t limit=identity_phase==2?0x10000U:identity_phase?0x8000U:RECOVERY_STOCK_BYTES;
 uint32_t base=identity_phase==1?0x08000000U:UPDATE_APP_BASE;
 UpdateSha256_Feed(&identity_hash,(const uint8_t*)(uintptr_t)(base+identity_offset),4096);
 identity_offset+=4096;
 if(identity_offset==limit){UpdateSha256_Final(&identity_hash,identity_digests+32U*identity_phase);
  identity_offset=0;if(identity_phase==2)__atomic_store_n(&identity_ready,1U,__ATOMIC_RELEASE);else identity_phase++;}
}
uint32_t AppRecovery_GateIdentity(uint8_t out[32])
{if(!out||!__atomic_load_n(&identity_ready,__ATOMIC_ACQUIRE))return 0;memcpy(out,identity_digests+64,32);return 1;}
uint32_t AppRecovery_Identity(uint8_t out[84],uint32_t role)
{
 if(!out||!__atomic_load_n(&identity_ready,__ATOMIC_ACQUIRE))return 0;
 uint32_t fields[5]={1,role,HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};
 /* Scalar array, no struct padding; STM32/Gate ABI is little endian. */
 memcpy(out,fields,sizeof(fields));
 memcpy(out+20,identity_digests,64);return 1;
}

/* Only called through StorageTask's steady-write arbitration, never during a
 * NOR backup/quiesce lease, photo commit, resource update or installer. */
void AppRecovery_StorageProcess(void)
{
 BootStore_Process();
 GateJournalRecord j;if(!BootStore_GetBootInfo(&j))return;
 if(j.flags&GATE_F_RESET_PENDING){uint32_t revision;
  if(!ConfigStore_ResetPreferences(j.reset_epoch,&revision)&&ConfigStore_Result(revision)==CFW_OK&&!PhotoStore_ResetSlots())(void)BootStore_FinishSettingsReset(j.reset_epoch);
  return;
 }
 if(j.state==GATE_J_CONFIRMED&&!(j.flags&(GATE_F_TRIAL|GATE_F_RESET_PENDING)))ConfigStore_SetTrialView(0);
 if(j.state!=GATE_J_BOOT_PENDING)return;
 uint32_t healthy=__atomic_load_n(&healthy_pending,__ATOMIC_ACQUIRE);
 if((j.flags&GATE_F_TRIAL)&&AppRecovery_TrialPhase()!=TRIAL_SAVING)return;
 /* Keep the health result visible while the queued journal commit runs.
  * Value2 means posted, never a second confirmation request. */
 if(healthy==1U){__atomic_store_n(&healthy_pending,2U,__ATOMIC_RELEASE);BootStore_RequestConfirm();}
}

/* Deadline is observed even while a transfer/backup owns NOR. Reset intent
 * waits for that owner's bounded transaction to drain; no NOR write occurs
 * here. A stuck owner is instead handled by the independent watchdog. */
void AppRecovery_TrialTick(uint32_t now,uint32_t drained)
{
 if(!trial_required)return;
 if(!trial_known){trial_known=1;trial_started=now;}
 GateJournalRecord j;if(BootStore_GetBootInfo(&j)&&!(j.flags&GATE_F_TRIAL)){trial_required=0;return;}
 uint32_t reason=(g_bluetooth.state==BLUETOOTH_STATE_FAULT||g_system_error.active)?GATE_REASON_INIT_FAILED:
     (now-trial_started>=GATE_APP_CONFIRM_DEADLINE_MS?GATE_REASON_TRIAL_TIMEOUT:0);
 if(reason&&drained){MailboxRequest(reason);NVIC_SystemReset();}
}

#endif
