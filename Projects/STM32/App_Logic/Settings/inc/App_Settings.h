#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H
#include <stdint.h>
#include "BSP_Calendar.h"
#include "Ui_Dashboard.h"
typedef enum {SK_NONE,SK_MODE,SK_BRIGHTNESS,SK_BIAS,SK_WALLPAPER,SK_PHOTO,SK_CENTER,
 SK_DATE,SK_TIME,SK_ZONE,SK_UNITS,SK_SCALE,SK_MODEL,
 SK_OIL_DISTANCE,SK_OIL_HOURS,SK_OIL_DAYS,SK_OIL_RESET,
 SK_BELT_DISTANCE,SK_BELT_HOURS,SK_BELT_DAYS,SK_BELT_RESET,
 SK_SERV_DISTANCE,SK_SERV_HOURS,SK_SERV_DAYS,SK_SERV_RESET,
 SK_PHONE,SK_PHONE1,SK_PHONE2,SK_OBD,SK_PAIR,SK_VERSION,SK_LANGUAGE,SK_DEFAULTS,SK_POWER,SK_AUTO_STATUS,SK_BT_HOLD,SK_TRIP_STOP_SPEED,SK_OFF_DISPLAY,SK_OFF_BT,SK_OFF_DEEP,SK_OFF_FINAL_WAIT,SK_THEME,SK_THEME_SOURCE,SK_THEME_DAY,SK_THEME_NIGHT,SK_THEME_DARK_LUX,SK_THEME_LIGHT_LUX,
 SK_BT_STATUS,SK_BT_REPAIR,SK_BT_DISCONNECT,SK_BT_CLOSE,SK_BT_RESTART,SK_SENSOR_RETRY,SK_OFF_PHOTO,SK_DASH_BIAS,SK_REBOOT,SK_NOTIFICATION_PREVIEW,SK_NOTIFICATION_SECONDS,SK_COUNT} SettingKey;
typedef enum {SETTING_NUMBER,SETTING_CHOICE,SETTING_READONLY,SETTING_DISABLED,SETTING_CONFIRM,SETTING_SUBMENU,SETTING_DATE,SETTING_TIME,SETTING_ACTION,SETTING_MESSAGE} SettingKind;
/* Internal immutable descriptor, never serialized. All catalog IDs fit 16
 * bits, kinds and increments fit 8 bits; min/max stay signed 32-bit. */
typedef struct {uint16_t key;uint8_t kind,step;const char *name;int32_t min,max;} SettingItem;
_Static_assert(sizeof(void*)!=4||sizeof(SettingItem)==16,"setting descriptor layout");
typedef struct {uint32_t clock_valid,odo_valid,odo_km,links,photo_mask,phone,on_valid;uint64_t on_ms;BSP_CalendarDateTime utc;} SettingsFacts;
typedef struct {uint32_t valid,odo_km,day;uint64_t on_ms;} SettingsServiceOrigin;
typedef struct {uint32_t magic,version;int32_t values[SK_COUNT];SettingsFacts facts;
 SettingsServiceOrigin origins[3];uint32_t next_id,pending_id,pending_key,completed_id,result;
 int32_t pending_value;uint32_t clock_id,started_ms,effective_brightness;
} AppSettings;
extern AppSettings *g_app_settings;
enum {APP_SETTINGS_OK,APP_SETTINGS_BUSY,APP_SETTINGS_ARGUMENT,APP_SETTINGS_UNAVAILABLE,APP_SETTINGS_DEVICE_ERROR,APP_SETTINGS_LAST_STAGE};
/* Owner initialization uses caller-owned fixed lifetime RAM. Product delegates
 * persistence to App_Persistence/ConfigStore; other profiles remain RAM-only.
 * Requests are copied under a bounded critical section; Process alone applies
 * device/phone/wallpaper changes. GetResult reports application to RAM/device;
 * AppSettings_GetSaveResult in App_Persistence.h reports durable completion. */
