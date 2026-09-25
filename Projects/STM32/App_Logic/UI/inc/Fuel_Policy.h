#ifndef FUEL_POLICY_H
#define FUEL_POLICY_H
#include <stdint.h>
enum {FUEL_UNKNOWN=0,FUEL_NORMAL,FUEL_LOW,FUEL_CRITICAL,FUEL_ERROR};
#define FUEL_START_RESERVE 0x100U
#define FUEL_CLEAR_RESERVE 0x200U
typedef struct {uint32_t candidate,since,last_sample,have_sample,stable,announced,session;} FuelPolicy;
/* raw0 is measurement error; 50/51 are observed,52..55 are inferred bars.
 * Result low byte is one warning, high bits are domain reserve transitions. */
uint32_t FuelPolicy_Step(FuelPolicy*,uint32_t session,uint32_t now,
 uint32_t valid,uint32_t sample_ms,uint32_t raw,uint32_t reserve);
#endif
