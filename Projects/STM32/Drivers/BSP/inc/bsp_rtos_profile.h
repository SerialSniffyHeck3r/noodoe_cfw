#ifndef BSP_RTOS_PROFILE_H
#define BSP_RTOS_PROFILE_H
/* USER CODE includes this after Cube defaults. First integrated hardware run
 * used20,824 of65,536 bytes with UI/IO/storage tasks;48KiB retains over27KiB
 * allocation margin and makes room for snapshot/update buffers in main SRAM.
 * The complete graphics-test profile preserves its previous64KiB setting. */
#if (defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED) || (defined(NOODOE_BOOTSTRAP) && NOODOE_BOOTSTRAP)
#undef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE ((size_t)49152)
#endif
#endif