void AppSettings_Init(AppSettings *s);
uint32_t AppSettings_RequestChange(uint32_t key,int32_t value,uint32_t *id);
/* Remote mutations are rechecked at the owner-task apply boundary. */
void AppSettings_RemoteAllowed(uint32_t ready);
uint32_t AppSettings_RequestRemote(uint32_t key,int32_t value,uint32_t *id);
uint32_t AppSettings_GetResult(uint32_t id,uint32_t *result);
void AppSettings_GetSnapshot(AppSettings *out);
void AppSettings_Process(const SettingsFacts *facts,uint32_t now);
int32_t AppSettings_Value(uint32_t key);
void AppSettings_FormatValue(uint32_t key,int32_t value,char *out,uint32_t size);
void AppSettings_Format(uint32_t key,char *out,uint32_t size);
void AppSettings_Maintenance(UiDashboardDistances *d,UiDashboardMaintenance *m,uint32_t *due);
/* Immutable catalogs: menu0 root,1..6 categories,7..9 service editors,10 power. */
const SettingItem *SettingsCatalog_Items(uint32_t menu,uint32_t *count);
const char *SettingsCatalog_Title(uint32_t menu);
uint32_t SettingsCatalog_Parent(uint32_t menu);
const SettingItem *SettingsDebug_Items(uint32_t *n);
uint32_t SettingsDebug_Format(uint32_t key,char *out,uint32_t size);
/* Debug keys are read-only IDs outside the persisted settings array. */
enum {SD_BT=0x8000,SD_BT_CHIP,SD_BT_IO,SD_BT_QUEUE,SD_AMBIENT,SD_AMBIENT_RAW,SD_AMBIENT_ID,SD_AMBIENT_ERROR,SD_DASH,SD_DASH_IO,SD_DASH_LIGHT,SD_VEHICLE,SD_GPS,SD_POWER,SD_INPUT_MODE};
const SettingItem *SettingsPower_Items(uint32_t *n);
uint32_t SettingsPower_EnabledMask(void);
const SettingItem *SettingsCatalog_Find(uint32_t key);
const SettingItem *SettingsDisplay_Items(uint32_t *n);
const SettingItem *SettingsTime_Items(uint32_t *n);
const SettingItem *SettingsVehicle_Items(uint32_t *n);
const SettingItem *SettingsMaintenance_Items(uint32_t menu,uint32_t *n);
const SettingItem *SettingsConnections_Items(uint32_t *n);
const SettingItem *SettingsSystem_Items(uint32_t *n);
/* Local-only developer message unlock: fresh UP releases, not auto-repeat. */
void SettingsSystem_Up(void);
void SettingsSystem_Enter(void);
uint32_t SettingsSystem_Restart(uint32_t now);
void SettingsSystem_Process(uint32_t now);
#define SK_DEV_MESSAGE 0x8100U
/* Device bridge: UI never waits on RTC writes. Storage worker owns RTC IO. */
uint32_t SettingsDevice_Lock(void);
void SettingsDevice_Unlock(uint32_t state);
uint32_t SettingsDisplay_Apply(uint32_t key,int32_t value);
void SettingsDisplay_Tick(uint32_t now,uint32_t active);
void SettingsDisplay_SensorText(char *out,uint32_t size);
uint32_t SettingsDisplay_Probe(uint32_t now);
uint32_t SettingsTime_Request(uint32_t key,int32_t value,uint32_t *id);
uint32_t SettingsTime_Result(uint32_t id,uint32_t *result);
uint32_t SettingsConnections_Apply(uint32_t key,int32_t value,uint32_t now);
uint32_t SettingsConnections_Format(uint32_t key,char *out,uint32_t size);
static inline uint32_t SettingsConnections_IsAction(uint32_t key)
{return key==SK_PAIR||(key>=SK_BT_REPAIR&&key<=SK_BT_RESTART);}
#endif
