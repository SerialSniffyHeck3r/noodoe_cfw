#ifndef GRAPHICS_MEMORY_H
#define GRAPHICS_MEMORY_H
#include <stdint.h>
#include <stddef.h>
/* LVGL-only CPU pool. Never lend these addresses to a peripheral DMA stream.
 * Reset clears the NOLOAD arena; repeated calls return the same pool, not a
 * fresh allocation. lv_init/lv_deinit own all allocations inside this region. */
void *GraphicsMemory_Pool(size_t bytes);
uint32_t GraphicsMemory_Check(void);
typedef struct {uint32_t magic,version,address,bytes,checks,failures;} GraphicsMemoryDiagnostics;
extern volatile GraphicsMemoryDiagnostics g_graphics_memory;
#endif
