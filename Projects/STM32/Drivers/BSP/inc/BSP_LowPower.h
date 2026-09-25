#ifndef BSP_LOWPOWER_H
#define BSP_LOWPOWER_H
#include <stdint.h>
typedef struct {uint32_t magic,version,policy,stop_allowed,sleeps,stops,aborts,
 rtc_wakes,ign_edges,elapsed_ms,last_sleep_ms,max_sleep_ms,error,phase,wake_pending;
 /* Actual entries, not requested policy. Prefix remains SWD compatible. */
 uint32_t idle_wfi,last_mode,entry_rcc_cr,entry_pwr_cr,entry_scr;} BSP_LowPowerDiagnostics;
extern volatile BSP_LowPowerDiagnostics g_bsp_lowpower;
/* Policy0=run,1/2=Sleep,3=STOP candidate. Readiness is supplied by the service;
 * the port rechecks DMA, actual GPIO and clock state immediately before WFI. */
void BSP_LowPower_SetPolicy(uint32_t policy,uint32_t stop_allowed);
void BSP_LowPower_CancelFromISR(void);
#endif
