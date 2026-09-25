
#include "Resources.h"
#include "ResourceStore.h"
#include "StorageService.h"
#include "StorageDisk.h"
#include "StorageBackup.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "BSP_Power.h"
#include <string.h>
#include "BootStore.h"
/* Resource writes run behind a confirmed Gate journal. This boundary fixture
 * also injects trial/reset states; it does not prove physical Gate operation. */
volatile BootStoreDiagnostics g_boot_store={.confirmed=1};
volatile uint32_t boot_ready=1,boot_flags;
uint32_t BootStore_GetBootInfo(GateJournalRecord *out){
 memset(out,0,sizeof(*out));out->state=GATE_J_CONFIRMED;out->flags=boot_flags;
 return boot_ready;
}
volatile BSP_NOR_Diagnostics g_bsp_nor={.ready=1};
volatile StorageBackup_Diagnostics g_storage_backup={.transport_verified=1};
volatile BSP_Power_Diagnostics g_bsp_power={.ign_valid=1,.ign_on=1};
volatile uint32_t writes,allocated,cluster,root_index,desired_slot,desired_command=1,expected_state=RESOURCES_READY;
static void *named[8];static size_t sizes[8];
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
BSP_NOR_Status BSP_NOR_Read(uint32_t a,void *d,uint32_t n){if(a>0x8000000||n>0x8000000-a)return BSP_NOR_ARGUMENT;memcpy(d,(void*)(0x90000000U+a),n);return BSP_NOR_OK;}
uint32_t BSP_NOR_CanWriteStorage(void){return 1;}
BSP_NOR_Status BSP_NOR_Program(uint32_t a,const void *d,uint32_t n){
 if(a+n>0x7F70000U)return BSP_NOR_ARGUMENT;
 uint8_t *p=(uint8_t*)(0x90000000U+a);const uint8_t *v=d;
 for(uint32_t i=0;i<n;++i){if((p[i]&v[i])!=v[i])return BSP_NOR_VERIFY;p[i]=v[i];}
 ++writes;return BSP_NOR_OK;}
BSP_NOR_Status BSP_NOR_UnlockStorage(uint32_t t){return t==0x42414B32U?BSP_NOR_OK:BSP_NOR_ARGUMENT;}
void BSP_NOR_LockStorage(void){}
BSP_NOR_Status BSP_NOR_Erase4K(uint32_t a){if(a+4096>0x7F70000U)return BSP_NOR_ARGUMENT;memset((void*)(0x90000000U+a),255,4096);++writes;return BSP_NOR_OK;}
BSP_NOR_Status BSP_NOR_BeginFormat(void){return BSP_NOR_LOCKED;}
void BSP_NOR_EndFormat(void){}
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t n){
 if(owner>=8)return 0;
 if(named[owner])return sizes[owner]==n?named[owner]:0;
 n=(n+31)&~31U;void *p=(void*)(0xC0010000U+allocated);allocated+=n;named[owner]=p;sizes[owner]=n;return p;
}
uint32_t TestInit(void){CHECK(StorageService_Init()==FR_OK);return 0;}
uint32_t TestInstall(void)
{
 CHECK(StorageService_Init()==FR_OK);ResourceStore_Process();CHECK(g_resource_install.buffer);
 memcpy((void *)g_resource_install.buffer,(void*)0xA0000000U,524288);
 g_resource_install.command=desired_command;g_resource_install.slot=desired_slot;
 g_resource_install.first_cluster=cluster;g_resource_install.root_index=root_index;g_resource_install.token=0x42414B32;
 g_resource_install.sequence=g_resource_install.ack+1;
 for(uint32_t i=0;i<1000&&g_resource_install.ack!=g_resource_install.sequence;++i)ResourceStore_Process();
 CHECK(g_resource_install.ack==g_resource_install.sequence);return g_resource_install.error;
}
uint32_t TestLoad(void)
{
 CHECK(StorageService_Init()==FR_OK);CHECK(Resources_RequestLoad());
 for(uint32_t i=0;i<1000&&Resources_GetStatus()!=RESOURCES_READY&&Resources_GetStatus()!=RESOURCES_FAILED;++i)Resources_Process();
 CHECK(Resources_GetStatus()==expected_state);
 if(expected_state==RESOURCES_READY){ResourceView v;for(uint32_t i=1;i<=RESOURCES_COUNT;++i)CHECK(Resources_Get(i,&v)&&v.bytes);
 uint32_t before=allocated;for(uint32_t i=0;i<1000;++i){CHECK(Resources_RequestLoad());Resources_Process();}CHECK(before==allocated);}
 else {ResourceView v;CHECK(!Resources_Get(1,&v));}
 return 0;
}

uint32_t TestCompatibility(void)
{
 const uint8_t *required=(void*)0xA0000010U;
 for(uint32_t i=0;i<1000&&!ResourceStore_Compatible(required);++i)ResourceStore_Process();
 CHECK(ResourceStore_Compatible(required));return 0;
}
/* Exercise the real SPP RAM receiver and StorageTask installer, including
 * bounded chunks, replay rejection, key OFF and cancellation boundaries. */
uint32_t TestWire(uint32_t cancel)
{
 CHECK(!TestLoad());ResourceStore_Process();g_bsp_power.ign_on=0;
 const uint8_t *src=(void*)0xa0000000U;uint32_t total=(*(const uint32_t*)(src+8)+8191)&~4095U;
 uint8_t b[968],r[92];uint32_t tx=42;memcpy(b,&tx,4);memcpy(b+4,&total,4);memcpy(b+8,src+16,32);
 CHECK(!ResourceStore_Transfer(0x8e,b,40,r));CHECK(ResourceStore_TransferActive());
 for(uint32_t i=0;i<128;i++)ResourceStore_Process();
 for(uint32_t at=0;at<total;){uint32_t n=total-at;if(n>960)n=960;if(n>4096-(at&4095))n=4096-(at&4095);
  memcpy(b,&tx,4);memcpy(b+4,&at,4);memcpy(b+8,src+at,n);
  CHECK(!ResourceStore_Transfer(0x8f,b,n+8,r));CHECK(ResourceStore_Transfer(0x8f,b,n+8,r));at+=n;
 }
 if(cancel==1){CHECK(!ResourceStore_TransferCancel());ResourceStore_Process();CHECK(!ResourceStore_TransferActive());CHECK(!writes);return 0;}
 memcpy(b,&tx,4);memcpy(b+4,src+16,32);CHECK(!ResourceStore_Transfer(0x90,b,36,r));
 uint32_t steps=0;while(ResourceStore_TransferActive()&&steps++<1000){
  ResourceStore_Process();if(cancel==2&&g_resource_install.progress==4096)ResourceStore_TransferCancel();
 }
 CHECK(steps<1000);CHECK(!ResourceStore_Transfer(0x8d,0,0,r));
 CHECK(*(uint32_t*)(r+4)==(cancel==2?6:4));return 0;
}
