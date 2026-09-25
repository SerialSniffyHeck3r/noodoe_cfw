#ifndef SETTINGS_MOTION_H
#define SETTINGS_MOTION_H
#include <stdint.h>
#define SETTINGS_ENTRY_DWELL_MS 5000U
#define SETTINGS_RECOVERY_DWELL_MS 3000U
#define SETTINGS_MOTION_TIMEOUT_MS 30000U
typedef struct {uint32_t entry_since,entry_tracking,ready,latched,started_ms,
 recovery_since,recovery_tracking,expired,unknown,elapsed_ms;} SettingsMotion;
/* Pure owner-task gate. Fresh UART speed, integer km/h; unsigned tick wrap.
 * Entry includes3; recovery excludes3. Repeated unsafe samples do not renew
 * the deadline. Recovery wins when it and expiry become eligible together. */
void SettingsMotion_Tick(SettingsMotion *s,uint32_t now,uint32_t valid,uint32_t speed,uint32_t in_settings);
#endif
