#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H
#include <stdint.h>

/* One hardware writer for software-started IWDG. No option bytes, debug-freeze
 * bits, IRQ callback, or peripheral driver may refresh the counter directly. */
enum { WATCHDOG_OFF, WATCHDOG_BOOT, WATCHDOG_RUN, WATCHDOG_WAIT,
       WATCHDOG_RECOVERY, WATCHDOG_FLASH, WATCHDOG_FAILED };
typedef struct {
    uint32_t magic, version, phase, failure, feeds, rejected;
    uint32_t started_ms, last_progress_ms, last_feed_ms, progress, budget_ms;
    uint32_t flash_started_cycles, flash_cycles, flash_previous_phase;
    uint32_t sleep_grants, sleep_until_ms, initialized;
    uint32_t last_feed_cycles,flash_completions,last_flash_ms,flash_grace_ms;
} BSP_WatchdogDiagnostics;
extern volatile BSP_WatchdogDiagnostics g_bsp_watchdog;

/* Pre-C startup entry: no RAM/globals, HAL, interrupts or inherited clock rate.
 * Returns zero if LSI/configuration cannot settle in a finite register poll.
 * Hardware maximum /256, reload4095: nominal32.768s at32kHz, 22.31..61.69s
 * over17..47kHz LSI. F429 cannot provide an exact60s CPU-independent timeout. */
uint32_t BSP_Watchdog_StartEarly(void);
/* Called once after C data initialization. BOOT<=30s, RECOVERY<=10min;
 * WAIT deliberately has no user-wait limit. RUN is supervised separately. */
uint32_t BSP_Watchdog_Init(uint32_t now_ms,uint32_t phase,uint32_t budget_ms);
uint32_t BSP_Watchdog_SetPhase(uint32_t now_ms,uint32_t phase,uint32_t budget_ms);
/* BOOT/RECOVERY/WAIT active-loop progress, NOT a timer/IRQ heartbeat. Repeated
 * progress values cannot keep feeding. Calls more than2s apart fail closed. */
uint32_t BSP_Watchdog_Checkpoint(uint32_t now_ms,uint32_t progress);
/* The health supervisor is the only runtime caller. It first verifies every
 * registered owner. A latched failure cannot be revived by later callbacks. */
uint32_t BSP_Watchdog_RunCheckpoint(uint32_t now_ms);
void BSP_Watchdog_Fail(uint32_t reason);
/* FLASH lease is acquired once before the first flash operation. IRQ masking
 * is permitted; exception context is not. The lease cannot be renewed.
 * The three RAM routines touch only SRAM, core/peripheral registers and other
 * RAM routines. DWT elapsed time remains usable while FLASH fetches stall. */
uint32_t BSP_Watchdog_BeginFlash(uint32_t now_ms,uint32_t clock_hz);
uint32_t BSP_Watchdog_RamCheckpoint(void);
uint32_t BSP_Watchdog_EndFlash(void);
/* STOP receives a short-lived proof from the supervisor. The sleep hook never
 * feeds. All task deadlines remain enforced across RTC-compensated sleep. */
void BSP_Watchdog_GrantSleep(uint32_t now_ms);
uint32_t BSP_Watchdog_CanSleep(uint32_t now_ms,uint32_t sleep_ms);
#endif
