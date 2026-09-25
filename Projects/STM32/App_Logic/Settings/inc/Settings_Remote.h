#ifndef SETTINGS_REMOTE_H
#define SETTINGS_REMOTE_H
#include <stdint.h>
/* Version 2 catalogue includes actions/RTC/read-only diagnostics, not just
 * persistence fields. Existing 0x71..73 remain wire compatible. */
void SettingsRemote_Tick(uint32_t now,uint32_t ign,uint32_t speed_valid,uint32_t speed);
int32_t SettingsRemote_Handle(uint32_t op,const uint8_t *p,uint32_t n,uint32_t epoch,uint8_t *out,uint32_t *bytes);
#endif
