#if !NOODOE_PRODUCT
#include "App_Recovery.h"
#include "Recovery_Buttons.h"
#include "Recovery_Store.h"
#include "Update_Metadata.h"
#include "BSP_NOR.h"
#include "BSP_Watchdog.h"
#include "BSP_Display.h"
#include "bsp_eve_bus.h"
#include "dma.h"
#include "spi.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>

/* The already-reserved RTOS heap is a pre-kernel workspace, not extra SRAM.
 * No RTOS object/allocation may exist when EarlyRun is called. On normal
 * fallthrough it is zeroed and subsequently owned exclusively by heap_4. */
#if configAPPLICATION_ALLOCATED_HEAP == 1
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((aligned(8)));
#endif
typedef struct {uint32_t magic,reason,inverse,check;} RecoveryIntent;
static volatile RecoveryIntent intent __attribute__((section(".noinit.app_recovery"),aligned(8)));
volatile AppRecoveryStatus g_app_recovery;
static uint32_t reply_sequence,reply_sent_ms,reply_sent,request_ms;
static uint32_t local_request,buttons_initialized;
static volatile uint32_t recovery_active;
static RecoveryButtons runtime_buttons;
static UpdateSha256 identity_hash;
static uint8_t identity_digests[64];
static uint32_t identity_phase,identity_offset;
static volatile uint32_t identity_ready;
static uint32_t RuntimeState(void){return __atomic_load_n(&g_app_recovery.state,__ATOMIC_ACQUIRE);}
static void RuntimeStateSet(uint32_t state){__atomic_store_n(&g_app_recovery.state,state,__ATOMIC_RELEASE);}
#define INTENT_MAGIC 0x32564352U
#define INTENT_WAIT 1U
#define INTENT_CONFIRMED 2U
#define INTENT_FAULT 3U

/* Retained intent has an inverse/check and is consumed once. SRAM retention
 * does not promise survival through complete power removal or broken startup. */
static void IntentSet(uint32_t reason)
{intent.magic=0;intent.reason=reason;intent.inverse=~reason;intent.check=reason^0xD615A73BU;__DSB();intent.magic=INTENT_MAGIC;__DSB();}
static uint32_t IntentPeek(void)
{uint32_t r=intent.reason;if(intent.magic!=INTENT_MAGIC||intent.inverse!=~r||intent.check!=(r^0xD615A73BU)||r<1||r>3)r=0;return r;}
uint32_t AppRecovery_IsFaultBoot(void){return IntentPeek()==INTENT_FAULT;}
#if configAPPLICATION_ALLOCATED_HEAP == 1 && !NOODOE_BOOTSTRAP
static uint32_t IntentTake(void)
{uint32_t r=IntentPeek();intent.magic=0;__DSB();return r;}
#endif
void AppRecovery_FaultReset(uint32_t code)
{(void)code;IntentSet(INTENT_FAULT);NVIC_SystemReset();}
void AppRecovery_RequestLocal(void){IntentSet(INTENT_WAIT);NVIC_SystemReset();}
void AppRecovery_MarkEarlyActive(void){recovery_active=1U;}
void AppRecovery_RequestConfirmed(void){IntentSet(INTENT_CONFIRMED);NVIC_SystemReset();}
uint32_t AppRecovery_ConsumeIntent(void){uint32_t r=IntentPeek();intent.magic=0;__DSB();return r;}
__attribute__((weak)) const RecoverySource *AppRecovery_EmbeddedSource(void){return NULL;}
/* BSP keeps a weak terminal hook; the application owns reset policy. */
#if NOODOE_PRODUCT || NOODOE_BOOTSTRAP
void BSP_FaultRecoveryRequested(uint32_t code)
{/* A fault in minimal recovery itself returns to the BSP terminal/SWD loop.
   * Rebooting the same failing rescue code forever would destroy recovery. */
 if(!recovery_active)AppRecovery_FaultReset(code);
}
#endif

/* Poll only known DOWN=PI6 and ENTER=PA15. UP is deliberately irrelevant on
 * the donor with damaged PD12. No broad GPIO reset or power-hold writes. */
static void ButtonsInit(void)
{GPIO_InitTypeDef g={0};__HAL_RCC_GPIOI_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE();
 g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
 g.Pin=GPIO_PIN_6;HAL_GPIO_Init(GPIOI,&g);g.Pin=GPIO_PIN_15;HAL_GPIO_Init(GPIOA,&g);}
static uint32_t Buttons(void)
{return ((GPIOI->IDR&GPIO_PIN_6)?0U:1U)|((GPIOA->IDR&GPIO_PIN_15)?0U:2U);}
/* Minimal recovery is a separate phase owner before the kernel starts. A
 * finite operation must return to this loop; drivers cannot feed on its behalf. */
