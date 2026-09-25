#include "StorageBackup.h"
#include "BSP_NOR.h"
#include <string.h>

/* Preserve the SWD diagnostic ABI used by existing NOR backup/install tools.
 * Legacy CDC counters are reserved zero, never a storage ownership signal. */
volatile StorageBackup_Diagnostics g_storage_backup;
/* Compare two independent transfer widths before exposing DMA data. Byte
 * comparisons cover the16MiB boundary and final page without writing anything. */
static uint32_t VerifyTransport(void)
{
    const uint32_t locations[]={0U,0x00FFFFE0U,0x07FFFFC0U};
    uint8_t polling[64],dma[64];
    for(uint32_t i=0;i<3U;++i) {
        if(BSP_NOR_ReadPolling(locations[i],polling,sizeof(polling))!=BSP_NOR_OK)return 1U;
        if(BSP_NOR_Read(locations[i],dma,sizeof(dma))!=BSP_NOR_OK)return 2U;
        if(memcmp(polling,dma,sizeof(dma)))return 3U;
    }
    return 0U;
}
/* Read-only transport qualification precedes every filesystem consumer.
 * A failure keeps writes disabled. This does not mount, format or export NOR. */
uint32_t StorageTransport_Init(void)
{
    memset((void *)&g_storage_backup,0,sizeof(g_storage_backup));
    g_storage_backup.magic=0x53544231U;g_storage_backup.version=1U;
    uint32_t status=(uint32_t)BSP_NOR_Init();
    if(!status){uint32_t check=VerifyTransport();if(check)status=0x100U+check;}
    g_storage_backup.result=status;g_storage_backup.transport_verified=status==0U;
    g_storage_backup.initialized=1U;
    return status;
}
