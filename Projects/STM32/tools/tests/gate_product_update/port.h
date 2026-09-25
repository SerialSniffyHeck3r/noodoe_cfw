#include "stm32f4xx_hal.h"
#undef NVIC_SystemReset
void GateProductTest_Reset(void);
#define NVIC_SystemReset GateProductTest_Reset
