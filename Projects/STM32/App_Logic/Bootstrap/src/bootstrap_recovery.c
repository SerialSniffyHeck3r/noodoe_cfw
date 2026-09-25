#include "Bootstrap_Recovery.h"
uint32_t BootstrapRecovery_Drain(UpdateService *update,uint32_t storage_idle,uint32_t elapsed)
{
    if(storage_idle)return UpdateService_CancelUncommitted(update)?BOOT_DRAIN_UNRESOLVED:BOOT_DRAIN_READY;
    return elapsed>=60000U?BOOT_DRAIN_TIMEOUT:BOOT_DRAIN_WAIT;
}
