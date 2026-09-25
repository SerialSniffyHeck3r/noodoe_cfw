#if NOODOE_PRODUCT
#include "RoutineUpdate.h"
#include "Recovery_Store.h"
#include "BootStore.h"
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "Resources.h"
#include "Recovery_Core.h"
#include "stm32f4xx_hal.h"
#include <string.h>
static RecoveryStore *audit;
static volatile uint32_t requested,state,error,epoch;
/* 0 idle,1 queued,2 reading,3 ready,4 failed. A later request invalidates the
 * old proof; no retained 'backed up once' boolean can authorize a new layout. */
uint32_t RoutineUpdate_Request(uint32_t connection)
{if(!connection||state==1||state==2)return 0;
 epoch=connection;error=0;state=1;__atomic_store_n(&requested,1,__ATOMIC_RELEASE);return 1;}
uint32_t RoutineUpdate_Status(uint32_t connection,uint32_t *e)
{uint32_t s=__atomic_load_n(&state,__ATOMIC_ACQUIRE);if(!e||connection!=epoch)return 0;*e=error;return s;}
static uint32_t Read(void *p,uint32_t address,void *data,uint32_t bytes)
{(void)p;return BSP_NOR_Read(address,data,bytes);}
void RoutineUpdate_Process(void)
{
 if(__atomic_exchange_n(&requested,0,__ATOMIC_ACQ_REL)){
  GateJournalRecord j;
  if(!BootStore_GetBootInfo(&j)||!g_boot_store.confirmed||j.flags||j.state!=GATE_J_CONFIRMED||
     Resources_GetStatus()!=RESOURCES_READY||!RecoveryTarget_Verify((const uint8_t*)0x08000000U)){
   error=1;__atomic_store_n(&state,4,__ATOMIC_RELEASE);return;
  }
  if(!audit)audit=BSP_RAM_AllocateNamed(BSP_RAM_ROUTINE_AUDIT,sizeof(*audit));
  if(!audit){error=2;__atomic_store_n(&state,4,__ATOMIC_RELEASE);return;}
  uint32_t uid[3]={HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};RecoveryStore_Init(audit,Read,0,uid);audit->resident=(const uint8_t*)0x08000000U;
  __atomic_store_n(&state,2,__ATOMIC_RELEASE);
 }
 if(state!=2)return;
 uint32_t s=RecoveryStore_Process(audit);
 if(s==RECOVERY_STORE_READY)__atomic_store_n(&state,3,__ATOMIC_RELEASE);
 else if(s==RECOVERY_STORE_FAILED||s==RECOVERY_STORE_MISSING){error=0x100U+audit->error;__atomic_store_n(&state,4,__ATOMIC_RELEASE);}
}
#endif
