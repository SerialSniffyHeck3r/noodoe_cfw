#include "Graphics_Memory.h"
volatile GraphicsMemoryDiagnostics g_graphics_memory;
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
#define POOL_BYTES (48U*1024U)
#define GUARD 0xC04D4D31U
/* Both sentinels sit outside the allocator's advertised bounds. The linker
 * and strong APP startup explicitly own .ccm_bss; no Cube/vendor edits. */
static struct {uint32_t before[8];uint8_t bytes[POOL_BYTES];uint32_t after[8];}
    pool __attribute__((section(".ccm_bss.graphics"),aligned(32)));
void *GraphicsMemory_Pool(size_t bytes)
{
    if(bytes!=POOL_BYTES)return NULL;
    for(uint32_t i=0;i<8U;++i)pool.before[i]=pool.after[i]=GUARD;
    g_graphics_memory.version=1U;g_graphics_memory.address=(uint32_t)pool.bytes;
    g_graphics_memory.bytes=POOL_BYTES;g_graphics_memory.magic=0x43434D31U;
    return pool.bytes;
}
uint32_t GraphicsMemory_Check(void)
{
    ++g_graphics_memory.checks;
    for(uint32_t i=0;i<8U;++i)if(pool.before[i]!=GUARD||pool.after[i]!=GUARD){++g_graphics_memory.failures;return 0U;}
    return g_graphics_memory.magic==0x43434D31U;
}
#else
void *GraphicsMemory_Pool(size_t bytes){(void)bytes;return NULL;}
uint32_t GraphicsMemory_Check(void){return 1U;}
#endif
