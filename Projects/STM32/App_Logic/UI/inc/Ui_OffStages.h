#ifndef UI_OFF_STAGES_H
#define UI_OFF_STAGES_H
#include "Ui_State.h"
/* Pure, bounded stage selection. Disabled/zero-time intermediate stages are
 * skipped; the last enabled stage holds until IGN. No device IO or allocation. */
uint32_t Ui_OffStages_IsOff(uint32_t power);
uint32_t Ui_OffStages_First(const UiConfig *config);
uint32_t Ui_OffStages_Next(const UiConfig *config,uint32_t power,uint32_t elapsed_ms);
#endif
