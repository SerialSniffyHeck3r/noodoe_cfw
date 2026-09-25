#if NOODOE_PRODUCT
#include "RuntimeUpdate.h"
#include "RuntimeUninstall.h"
#include "Uninstall_Expected.h"
#include "Recovery_Core.h"
#include "BootStore.h"
#include "Resources.h"
#include "ResourceStore.h"
#include "Update_Metadata.h"
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "BSP_Buttons.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Routine Product updates use the inactive audited FAT container and Gate
 * S5-S7 installer. Only the exact release-pinned standalone uninstaller may
 * use the original pending word: replacing S4 is intentional for that route. */
volatile RuntimeUpdate_Diagnostics g_runtime_update;
static UpdateService *service;
static volatile uint32_t published;
static uint32_t transaction,process_time,requirement_checked,requirement_valid;
static ResourceRequirement requirement;
#define RU_ALIGNMENT 32U
#define RU_SERVICE_BYTES ((sizeof(UpdateService)+RU_ALIGNMENT-1U)&~(size_t)(RU_ALIGNMENT-1U))
static uint32_t Ready(void){return __atomic_load_n(&published,__ATOMIC_ACQUIRE);}
/* An updater is never callable from an exception, unprivileged context or a
 * masked scheduler. StorageTask remains its sole normal-context owner. */
static uint32_t ContextOkay(void)
{return __get_IPSR()==0U&&(__get_CONTROL()&1U)==0U&&!__get_PRIMASK()&&!__get_BASEPRI()&&!__get_FAULTMASK();}
/* Recheck the actual tested memory geometry and known NOR identity for every
 * operation; a stale ready bit alone does not confer an I/O capability. */
static uint32_t HardwareReady(void)
{return g_bsp_ram.ready&&g_bsp_ram.result==0U&&g_bsp_ram.capacity_bytes<=BSP_RAM_GEOMETRY_BYTES&&
 g_bsp_ram.capacity_bytes>=BSP_RAM_TEST_BYTES+RU_SERVICE_BYTES&&g_bsp_nor.ready&&
 g_bsp_nor.capacity_bytes==BSP_NOR_CAPACITY_BYTES&&g_bsp_nor.jedec_id==BSP_NOR_EXPECTED_JEDEC_ID;}
static uint32_t Context(void *p)
{return Ready()&&p==service&&ContextOkay()&&HardwareReady();}
static uint32_t Authorized(void)
{return __atomic_load_n(&service->connected,__ATOMIC_ACQUIRE)&&__atomic_load_n(&service->authorization,__ATOMIC_ACQUIRE)&&__atomic_load_n(&service->link_generation,__ATOMIC_ACQUIRE)==service->generation;}
static uint32_t Diagnostic(void){return service->target==UPDATE_TARGET_DIAGNOSTIC;}
uint32_t RuntimeUpdate_DiagnosticSupported(void)
{const volatile uint32_t *f=(const volatile uint32_t*)GATE_FEATURE_ADDRESS;
 return f[0]==GATE_FEATURE_MAGIC&&f[1]==1&&(f[2]&GATE_FEATURE_DIAGNOSTIC)&&f[3]==~f[2];}
static uint32_t Uninstall(void){return service->target==UPDATE_TARGET_UNINSTALL;}
static uint32_t Range(uint32_t address,uint32_t n)
{uint32_t bytes=UpdateService_ImageBytes(service);return address>=UPDATE_STAGE_BASE&&address<UPDATE_STAGE_BASE+bytes&&n&&n<=UPDATE_STAGE_BASE+bytes-address;}
static uint32_t Result(uint32_t r){g_runtime_update.last_platform_result=r;return r;}
static uint32_t Read(void *p,uint32_t a,void *data,uint32_t n)
{if(!Context(p)||!Range(a,n)||!data)return Result(RUNTIME_UPDATE_ARGUMENT);return Result(Uninstall()?RuntimeUninstall_Read(a,data,n):BootStore_Read(a-UPDATE_STAGE_BASE,data,n));}
static uint32_t Enable(void *p,uint32_t tx)
{if(!Context(p)||!Authorized()||ResourceStore_TransferActive()||ResourceStore_Busy()||(service->target!=UPDATE_TARGET_GATE_CFW&&!Uninstall()&&!Diagnostic())||(Diagnostic()&&!RuntimeUpdate_DiagnosticSupported())||!tx)return Result(RUNTIME_UPDATE_LOCKED);
 uint32_t r=Uninstall()?RuntimeUninstall_Enable(tx):BootStore_BeginUpdate(tx);if(!r){transaction=tx;g_runtime_update.enabled_transaction=tx;}return Result(r);}
