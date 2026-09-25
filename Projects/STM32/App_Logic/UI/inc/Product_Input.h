#ifndef NOODOE_PRODUCT_INPUT_H
#define NOODOE_PRODUCT_INPUT_H
#include "Ui_State.h"
/* Owner-task bridge from the single Graphics/BSP event consumer to the pure
 * UI reducer. Returns1 for a recognized event, not a guaranteed transition:
 * boot-held, warning, power and long-press policies remain reducer-owned.
 * No queue consumption, allocation, GPIO access or waiting occurs here. */
uint32_t ProductInput_Dispatch(UiState *state,uint32_t bsp_button,
    uint32_t bsp_event,uint32_t duration_ms,uint32_t now_ms);
#endif
