#include <stdint.h>
#include <stddef.h>
#include "stm32f4xx_hal.h"
typedef void *osMutexId_t;
#define osKernelRunning 2
#define osOK 0
static inline uint32_t __get_IPSR(void){return 0;}
static inline uint32_t __get_BASEPRI(void){return 0;}
static inline uint32_t osKernelGetState(void){return osKernelRunning;}
static inline void *osMutexNew(void *p){(void)p;return (void*)1;}
static inline uint32_t osMutexAcquire(void *p,uint32_t t){(void)p;(void)t;return 0;}
static inline uint32_t osMutexRelease(void *p){(void)p;return 0;}
