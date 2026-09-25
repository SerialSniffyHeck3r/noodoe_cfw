#ifndef BACKLIGHT_POLICY_H
#define BACKLIGHT_POLICY_H
#include <stdint.h>
typedef struct {uint32_t tick,candidate,since,source_valid,target;} BacklightPolicy;
/* V5.16 mode2: calibrated index0..9 -> control10..100 -> TIM5 CCR4
 * min(control,99), PWM1/high. Preserve this traced electrical mapping;
 * optical polarity must not be inferred from the index names alone.
 * Calibration/UART reporting remain untouched. */
uint32_t BacklightPolicy_Target(uint32_t index,int32_t bias);
uint32_t BacklightPolicy_Step(BacklightPolicy *policy,uint32_t now,uint32_t active,
    uint32_t automatic,uint32_t valid,uint32_t index,int32_t bias,uint32_t manual,uint32_t current);
#endif
