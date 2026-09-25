#include "FreeRTOS.h"
typedef enum {eAbortSleep,eStandardSleep,eNoTasksWaitingTimeout} eSleepModeStatus;
eSleepModeStatus eTaskConfirmSleepModeStatus(void);
void vTaskStepTick(TickType_t ticks);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t ticks);
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake);
void xTaskNotifyGive(TaskHandle_t task);
