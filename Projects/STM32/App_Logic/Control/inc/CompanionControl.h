#ifndef COMPANION_CONTROL_H
#define COMPANION_CONTROL_H
#include <stdint.h>
/* I/O-owner parser adapter; writes only bounded App API mailboxes. No NOR or
 * LVGL call is allowed here. Wire integers are LE32, never native structs. */
int32_t CompanionControl_Handle(uint32_t op,const uint8_t *in,uint32_t n,
    uint32_t epoch,uint32_t now,uint8_t *out,uint32_t *bytes);
#endif
