#ifndef BSP_RAM_H
#define BSP_RAM_H
#include <stdint.h>
#include <stddef.h>
#define BSP_RAM_BASE 0xC0000000UL
#define BSP_RAM_GEOMETRY_BYTES 0x04000000UL
#define BSP_RAM_TEST_BYTES 65536U
typedef struct {
    uint32_t magic,ready,result,geometry_bytes,capacity_bytes,tested_bytes;
    uint32_t first_bad_address,expected,observed,allocated_bytes,dma_verified;
} BSP_RAM_Diagnostics;
extern volatile BSP_RAM_Diagnostics g_bsp_ram;
/* Initializes/checks SDRAM before its first consumer. A successful repeated
 * call is a no-op; STOP wake uses self-refresh restoration, not this test. */
uint32_t BSP_RAM_Init(void);
/* Monotonic arena with fixed diagnostic records and32byte alignment. It excludes
 * the64KiB bring-up scratch area; NULL means unready/overflow/exhaustion. */
void *BSP_RAM_Allocate(size_t bytes);
/* DMA is forbidden for every CCM allocation. RX accepts only normal SRAM or
 * initialized external RAM; TX may additionally read flash. No implicit copy. */
static inline uint32_t BSP_RAM_DMAAccessible(const void *p,size_t n,uint32_t rx)
{
    uintptr_t a=(uintptr_t)p;if(!n||a>UINT32_MAX-n)return 0;
    return (a>=0x20000000U&&a+n<=0x20030000U)||
        (a>=0xC0000000U&&a+n<=0xC4000000U)||
        (!rx&&a>=0x08000000U&&a+n<=0x08100000U);
}
enum {BSP_RAM_RESOURCE=1,BSP_RAM_CAPTURE,BSP_RAM_CAPTURE_DL,BSP_RAM_RESOURCE_IMPORT,
    BSP_RAM_CFW_AUDIT,BSP_RAM_CFW_JOURNAL,BSP_RAM_CFW_PHOTOS,BSP_RAM_CFW_APP,BSP_RAM_CFW_CONFIG,BSP_RAM_CFW_INSTALL,BSP_RAM_BOOT_STORE,BSP_RAM_EVENT_LOG,BSP_RAM_TEXT_STORE,BSP_RAM_ROUTINE_AUDIT,BSP_RAM_PHONE_VISUAL};
/* Legacy owner0 allocations retain their caller PC for ELF symbol lookup. */
typedef struct {uint32_t owner,address,bytes,caller;} BSP_RAM_Allocation;
extern volatile BSP_RAM_Allocation g_bsp_ram_allocations[32];
/* Idempotent named reservation. A different size for the same owner fails;
 * callers cannot silently leak another arena on retry or STOP wake. */
void *BSP_RAM_AllocateNamed(uint32_t owner,size_t bytes);
#endif
