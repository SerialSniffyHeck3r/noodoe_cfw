#ifndef BOOTSTRAP_RECOVERY_H
#define BOOTSTRAP_RECOVERY_H
#include "Update_Service.h"
enum {BOOT_DRAIN_WAIT,BOOT_DRAIN_READY,BOOT_DRAIN_UNRESOLVED,BOOT_DRAIN_TIMEOUT};
/* Owner-only, between completed physical operations. This function never
 * writes metadata, resets, feeds/stops a watchdog or changes staging bytes. */
uint32_t BootstrapRecovery_Drain(UpdateService *,uint32_t storage_idle,uint32_t elapsed);
#endif
