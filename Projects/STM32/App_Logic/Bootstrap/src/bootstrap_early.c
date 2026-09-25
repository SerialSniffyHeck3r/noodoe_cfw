#include "App_Recovery.h"
#include "Bootstrap_Image.h"
#include "Bootstrap_Confirm.h"
#include "gate_board.h"
#include "gate_policy.h"
#include "Update_Metadata.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx.h"

/* The reset flags are captured before C initialization and before any HAL code
 * clears RCC_CSR. A warm reset must not silently retry a hung BT startup. */
static volatile uint32_t reset_flags __attribute__((section(".noinit.bootstrap_reset")));
static uint32_t early_active,progress;
static RecoveryCore core;
static uint32_t metadata_scratch[4096];
extern uint32_t HAL_GetTick(void);
void BSP_BootSafetyStart(void)
{reset_flags=RCC->CSR;(void)BSP_Watchdog_StartEarly();}
uint32_t Bootstrap_MetadataMillis(void)
{return early_active?GateBoard_Millis():HAL_GetTick();}
static uint32_t Source(void *ctx,uint32_t off,void *dst,uint32_t n)
{return BootstrapImage_Read(ctx,off,dst,n)?1U:0U;}
static uint32_t Read(void *ctx,uint32_t off,void *dst,uint32_t n)
{if((off|n)&1U||off>=0x70000U||n>0x70000U-off||GateBoard_RawRead(ctx,0x7f90000U+off,dst,n))return 1;
 uint8_t *p=dst;for(uint32_t i=0;i<n;i+=2){uint8_t x=p[i];p[i]=p[i+1];p[i+1]=x;}return 0;}
static uint32_t Enable(void *ctx){(void)ctx;return GateBoard_SetWriteRange(0x7f90000,0x70000);}
static uint32_t Erase(void *ctx,uint32_t off){(void)ctx;return off>=0x70000?1:GateBoard_NorErase(0x7f90000+off);}
static uint32_t Program(void *ctx,uint32_t off,const void *data,uint32_t n)
{(void)ctx;uint8_t pair[256];if(n!=256||off>0x70000-n)return 1;
 const uint8_t *p=data;for(uint32_t i=0;i<n;i++)pair[i]=p[i^1U];return GateBoard_NorProgram(0x7f90000+off,pair,n);}
static void Lock(void *ctx){(void)ctx;GateBoard_LockNor();}
static uint32_t MetaRead(void *ctx,uint32_t *out){(void)ctx;return UpdateMetadata_Read(out);}
static uint32_t Commit(void *ctx,uint32_t version,uint32_t crc)
{(void)ctx;return UpdateMetadata_Commit(version,crc,UPDATE_METADATA_ARM_TOKEN,metadata_scratch,sizeof(metadata_scratch));}
static void Reset(void *ctx){(void)ctx;GateBoard_Reset();}
/* No BT, FAT, SDRAM, heap, scheduler or external font can block this loop.
 * Flash is not touched until the embedded full image and resident match. */
static uint32_t Restore(void)
{
 if(!RecoveryTarget_Verify((const uint8_t*)0x08000000))return 40;
 if(GateBoard_NorInit())return 41;
 RecoveryPlatform io={0,Read,Enable,Erase,Program,Lock,MetaRead,Commit,Reset};
 RecoverySource source={0,Source};RecoveryCore_Init(&core,&io);BootstrapImage_Reset();
 uint32_t result=RecoveryCore_Request(&core,&source);if(result)return 50+result;
 uint32_t last=0;
 while(core.state!=RECOVERY_VERIFIED&&core.state!=RECOVERY_ERROR){
  RecoveryCore_Process(&core);uint32_t now=GateBoard_Millis();
  if(!BSP_Watchdog_Checkpoint(now,++progress))for(;;){}
  if(now-last>=100){last=now;GateBoard_Display(5,core.offset*100/RECOVERY_STOCK_BYTES,0);}
 }
 if(core.state==RECOVERY_ERROR)return 60+core.result;
 result=RecoveryCore_Commit(&core,RECOVERY_CONFIRM_TOKEN);
 return result?80+result:RecoveryCore_Reset(&core,RECOVERY_CONFIRM_TOKEN);
}
void BSP_ApplicationEarly(void)
{
 uint32_t reason=AppRecovery_ConsumeIntent(),reset=reset_flags;
 RCC->CSR|=RCC_CSR_RMVF;
 RCC->AHB1ENR|=RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIOGEN;
 (void)RCC->AHB1ENR;
 GPIOA->MODER&=~(3U<<30);GPIOA->PUPDR&=~(3U<<30);
 /* Held ENTER at boot requests WAIT, never infers an unobserved IGN edge. */
 if(!reason&&!(reset&(RCC_CSR_IWDGRSTF|RCC_CSR_WWDGRSTF))&&(GPIOA->IDR&(1U<<15)))return;
 early_active=1;AppRecovery_MarkEarlyActive();uint32_t error=GateBoard_Init();
 if(error)for(;;){} /* IWDG, not an unbounded driver feeding loop. */
 (void)BSP_Watchdog_Init(GateBoard_Millis(),WATCHDOG_WAIT,0);
 (void)GateBoard_DisplayInit();
 BootstrapConfirm confirmation;BootstrapConfirm_Init(&confirmation);
 uint32_t confirmed=reason==2U,last=0;
 for(;;){
  uint32_t now=GateBoard_Millis();
  if(!BSP_Watchdog_Checkpoint(now,++progress))for(;;){}
  if(confirmed||BootstrapConfirm_Process(&confirmation,now,GateBoard_Enter())){
   (void)BSP_Watchdog_SetPhase(now,WATCHDOG_RECOVERY,600000U);
   error=Restore();confirmed=0;GateBoard_LockNor();
   (void)BSP_Watchdog_SetPhase(GateBoard_Millis(),WATCHDOG_WAIT,0);
   BootstrapConfirm_Init(&confirmation);
  }
  if(now-last>=100){last=now;GateBoard_Display(7,confirmation.elapsed/20U,error);}
 }
}
