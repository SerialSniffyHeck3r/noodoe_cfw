#ifndef SCALAR_TRANSITION_H
#define SCALAR_TRANSITION_H
#include <stdint.h>
/* Owner-task, allocation-free shared slow-fast-slow animation for opacity,
 * brightness or another bounded scalar0..1024. Clock wraps are supported. */
typedef struct {uint32_t from,target,value,start_ms,duration_ms;} ScalarTransition;
uint32_t ScalarTransition_Value(ScalarTransition *s,uint32_t now);
/* Repeated requests do not restart; reversals sample the current value first.
 * duration0 applies immediately. Callers supply target<=1024,duration<=60000. */
void ScalarTransition_Request(ScalarTransition *s,uint32_t target,uint32_t duration,uint32_t now);
#endif
