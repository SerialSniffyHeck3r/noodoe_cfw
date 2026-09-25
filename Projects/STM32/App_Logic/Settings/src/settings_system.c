#include "App_Settings.h"
/* Feature-owned immutable settings descriptors. Bounds apply equally to UI
 * and public requests; changing focus never changes a device. */
static uint32_t up_count;
static const SettingItem items[]={
 {.key=SK_VERSION,.name="Firmware",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SK_LANGUAGE,.name="Language",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SK_DEFAULTS,.name="Reset display",.kind=SETTING_CONFIRM,.min=0,.max=1,.step=1},
 {.key=SK_REBOOT,.name="Restart Noodoe",.kind=SETTING_CONFIRM,.min=0,.max=1,.step=1},
 {.key=10,.name="Power sequence",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},
 {.key=SK_DEV_MESSAGE,.name="See dev message",.kind=SETTING_MESSAGE,.step=1},
};
const SettingItem *SettingsSystem_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0])-(up_count<20);return items;}

void SettingsSystem_Enter(void){if(up_count<20)up_count=0;}
void SettingsSystem_Up(void){if(up_count<20)++up_count;}
#if NOODOE_PRODUCT
#include "App_Persistence.h"
#include "Config_Store.h"

#include "BootStore.h"
#include "stm32f4xx_hal.h"
static volatile uint32_t restart_pending;
static uint32_t restart_at,checkpoint;
/* UI submits intent. The storage owner alone resets, after durable settings
 * and ride checkpoints. A failed save aborts this restart instead of losing it. */
uint32_t SettingsSystem_Restart(uint32_t now)
{
 if(restart_pending==2){restart_pending=0;return APP_SETTINGS_DEVICE_ERROR;}
 if(!restart_pending){GateJournalRecord j;
  if(!BootStore_GetBootInfo(&j)||j.state!=GATE_J_CONFIRMED||j.flags&(GATE_F_TRIAL|GATE_F_RESET_PENDING))return APP_SETTINGS_UNAVAILABLE;
  if(RideStore_RequestCheckpoint(&checkpoint))return APP_SETTINGS_DEVICE_ERROR;
  restart_at=now;__atomic_store_n(&restart_pending,1,__ATOMIC_RELEASE);
 }
 if(now-restart_at>10000U){restart_pending=0;return APP_SETTINGS_DEVICE_ERROR;}
 return APP_SETTINGS_BUSY;
}
/* Called only at StorageTask's existing safe steady-write boundary. */
void SettingsSystem_Process(uint32_t now)
{
 if(__atomic_load_n(&restart_pending,__ATOMIC_ACQUIRE)!=1)return;
 uint32_t ride=RideStore_GetResult(checkpoint),cfg=ConfigStore_Result(g_config_store.revision);
 if(ride==CFW_PENDING||cfg==CFW_PENDING)return;
 if(ride||cfg){restart_pending=2;return;}
 if(now-restart_at>=1000U)NVIC_SystemReset();
}
#else
uint32_t SettingsSystem_Restart(uint32_t now){(void)now;return APP_SETTINGS_UNAVAILABLE;}
#endif

