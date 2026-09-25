#ifndef RIDER_NAME_H
#define RIDER_NAME_H
#include <stdint.h>

#define RIDER_NAME_MAX_BYTES 48U
#define RIDER_NAME_CAPACITY (RIDER_NAME_MAX_BYTES+1U)
/* These APIs run in normal task context, including a future phone command
 * consumer. Set copies a length-delimited UTF-8 packet immediately into fixed
 * RAM; the caller can then reuse its packet. It performs no flash/BT/LVGL IO.
 * Empty input clears the name. Invalid/control/oversize input changes nothing.
 * A session override survives IGN/STOP, but not loss of MCU power. */
uint32_t RiderName_Set(const char *utf8,uint32_t bytes);
/* Copy a coherent name into at least CAPACITY bytes, or leave output untouched
 * and return0. Without an override, use the committed settings name (or empty
 * before storage is ready). The display never owns a pointer into this state. */
uint32_t RiderName_Get(char *out,uint32_t capacity);
/* Pure validator shared with the persistent codec. No implicit strlen on phone
 * bytes. Valid Unicode scalars only; C0/C1 and line/paragraph separators denied. */
uint32_t RiderName_Validate(const char *utf8,uint32_t bytes);
#endif
