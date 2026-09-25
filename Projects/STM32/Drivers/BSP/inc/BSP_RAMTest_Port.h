#ifndef BSP_RAMTEST_PORT_H
#define BSP_RAMTEST_PORT_H
/* Internal platform seam. The CPU algorithm is host-tested independently of
 * the generated HAL; production owns only DMA2 Stream0 after basic RAM init. */
#include <stdint.h>
#include <stddef.h>
int RAMTestPort_Ready(void);
void *RAMTestPort_Allocate(size_t bytes);
uint32_t RAMTestPort_Time(void);
uint32_t RAMTestPort_Cycles(void);
void RAMTestPort_Enter(void);
void RAMTestPort_Exit(void);
void RAMTestPort_Barrier(void);
int RAMTestPort_DMAClaim(void);
void RAMTestPort_DMARelease(void);
int RAMTestPort_DMAStart(uintptr_t source,uintptr_t destination,uint32_t bytes);
/* Poll:0 pending,1 complete,-1 failed. No polling-with-timeout loop. */
int RAMTestPort_DMAPoll(void);
/* Stop:0 still disabling,1 safely disabled. Repeated calls never re-enable. */
int RAMTestPort_DMAStop(void);
#ifdef BSP_RAMTEST_HOST
void RAMTestPort_Store(uintptr_t address,uint32_t value);
uint32_t RAMTestPort_Load(uintptr_t address);
#else
static inline void RAMTestPort_Store(uintptr_t address,uint32_t value)
{ *(volatile uint32_t *)address=value; }
static inline uint32_t RAMTestPort_Load(uintptr_t address)
{ return *(volatile const uint32_t *)address; }
#endif
#endif