static uint32_t Erase(void *p,uint32_t a)
{if(!Context(p)||!Authorized()||!transaction||transaction!=service->transaction||!Range(a,4096)||(a&4095))return Result(RUNTIME_UPDATE_LOCKED);return Result(Uninstall()?BSP_NOR_OTAErase4K(a):BootStore_Erase(a-UPDATE_STAGE_BASE));}
static uint32_t Resume(void *p,uint32_t version,uint32_t crc,const uint8_t sha[32],uint32_t *offset)
{if(!Context(p)||!Authorized()||transaction!=service->transaction)return RUNTIME_UPDATE_LOCKED;
 return Result(BootStore_Resume(version,crc,sha,offset));}
static uint32_t Checkpoint(void *p,uint32_t offset)
{if(!Context(p)||!Authorized()||transaction!=service->transaction)return RUNTIME_UPDATE_LOCKED;
 return Result(BootStore_Checkpoint(offset));}
static uint32_t Program(void *p,uint32_t a,const void *data,uint32_t n)
{if(!Context(p)||!Authorized()||!transaction||transaction!=service->transaction||!Range(a,n)||!data)return Result(RUNTIME_UPDATE_LOCKED);return Result(Uninstall()?RuntimeUninstall_Program(a,data,n):BootStore_Program(a-UPDATE_STAGE_BASE,data,n));}
static void Disable(void *p)
{if(!Ready()||p!=service)return;
 /* FINISH closes incoming DATA through service state, preserving the verified
  * destination for explicit COMMIT. Abort/failure releases the write lease. */
 if(service->state!=UPDATE_VERIFIED){BSP_NOR_OTADisable();BootStore_EndUpdate();transaction=0;g_runtime_update.enabled_transaction=0;}}
