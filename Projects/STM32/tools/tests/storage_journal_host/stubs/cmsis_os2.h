#ifndef CMSIS_OS2_H
#define CMSIS_OS2_H
#include <stdint.h>
typedef void *osMutexId_t;
typedef enum {osOK=0,osError=-1} osStatus_t;
typedef enum {osKernelInactive=0,osKernelRunning=2} osKernelState_t;
osKernelState_t osKernelGetState(void);
osMutexId_t osMutexNew(const void *attributes);
osStatus_t osMutexAcquire(osMutexId_t mutex,uint32_t timeout);
osStatus_t osMutexRelease(osMutexId_t mutex);
#endif
