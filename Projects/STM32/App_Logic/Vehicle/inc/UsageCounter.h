#ifndef USAGE_COUNTER_H
#define USAGE_COUNTER_H
#include <stdint.h>
typedef struct {
    uint64_t on_ms;
    uint32_t last_ms,known,on,primed,gaps;
} UsageCounter;
/* Single owner, no hardware. First sample establishes the origin. Elapsed
 * intervals belong to the previously observed IGN state. Unknown intervals
 * and scheduler gaps over1second are excluded explicitly, never guessed ON. */
void UsageCounter_Sample(UsageCounter *counter,uint32_t now_ms,uint32_t valid,uint32_t on);
#endif
