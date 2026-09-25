#ifndef NOODOE_CLOCK_SERVICE_H
#define NOODOE_CLOCK_SERVICE_H
#include "BSP_Clock.h"
/* Task callers enqueue one immutable UTC value. Only the storage worker calls
 * Process; acceptance is not completion. No backup-domain reset or retry. */
uint32_t ClockService_Request(const BSP_Clock_Time *value,uint32_t *id);
uint32_t ClockService_Result(uint32_t id,uint32_t *result);
void ClockService_Process(void);
#endif