static uint32_t MetadataRead(void *p,uint32_t words[5])
{if(!Context(p)||!words)return Result(RUNTIME_UPDATE_ARGUMENT);return Result(UpdateMetadata_Read(words));}
static uint32_t Commit(void *p,uint32_t version,uint32_t crc,uint32_t arm)
{
 if(Uninstall()){
  static const uint8_t approved[32]=UNINSTALL_IMAGE_SHA;
  if(!Context(p)||!Authorized()||arm!=UPDATE_COMMIT_ARM||service->state!=UPDATE_VERIFIED||
     version!=UNINSTALL_TRANSPORT_VERSION||crc!=service->expected_crc||service->transaction!=transaction||
     service->received!=UPDATE_APP_BYTES||service->verified!=UPDATE_APP_BYTES||memcmp(service->actual_sha,approved,32)||
     !RecoveryTarget_Verify((const uint8_t*)0x08000000))return RUNTIME_UPDATE_LOCKED;
  return RuntimeUninstall_Commit(version,crc);
 }
 if(!Context(p)||!Authorized()||arm!=UPDATE_COMMIT_ARM||(service->target!=UPDATE_TARGET_GATE_CFW&&!Diagnostic())||service->state!=UPDATE_VERIFIED||
    service->received!=UPDATE_GATE_BYTES||service->verified!=UPDATE_GATE_BYTES||service->transaction!=transaction||
    version!=service->version||crc!=service->expected_crc||memcmp(service->actual_sha,service->expected_sha,32)||
    !requirement_valid||requirement_checked!=transaction||(requirement.required&&!ResourceStore_Compatible(requirement.sha256)))return Result(RUNTIME_UPDATE_LOCKED);
 ++g_runtime_update.commit_calls;
 return Result((Diagnostic()?BootStore_CommitDiagnostic:BootStore_Commit)(version,service->actual_sha,(const uint8_t*)&requirement));
}
static uint32_t Committed(void *p,const uint8_t sha[32])
{return Context(p)&&BootStore_Committed(sha);}
static void Reset(void *p)
{
 if(!Context(p)||service->state!=UPDATE_COMMITTED||(!Uninstall()&&!BootStore_Committed(service->expected_sha))||
    !__atomic_load_n(&service->sent_valid,__ATOMIC_ACQUIRE)||
    __atomic_load_n(&service->sent_sequence,__ATOMIC_ACQUIRE)!=service->reset_sequence||
    process_time-__atomic_load_n(&service->sent_ms,__ATOMIC_ACQUIRE)<1500U)return;
 if(Uninstall()){
  uint32_t m[5];if(UpdateMetadata_Read(m)||m[1]!=UNINSTALL_TRANSPORT_VERSION||m[2]!=0x7f90||m[3]!=UPDATE_APP_BYTES||m[4]!=service->expected_crc)return;
  __DSB();NVIC_SystemReset();return;
 }
 GateRetained r;memcpy(&r,(const void*)&g_recovery_mailbox.request,sizeof(r));GateRetained_Request(&r,GATE_REASON_INSTALL);
 memcpy((void*)&g_recovery_mailbox.request,&r,sizeof(r));__DSB();++g_runtime_update.reset_calls;NVIC_SystemReset();
}
static uint32_t Untouched(void *p){return p==service&&Uninstall()&&RuntimeUninstall_Untouched();}
uint32_t RuntimeUpdate_Init(void)
{
 if(Ready())return 0;
 if(!ContextOkay())return RUNTIME_UPDATE_CONTEXT;
 if(!HardwareReady())return RUNTIME_UPDATE_NOT_READY;
 /* Reject bad allocator output before any memset or public pointer exists.
  * Subtraction bounds avoid address+size wrap and exclude SDRAM test scratch. */
 void *arena=BSP_RAM_Allocate(RU_SERVICE_BYTES);
 if(!arena)return RUNTIME_UPDATE_NO_MEMORY;
 uintptr_t address=(uintptr_t)arena;
 if((address&(RU_ALIGNMENT-1U))||address<BSP_RAM_BASE+BSP_RAM_TEST_BYTES||
    address>BSP_RAM_BASE+g_bsp_ram.capacity_bytes-RU_SERVICE_BYTES)return RUNTIME_UPDATE_BAD_MEMORY;
 service=arena;
 UpdatePlatform p={0};p.context=service;p.read=Read;p.enable=Enable;p.erase4k=Erase;p.program=Program;p.disable=Disable;
 p.commit_untouched=Untouched;p.metadata_read=MetadataRead;p.metadata_commit=Commit;p.reset=Reset;p.gate_committed=Committed;p.resume=Resume;p.checkpoint=Checkpoint;UpdateService_Init(service,&p);
 g_runtime_update.service_address=(uint32_t)(uintptr_t)service;g_runtime_update.service_bytes=sizeof(*service);
 g_runtime_update.ready=1;__atomic_store_n(&published,1U,__ATOMIC_RELEASE);return 0;
}
UpdateService *RuntimeUpdate_GetService(void){return Ready()?service:NULL;}
void RuntimeUpdate_Process(uint32_t now)
{
 if(!Ready())return;
 if(!Context(service)){g_runtime_update.result=RUNTIME_UPDATE_CONTEXT;return;}
 if(service->state==UPDATE_RECEIVING||service->state==UPDATE_IDLE){requirement_checked=0;requirement_valid=0;}
 if(service->state==UPDATE_VERIFIED&&!Uninstall()){
  if(requirement_checked!=service->transaction){requirement_valid=!Read(service,UPDATE_STAGE_BASE+0x200,&requirement,sizeof(requirement))&&requirement.magic==0x51534352U&&requirement.version==1&&requirement.required==(Diagnostic()?0U:1U);if(requirement_valid&&Diagnostic()){for(uint32_t i=0;i<32;i++)if(requirement.sha256[i])requirement_valid=0;}requirement_checked=service->transaction;}
  if(requirement_valid&&requirement.required)(void)ResourceStore_Compatible(requirement.sha256);
 }
 process_time=now;++g_runtime_update.polls;
 if(ResourceStore_TransferActive()&&!InstallSession_Active(&service->install))InstallSession_Begin(&service->install,now,now);
 if(InstallSession_Active(&service->install)){
  BSP_Buttons_State button;BSP_Buttons_GetState(BSP_BUTTON_ENTER,&button);
  InstallSession_Button(&service->install,now,button.raw_pressed);
  if(service->install.cancel_requested&&ResourceStore_TransferCancel()){
   uint32_t r=UpdateService_CancelUncommitted(service);
   InstallSession_End(&service->install,r?INSTALL_UNKNOWN:INSTALL_CANCELLED);}
 }
 UpdateService_Process(service,now);
 if(InstallSession_Active(&service->install)){
  uint32_t w[23]={0};(void)ResourceStore_Transfer(0x8d,0,0,(uint8_t*)w);
  if(ResourceStore_TransferActive()||(w[1]==5&&service->state==UPDATE_IDLE)){
   InstallSession_Observe(&service->install,now,w[1]==5?INSTALL_ERROR:INSTALL_TRANSFER,200+w[1],0,1,
     w[1]==3?w[6]:w[3],w[1]==3?524288:w[4],w[1]==3?w[6]:0,w[5],service->connected);
  }else{
  uint32_t state=service->commit_uncertain?INSTALL_UNKNOWN:service->state==UPDATE_FAILED?INSTALL_ERROR:
   service->state==UPDATE_RESET_WAIT?INSTALL_RESTART:service->state==UPDATE_COMMITTED?INSTALL_COMMIT:
   service->state==UPDATE_VERIFIED?INSTALL_CONFIRM:INSTALL_TRANSFER;
  InstallSession_Observe(&service->install,now,state,service->state,0,1,
   service->state==UPDATE_VERIFYING?service->verified:service->received,UpdateService_ImageBytes(service),service->verified,
   service->result,service->connected);
  }
 }
 g_runtime_update.state=service->state;g_runtime_update.service_result=service->result;
 g_runtime_update.connected=__atomic_load_n(&service->connected,__ATOMIC_ACQUIRE);g_runtime_update.authorized=__atomic_load_n(&service->authorization,__ATOMIC_ACQUIRE);
 g_runtime_update.received_bytes=service->received;g_runtime_update.verified_bytes=service->verified;
}
#endif
