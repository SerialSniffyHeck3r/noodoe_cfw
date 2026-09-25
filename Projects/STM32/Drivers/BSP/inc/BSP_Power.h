#ifndef BSP_POWER_H
#define BSP_POWER_H
#include <stdint.h>
#define BSP_POWER_IGN_DEBOUNCE_MS 11U
typedef struct {
    uint32_t magic,ready,raw_ign_off,ign_valid,ign_on,changes,last_change_ms;
    uint32_t shutdown_outputs,board_revision;
} BSP_Power_Diagnostics;
extern volatile BSP_Power_Diagnostics g_bsp_power;
/* Input observation never cuts power. PG13 is MCU levelLOW=IGNON. */
void BSP_Power_Init(uint32_t board_revision);
void BSP_Power_Process(uint32_t now_ms);
/* Called only AFTER storage/BT/display are quiescent. Revision-specific PI9
 * is left untouched for unknown revisions; caller must supply verified data. */
uint32_t BSP_Power_RequestOff(void);
#endif
