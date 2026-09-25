#include "App_Settings.h"
#include "stm32f4xx_hal.h"
/* Bounded copies only; peripheral functions are never called with IRQs off. */
uint32_t SettingsDevice_Lock(void){uint32_t mask=__get_PRIMASK();__disable_irq();return mask;}
void SettingsDevice_Unlock(uint32_t mask){__set_PRIMASK(mask);}
