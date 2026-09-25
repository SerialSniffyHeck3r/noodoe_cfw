#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H
#include <stdint.h>
uint32_t PowerService_RunRequired(void);
/* Nonblocking CPU/peripheral policy; DISPLAY_SLEEP retains BT clocks and
 * permits tickless CPU Sleep. Actual panel sleep is independently UI-owned.
 * Requested deep sleep is distinct from actual STOP. */
enum {POWER_RUN,POWER_ECONOMY,POWER_DISPLAY_SLEEP,POWER_DEEP};
enum {POWER_OWNER_IO=1,POWER_OWNER_STORAGE=2,POWER_OWNER_GRAPHICS=4,POWER_OWNER_BT=8};
typedef struct {uint32_t magic,version,requested,acknowledged,changes,wakes;} PowerDiagnostics;
extern volatile PowerDiagnostics g_power_service;
void PowerService_Request(uint32_t mode);
uint32_t PowerService_Mode(void);
/* Mark safe after bus work completes; clear before touching peripherals. */
void PowerService_Acknowledge(uint32_t owner,uint32_t ready);
void PowerService_Wait(uint32_t owner,uint32_t milliseconds);
/* Task-context wake after a debounced IGN or published minute change. */
void PowerService_Notify(uint32_t owners);
void PowerService_IgnitionIRQ(void);
#endif