static void FeedWatchdog(void)
{(void)BSP_Watchdog_Checkpoint(HAL_GetTick(),g_bsp_watchdog.progress+1U);}

/* Independent8bit polling reads avoid IRQ/DMA/SDRAM dependencies. The public
 * BSP still enforces NOR bounds; raw physical order is adapted only once. */
static uint32_t RawRead(void *unused,uint32_t address,void *dst,uint32_t bytes)
{(void)unused;uint8_t *p=dst;while(bytes){uint32_t n=bytes>256?256:bytes;
 uint32_t r=BSP_NOR_ReadPolling(address,p,n);if(r)return r;address+=n;p+=n;bytes-=n;FeedWatchdog();}return 0;}
static uint32_t StageRead(void *unused,uint32_t offset,void *dst,uint32_t bytes)
{if((offset&1)||(bytes&1)||offset>RECOVERY_STOCK_BYTES||bytes>RECOVERY_STOCK_BYTES-offset)return 1;
 uint32_t r=RawRead(unused,UPDATE_STAGE_BASE+offset,dst,bytes);if(r)return r;uint8_t *p=dst;
 for(uint32_t i=0;i<bytes;i+=2){uint8_t b=p[i];p[i]=p[i+1];p[i+1]=b;}return 0;}
static uint32_t Enable(void *unused){(void)unused;return BSP_NOR_OTAEnable(RECOVERY_CONFIRM_TOKEN);}
static uint32_t Erase(void *unused,uint32_t offset)
{(void)unused;if((offset&4095)||offset>RECOVERY_STOCK_BYTES-4096)return 1;return BSP_NOR_OTAErase4K(UPDATE_STAGE_BASE+offset);}
static uint32_t Program(void *unused,uint32_t offset,const void *src,uint32_t bytes)
{(void)unused;uint8_t wire[256];const uint8_t *p=src;
 if(!p||bytes!=256||(offset&255)||offset>RECOVERY_STOCK_BYTES-bytes)return 1;
 for(uint32_t i=0;i<bytes;i+=2){wire[i]=p[i+1];wire[i+1]=p[i];}
 return BSP_NOR_OTAProgram(UPDATE_STAGE_BASE+offset,wire,bytes);}
static void Disable(void *unused){(void)unused;BSP_NOR_OTADisable();}
static uint32_t MetaRead(void *unused,uint32_t words[5]){(void)unused;return UpdateMetadata_Read(words);}
static uint32_t MetaCommit(void *scratch,uint32_t version,uint32_t crc)
{return UpdateMetadata_Commit(version,crc,UPDATE_METADATA_ARM_TOKEN,scratch,UPDATE_METADATA_SECTOR_SIZE);}
static void Reset(void *unused){(void)unused;NVIC_SystemReset();}

/* Recovery uses EVE ROM text only. It has no font package, LVGL allocation,
 * texture or SDRAM access. Display failure does not grant install approval. */
static void Word(uint8_t *p,uint32_t *n,uint32_t v){for(uint32_t i=0;i<4;++i)p[(*n)++]=(uint8_t)(v>>(8*i));}
static void Text(uint8_t *p,uint32_t *n,uint32_t y,const char *s)
{Word(p,n,0xFFFFFF0CU);Word(p,n,240U|(y<<16));Word(p,n,28U|(1536U<<16));
 do{p[(*n)++]=(uint8_t)*s;}while(*s++);while(*n&3)p[(*n)++]=0;}
