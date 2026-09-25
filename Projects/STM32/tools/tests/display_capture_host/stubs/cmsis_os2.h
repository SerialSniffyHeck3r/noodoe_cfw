#ifndef TEST_CMSIS_H
#define TEST_CMSIS_H
#include <stdint.h>
typedef enum {osKernelInactive=0,osKernelRunning=2} osKernelState_t;
osKernelState_t osKernelGetState(void);
uint32_t osDelay(uint32_t ticks);
#endif
