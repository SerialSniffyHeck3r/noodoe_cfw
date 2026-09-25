#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "RAM codec requires little-endian target"
#endif
#include "Resources.h"
#include "StorageService.h"
#include "Update_Service.h"
#include "BSP_RAM.h"
#include <string.h>
volatile ResourcesDiagnostics g_resources;
static uint8_t *arena;
static UpdateSha256 hash;
static const uint8_t expected[32]=RESOURCES_EXPECTED_SHA;
static uint32_t U32(const uint8_t *p){ uint32_t v;memcpy(&v,p,4);return v;}
static void State(ResourcesState s){__atomic_store_n(&g_resources.state,s,__ATOMIC_RELEASE);}
ResourcesState Resources_GetStatus(void){return (ResourcesState)__atomic_load_n(&g_resources.state,__ATOMIC_ACQUIRE);}
/* One request may be outstanding. Retry is deliberately read-only and reuses
 * the same allocation. Consumers never see failed/partially loaded bytes. */
uint32_t Resources_RequestLoad(void)
{
    uint32_t state=Resources_GetStatus();
    if(state==RESOURCES_READY)return 1;
    if(state!=RESOURCES_IDLE&&state!=RESOURCES_FAILED)return 0;
    return __atomic_compare_exchange_n(&g_resources.state,&state,RESOURCES_QUEUED,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
}
uint32_t Resources_Get(uint32_t id,ResourceView *out)
{
    if(out){out->data=NULL;out->bytes=0;}
    if(!out||id<1||id>RESOURCES_COUNT||Resources_GetStatus()!=RESOURCES_READY)return 0;
    const uint8_t *e=arena+48+(id-1)*16;
    out->data=arena+RESOURCES_HEADER_BYTES+U32(e+4);out->bytes=U32(e+8);return 1;
}
static void Fail(ResourcesError error)
{
    g_resources.error=error;++g_resources.failures;g_resources.loaded=0;
    if(!g_resources.slot){g_resources.slot=1;State(RESOURCES_HEADER);}
    else State(RESOURCES_FAILED);
}
void Resources_Process(void)
{
    ResourcesState s=Resources_GetStatus();uint32_t got=0;
    if(s==RESOURCES_QUEUED){
        g_resources.magic=0x52534331;g_resources.version=1;++g_resources.requests;
        g_resources.slot=0;g_resources.loaded=0;g_resources.error=0;
        if(!arena)arena=BSP_RAM_AllocateNamed(BSP_RAM_RESOURCE,RESOURCES_SLOT_BYTES);
        if(!arena){g_resources.error=RESOURCE_RAM;State(RESOURCES_FAILED);return;}
        g_resources.address=(uint32_t)arena;
        uint32_t bytes=0;
        if(StorageService_Stat("0:/NOODOE.RSC",&bytes)!=FR_OK){g_resources.error=RESOURCE_MISSING;State(RESOURCES_FAILED);return;}
        if(bytes!=2*RESOURCES_SLOT_BYTES){g_resources.error=RESOURCE_SIZE;State(RESOURCES_FAILED);return;}
        State(RESOURCES_HEADER);return;
    }
    if(s==RESOURCES_HEADER){
        if(StorageService_ReadFile("0:/NOODOE.RSC",g_resources.slot*RESOURCES_SLOT_BYTES,arena,RESOURCES_HEADER_BYTES,&got)!=FR_OK||got!=RESOURCES_HEADER_BYTES){Fail(RESOURCE_IO);return;}
        ResourcesError result=Resources_CheckHeader(arena,expected);
        if(result){Fail(result);return;}
        if(U32(arena+12)!=RESOURCES_COUNT){Fail(RESOURCE_SIZE);return;}
        g_resources.total=U32(arena+8);g_resources.loaded=0;
        UpdateSha256_Init(&hash);UpdateSha256_Feed(&hash,arena+48,RESOURCES_COUNT*16);
        State(RESOURCES_LOADING);return;
    }
    if(s!=RESOURCES_LOADING)return;
    uint32_t n=g_resources.total-g_resources.loaded;if(n>4096)n=4096;
    uint8_t *p=arena+RESOURCES_HEADER_BYTES+g_resources.loaded;
    if(StorageService_ReadFile("0:/NOODOE.RSC",g_resources.slot*RESOURCES_SLOT_BYTES+RESOURCES_HEADER_BYTES+g_resources.loaded,p,n,&got)!=FR_OK||got!=n){Fail(RESOURCE_IO);return;}
    UpdateSha256_Feed(&hash,p,n);g_resources.loaded+=n;
    if(g_resources.loaded!=g_resources.total)return;
    uint8_t digest[32];UpdateSha256_Final(&hash,digest);
    if(memcmp(digest,expected,32)){Fail(RESOURCE_SHA);return;}
    for(uint32_t i=0;i<RESOURCES_COUNT;++i){const uint8_t *e=arena+48+i*16;
        if((StorageService_Crc32(arena+RESOURCES_HEADER_BYTES+U32(e+4),U32(e+8),~0U)^~0U)!=U32(e+12)){Fail(RESOURCE_CRC);return;}
    }
    g_resources.error=0;State(RESOURCES_READY);
}