static void Draw(uint32_t state,uint32_t progress,uint32_t error)
{
 uint8_t reg[2],data[384];uint32_t n=0;char line[56];
 if(!g_bsp_eve.ready||BSP_EVE_BusRead(0x302574,reg,2)||((uint32_t)reg[0]|((uint32_t)reg[1]<<8))!=0xFFCU)return;
 if(BSP_EVE_BusRead(0x302054,reg,1)||reg[0])return;
 Word(data,&n,0xFFFFFF00);Word(data,&n,0x02000000);Word(data,&n,0x26000007);
 Word(data,&n,0x04FFFFFF);Word(data,&n,0xFFFFFF3F);Word(data,&n,28);Word(data,&n,28);
 Text(data,&n,145,"STOCK RECOVERY");
 if(state==APP_RECOVERY_WAIT_RELEASE)Text(data,&n,215,"Release all buttons");
 else if(state==APP_RECOVERY_WAIT_CONFIRM){Text(data,&n,207,"Restore original V5.16?");Text(data,&n,250,"Hold ENTER, then release");Text(data,&n,293,"DOWN to cancel");}
 else if(state==APP_RECOVERY_FAILED){Word(data,&n,0x04FF4040);Text(data,&n,205,"Recovery stopped");Word(data,&n,0x04FFFFFF);
  snprintf(line,sizeof(line),"Error %lu",(unsigned long)error);Text(data,&n,250,line);Text(data,&n,294,"No automatic retry");}
 else{Text(data,&n,212,state==APP_RECOVERY_VALIDATING?"Checking original image":"Restoring original image");
  snprintf(line,sizeof(line),"%lu / 448 KiB",(unsigned long)(progress/1024U));Text(data,&n,258,line);Text(data,&n,305,"Keep main power connected");}
 Word(data,&n,0);Word(data,&n,0xFFFFFF01);const uint8_t prefix[3]={0xB0,0x25,0x78};
 if(!BSP_EVE_BusSelect()){uint32_t r=BSP_EVE_BusSend(prefix,3);if(!r)r=BSP_EVE_BusSend(data,n);
  uint32_t end=BSP_EVE_BusDeselect();g_app_recovery.display_error=r?r:end;}
}

void AppRecovery_Run(const RecoverySource *embedded,void *workspace,uint32_t bytes,uint32_t force_wait)
{
 RecoveryButtons buttons;uint32_t start=HAL_GetTick();ButtonsInit();
 if(!g_bsp_watchdog.initialized){(void)BSP_Watchdog_StartEarly();(void)BSP_Watchdog_Init(start,WATCHDOG_WAIT,0);}
 else (void)BSP_Watchdog_SetPhase(start,WATCHDOG_WAIT,0);
 RecoveryButtons_Init(&buttons,force_wait!=0,start);
 if(!force_wait){
  if(Buttons()!=3)return;
  do{uint32_t now=HAL_GetTick();RecoveryButtons_Process(&buttons,Buttons(),now);FeedWatchdog();
   if(buttons.state==1)break;
   if(Buttons()!=3)return;
   HAL_Delay(10);
  }while(HAL_GetTick()-start<3400U);
  if(buttons.state!=1)return;
 }
 recovery_active=1U;__DMB();
 uintptr_t a=(uintptr_t)workspace;
 uint32_t store_bytes=(sizeof(RecoveryStore)+7U)&~7U,core_bytes=(sizeof(RecoveryCore)+7U)&~7U;
 if(!workspace||(a&7U)||bytes<store_bytes+core_bytes+UPDATE_METADATA_SECTOR_SIZE){g_app_recovery.result=RECOVERY_ARGUMENT;recovery_active=0U;return;}
 memset(workspace,0,bytes);RecoveryStore *store=(RecoveryStore*)workspace;
 RecoveryCore *core=(RecoveryCore*)(a+store_bytes);void *scratch=(void*)(a+store_bytes+core_bytes);
 RecoveryPlatform platform={scratch,StageRead,Enable,Erase,Program,Disable,MetaRead,MetaCommit,Reset};
 RecoveryCore_Init(core,&platform);RecoverySource source=embedded?*embedded:(RecoverySource){store,RecoveryStore_Read};
 g_app_recovery.state=APP_RECOVERY_WAIT_RELEASE;g_app_recovery.error=0;
 uint32_t display=BSP_Display_Init();g_app_recovery.display_error=display;
 if(!display)(void)BSP_Display_SetBrightnessPercent(25);
 uint32_t validated=0,started=0,last_draw=UINT32_MAX,confirmed=force_wait==2U;
 for(;;){uint32_t now=HAL_GetTick();FeedWatchdog();
  if(!confirmed){RecoveryButtons_Process(&buttons,Buttons(),now);
   g_app_recovery.state=buttons.state==1?APP_RECOVERY_WAIT_RELEASE:APP_RECOVERY_WAIT_CONFIRM;
   if(buttons.state==4){BSP_Display_Shutdown();memset(workspace,0,bytes);g_app_recovery.state=APP_RECOVERY_NORMAL;recovery_active=0U;return;}
   if(buttons.state==3)confirmed=1;
  }
  if(confirmed&&!validated){
   if(g_bsp_watchdog.phase==WATCHDOG_WAIT)(void)BSP_Watchdog_SetPhase(now,WATCHDOG_RECOVERY,600000U);
   g_app_recovery.state=APP_RECOVERY_VALIDATING;
   if(!started){
    if(!RecoveryTarget_Verify((const uint8_t*)0x08000000U))g_app_recovery.error=RECOVERY_RESIDENT;
    else{MX_DMA_Init();MX_SPI5_Init();g_app_recovery.error=BSP_NOR_Init();}started=1;
    if(!g_app_recovery.error&&!embedded){uint32_t uid[3]={HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};RecoveryStore_Init(store,RawRead,NULL,uid);store->resident=(const uint8_t*)0x08000000U;}}
   if(!g_app_recovery.error){
    if(!embedded){RecoveryStore_Process(store);if(store->state==RECOVERY_STORE_READY)validated=1;
     else if(store->state==RECOVERY_STORE_FAILED||store->state==RECOVERY_STORE_MISSING)g_app_recovery.error=0x100U+store->error;}
    else validated=1;
    if(validated){g_app_recovery.source_ready=1;g_app_recovery.error=RecoveryCore_Request(core,&source);}
   }
  }else if(confirmed&&!g_app_recovery.error){
   RecoveryCore_Process(core);g_app_recovery.verified=core->offset;
   g_app_recovery.state=core->state==RECOVERY_SOURCE_HASH?APP_RECOVERY_VALIDATING:APP_RECOVERY_INSTALLING;
   if(core->state==RECOVERY_ERROR)g_app_recovery.error=0x200U+core->result;
   else if(core->state==RECOVERY_VERIFIED){g_app_recovery.error=RecoveryCore_Commit(core,RECOVERY_CONFIRM_TOKEN);
    if(!g_app_recovery.error)g_app_recovery.error=RecoveryCore_Reset(core,RECOVERY_CONFIRM_TOKEN);}
  }
  if(g_app_recovery.error)g_app_recovery.state=APP_RECOVERY_FAILED;
  if(now-last_draw>=150U){Draw(g_app_recovery.state,g_app_recovery.verified,g_app_recovery.error);last_draw=now;}
  HAL_Delay(1);
 }
}

