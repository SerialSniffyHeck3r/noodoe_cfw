#ifndef ODOMETER_GUARD_H
#define ODOMETER_GUARD_H
#include <stdint.h>
#define ODOMETER_MAX_KM 999999U
#define ODOMETER_CONFIRM_MS 3000U
enum {ODO_NORMAL,ODO_REVERSED,ODO_JUMP,ODO_RANGE};
enum {ODO_KEEP=1,ODO_ACCEPT,ODO_LATER};
/* Pure model: raw never leaves this module as a corrected vehicle packet.
 * Persistent quantities are distinct from boot-local debounce/timestamps. */
typedef struct {
 uint32_t valid,raw,display,corrected,pending,reason,candidate_base,candidate;
 uint32_t revision,last_ms,last_sample,have_sample,tracking,since,count,dirty;
 int32_t offset;
} OdometerGuard;
void OdometerGuard_Feed(OdometerGuard *,uint32_t now,uint32_t sample,uint32_t valid,uint32_t raw);
uint32_t OdometerGuard_Choose(OdometerGuard *,uint32_t decision);
/* UI-owner facade; request acceptance is separate from journal completion.
 * GetStatus is a RAM snapshot usable by the phone service, never a NOR read. */
typedef struct {uint32_t valid,raw,display,corrected,pending,reason,ready,revision,save_result;} OdometerStatus;
void OdometerService_Tick(uint32_t now,uint32_t sample,uint32_t valid,uint32_t raw,uint32_t ign,uint32_t speed_valid,uint32_t speed);
void OdometerGuard_GetStatus(OdometerStatus *);
uint32_t OdometerGuard_RequestDecision(uint32_t revision,uint32_t decision);
uint32_t OdometerService_TripValid(void);
void OdometerService_Checkpoint(void);
#endif
