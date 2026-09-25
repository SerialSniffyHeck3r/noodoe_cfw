#if NOODOE_PRODUCT
#include "DeviceLog.h"
#include "Cfw_Files.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include "gate_abi.h"
#include <string.h>
/* A small bounded SRAM mailbox accepts events before SDRAM is ready. The NOR
 * state machine and sector bounce live in one named SDRAM allocation. */
static DeviceEvent queue[16];
static uint32_t count,last_flush;
typedef struct {EventLog log;DeviceEvent batch[16];} DeviceLogArena;
static DeviceLogArena *arena;
static EventLog *store;
static uint32_t boot_posted,fault_pending,fault_after;
static struct {uint32_t token,sequence,offset,result;uint8_t bytes[256];} reader;
static volatile uint32_t read_pending,read_complete;
volatile DeviceLogStatus g_device_log;
uint32_t DeviceLog_RequestRead(uint32_t token,uint32_t sequence,uint32_t offset)
{if(!token||(offset&255)||offset>EVENT_LOG_BYTES-256)return CFW_ARGUMENT;
 if(__atomic_load_n(&read_pending,__ATOMIC_ACQUIRE))return CFW_BUSY;
 reader.token=token;reader.sequence=sequence;reader.offset=offset;reader.result=CFW_PENDING;
 __atomic_store_n(&read_complete,0,__ATOMIC_RELEASE);__atomic_store_n(&read_pending,1,__ATOMIC_RELEASE);return 0;}
uint32_t DeviceLog_ReadResult(uint32_t token,uint8_t out[256])
{if(!token||reader.token!=token||!out)return CFW_ARGUMENT;
 if(!__atomic_load_n(&read_complete,__ATOMIC_ACQUIRE))return CFW_PENDING;
 if(!reader.result)memcpy(out,reader.bytes,256);
 return reader.result;}
uint32_t DeviceLog_Post(const DeviceEvent *e)
{if(!e||!e->code||__get_IPSR())return 0;
 uint32_t irq=__get_PRIMASK();__disable_irq();
 if(count){DeviceEvent *p=&queue[count-1];
  if(p->code==e->code&&p->detail==e->detail&&p->transaction==e->transaction&&p->boot==e->boot&&!memcmp(p->data,e->data,32)){
   if(p->count!=UINT32_MAX)p->count++;
   p->last_ms=e->time_ms;g_device_log.coalesced++;__set_PRIMASK(irq);return 1;}}
 if(count==16){g_device_log.dropped++;__set_PRIMASK(irq);return 0;}
 queue[count]=*e;queue[count].count=1;queue[count].first_ms=queue[count].last_ms=e->time_ms;
 g_device_log.queued=++count;__set_PRIMASK(irq);return 1;}
static uint32_t Read(void *c,uint32_t o,void *p,uint32_t n){(void)c;return CfwFiles_Read(CFW_LOG,o,p,n);}
static uint32_t Grant(void *c){(void)c;return CfwFiles_Grant(CFW_LOG);}
static uint32_t Erase(void *c,uint32_t o){(void)c;return CfwFiles_Erase(CFW_LOG,o);}
static uint32_t Program(void *c,uint32_t o,const void *p,uint32_t n){(void)c;return CfwFiles_Program(CFW_LOG,o,p,n);}
uint32_t DeviceLog_Busy(void){return read_pending||(store&&store->phase!=LOG_ERROR&&(store->phase!=LOG_READY||count));}
void DeviceLog_Process(uint32_t now,uint32_t drain)
{if(!store){uint32_t status=CfwFiles_Status();if(status!=CFW_OK){
   if(status==CFW_PENDING)return;
   g_device_log.error=status;goto unavailable;}
  if(!CfwFiles_Present(CFW_LOG)){g_device_log.error=CFW_MISSING;goto unavailable;}
  arena=BSP_RAM_AllocateNamed(BSP_RAM_EVENT_LOG,sizeof(*arena));if(!arena){g_device_log.error=CFW_MEMORY;goto unavailable;}store=&arena->log;
  EventLogIO io={0,Read,Grant,Erase,Program};uint32_t uid[3]={HAL_GetUIDw0(),HAL_GetUIDw1(),HAL_GetUIDw2()};EventLog_Init(store,&io,uid);
 }
 if(read_pending&&store->phase==LOG_READY){
  reader.result=reader.sequence==store->sequence?Read(0,reader.offset,reader.bytes,256):CFW_BUSY;
  __atomic_store_n(&read_pending,0,__ATOMIC_RELEASE);__atomic_store_n(&read_complete,1,__ATOMIC_RELEASE);return;
 }
 EventLog_Process(store);
 if(store->phase==LOG_READY&&!boot_posted){
  g_device_log.boot_id=g_recovery_mailbox.request.sequence;
  DeviceEvent e={.code=LOG_BOOT,.boot=g_device_log.boot_id,.time_ms=now,.detail=RCC->CSR};
  (void)DeviceLog_Post(&e);
  GateFault fault;memcpy(&fault,(const void*)&g_recovery_mailbox.fault,sizeof(fault));
  if(GateFault_Valid(&fault)){e.code=LOG_FAULT;e.detail=fault.code;e.boot=fault.boot;memcpy(e.data,fault.registers,sizeof(e.data));
   if(DeviceLog_Post(&e)){fault_pending=1;fault_after=store->written+count;}}
  boot_posted=1;
 }
 if(fault_pending&&store->written>=fault_after){g_recovery_mailbox.fault.magic=0;__DSB();fault_pending=0;}
 if(store->phase==LOG_READY&&count&&(drain||now-last_flush>=1000U||count==16)){
  uint32_t irq=__get_PRIMASK();__disable_irq();
  uint32_t batch=count;memcpy(arena->batch,queue,batch*sizeof(DeviceEvent));count=0;g_device_log.queued=0;
  __set_PRIMASK(irq);
  (void)EventLog_Submit(store,arena->batch,batch);last_flush=now;
 }
 g_device_log.state=store->phase;g_device_log.error=store->error;g_device_log.sequence=store->sequence;g_device_log.written=store->written;
 if(store->phase!=LOG_ERROR)return;
 unavailable:
 if(read_pending){reader.result=g_device_log.error?g_device_log.error:CFW_IO;
  __atomic_store_n(&read_pending,0,__ATOMIC_RELEASE);__atomic_store_n(&read_complete,1,__ATOMIC_RELEASE);}
}
#endif