void AppRecovery_EarlyRun(void)
{
#if configAPPLICATION_ALLOCATED_HEAP == 1 && !NOODOE_BOOTSTRAP
 uint32_t reason=IntentTake();g_app_recovery.reason=reason;
 AppRecovery_Run(AppRecovery_EmbeddedSource(),ucHeap,sizeof(ucHeap),reason==INTENT_CONFIRMED?2U:reason?1U:0U);
 if(g_bsp_watchdog.initialized)(void)BSP_Watchdog_SetPhase(HAL_GetTick(),WATCHDOG_BOOT,30000U);
#endif
}

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
  IntentSet(local_request?INTENT_WAIT:INTENT_CONFIRMED);NVIC_SystemReset();}
}
void AppRecovery_RuntimeButtons(uint32_t now)
{
 if(local_request)return;
 if(!buttons_initialized){RecoveryButtons_Init(&runtime_buttons,0,now);buttons_initialized=1;}
 RecoveryButtons_Process(&runtime_buttons,Buttons(),now);
 if(runtime_buttons.state==1){local_request=1;reply_sent_ms=now;
  __atomic_store_n(&reply_sent,1U,__ATOMIC_RELEASE);RuntimeStateSet(APP_RECOVERY_RESET_WAIT);}
}

/* Only the storage/bootstrap owner advances this read-only computation. The
 * release word publishes both finished hashes to the independent I/O owner. */
void AppRecovery_IdentityProcess(void)
{
 if(__atomic_load_n(&identity_ready,__ATOMIC_ACQUIRE))return;
 if(!identity_offset)UpdateSha256_Init(&identity_hash);
 uint32_t limit=identity_phase?0x8000U:RECOVERY_STOCK_BYTES;
 uint32_t base=identity_phase?0x08000000U:UPDATE_APP_BASE;
 UpdateSha256_Feed(&identity_hash,(const uint8_t*)(uintptr_t)(base+identity_offset),4096);
 identity_offset+=4096;
 if(identity_offset==limit){UpdateSha256_Final(&identity_hash,identity_digests+32U*identity_phase);
  identity_offset=0;if(identity_phase)__atomic_store_n(&identity_ready,1U,__ATOMIC_RELEASE);else identity_phase=1;}
}
uint32_t AppRecovery_Identity(uint8_t out[84],uint32_t role)
{
 if(!out||!__atomic_load_n(&identity_ready,__ATOMIC_ACQUIRE))return 0;
 uint32_t fields[5]={1,role,HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};
 for(uint32_t j=0;j<5;++j)for(uint32_t i=0;i<4;++i)out[4*j+i]=(uint8_t)(fields[j]>>(8*i));
 memcpy(out+20,identity_digests,64);return 1;
}

#endif /* legacy Bootstrap/bring-up recovery */
