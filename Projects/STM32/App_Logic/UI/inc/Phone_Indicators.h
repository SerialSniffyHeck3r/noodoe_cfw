#ifndef PHONE_INDICATORS_H
#define PHONE_INDICATORS_H
#include <stdint.h>
typedef struct {uint32_t id,revision,age_ms,read;} PhoneIndicatorEntry;
/* Caller serializes UI/control access with a bounded critical section. */
uint32_t PhoneIndicators_Update(uint32_t epoch,uint32_t source,uint32_t now,uint32_t gps,
 PhoneIndicatorEntry *entries,uint32_t count);
void PhoneIndicators_MarkRead(void);
void PhoneIndicators_Colors(uint32_t now,uint32_t connected,uint32_t *bt,uint32_t *gps);
#endif
