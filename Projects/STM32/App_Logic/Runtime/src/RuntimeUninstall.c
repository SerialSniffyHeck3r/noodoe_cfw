#if NOODOE_PRODUCT
#include "RuntimeUninstall.h"
#include "Uninstall_Expected.h"
#include "BSP_NOR.h"
#include "BSP_RAM.h"
#include "NoodoeBluetooth.h"
#include "Health_Service.h"
#include "Update_Metadata.h"
#include "stm32f4xx_hal.h"
static void *scratch;
static uint32_t attempted;
uint32_t RuntimeUninstall_Enable(uint32_t tx)
{if(!scratch)scratch=BSP_RAM_Allocate(UPDATE_METADATA_SECTOR_SIZE);
 return !scratch?1:BSP_NOR_OTAEnable(tx);}
/* The common updater sends even chunks aligned inside256-byte pages. READ
 * also supports odd diagnostic offsets without exposing neighboring bytes. */
uint32_t RuntimeUninstall_Read(uint32_t a,void *p,uint32_t n)
{uint8_t *out=p;
 while(n){if((a&1)||n==1){if(BSP_NOR_Read(a^1U,out,1))return 1;a++;out++;n--;}
  else{uint32_t k=n&~1U;if(BSP_NOR_Read(a,out,k))return 1;
   for(uint32_t i=0;i<k;i+=2){uint8_t v=out[i];out[i]=out[i+1];out[i+1]=v;}a+=k;out+=k;n-=k;}}
 return 0;}
uint32_t RuntimeUninstall_Program(uint32_t a,const void *p,uint32_t n)
{uint8_t pair[256];if((a|n)&1U||!n||n>256)return 1;
 for(uint32_t i=0;i<n;i++)pair[i]=((const uint8_t*)p)[i^1U];
 return BSP_NOR_OTAProgram(a,pair,n);}
uint32_t RuntimeUninstall_Commit(uint32_t version,uint32_t crc)
{if(!scratch||version!=UNINSTALL_TRANSPORT_VERSION)return 1;
 uint32_t now=HAL_GetTick(),token=0;HealthService_Progress(HEALTH_STORAGE,now);
 if(!HealthService_Process(now,0)||Bluetooth_QuiesceTransport(1000,&token)||!token)return 1;
 uint32_t result=1;now=HAL_GetTick();HealthService_Progress(HEALTH_STORAGE,now);
 if(HealthService_Process(now,0)){
  attempted=1;result=UpdateMetadata_Commit(version,crc,UPDATE_METADATA_ARM_TOKEN,scratch,UPDATE_METADATA_SECTOR_SIZE);
 }
 (void)Bluetooth_ResumeTransport(token);return result;
}
uint32_t RuntimeUninstall_Untouched(void)
{UpdateMetadataDiagnostics d={0};if(!attempted)return 1;UpdateMetadata_GetDiagnostics(&d);return !d.destructive_started;}
#endif
