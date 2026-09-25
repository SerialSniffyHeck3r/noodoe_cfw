#include "App_Settings.h"
/* Feature-owned immutable settings descriptors. Bounds apply equally to UI
 * and public requests; changing focus never changes a device. */
static const SettingItem items[]={
 {.key=SK_DATE,.name="Date",.kind=SETTING_DATE,.min=20000101,.max=20991231,.step=1},
 {.key=SK_TIME,.name="Time",.kind=SETTING_TIME,.min=0,.max=2359,.step=1},
 {.key=SK_ZONE,.name="UTC offset",.kind=SETTING_NUMBER,.min=-720,.max=840,.step=15},
};
const SettingItem *SettingsTime_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}

#include "ClockService.h"
/* Date/time editors show local time; the physical RTC always stores UTC.
 * Invalid civil dates and timezone conversions beyond2000..2099 are rejected. */
uint32_t SettingsTime_Request(uint32_t key,int32_t value,uint32_t *id)
{
    AppSettings *s=g_app_settings;if(!s)return APP_SETTINGS_UNAVAILABLE;
    BSP_CalendarDateTime local={2000,1,2,0,0,0,0},utc;
    if(s->facts.clock_valid&&!BSP_Calendar_AddSeconds(&s->facts.utc,s->values[SK_ZONE]*60,&local))return APP_SETTINGS_ARGUMENT;
    if(key==SK_DATE){local.year=value/10000;local.month=value/100%100;local.day=value%100;}
    else if(key==SK_TIME){local.hour=value/100;local.minute=value%100;local.second=0;}
    else return APP_SETTINGS_ARGUMENT;
    if(!BSP_Calendar_Normalize(&local)||!BSP_Calendar_AddSeconds(&local,-s->values[SK_ZONE]*60,&utc)||utc.year<2000||utc.year>2099)return APP_SETTINGS_ARGUMENT;
    BSP_Clock_Time t={utc.year,utc.month,utc.day,utc.weekday,utc.hour,utc.minute,utc.second,1};
    uint32_t result=ClockService_Request(&t,id);
    return result==BSP_CLOCK_BUSY?APP_SETTINGS_BUSY:result?APP_SETTINGS_DEVICE_ERROR:APP_SETTINGS_OK;
}
uint32_t SettingsTime_Result(uint32_t id,uint32_t *result)
{
    uint32_t status;if(!result||!ClockService_Result(id,&status))return 0;
    *result=status?APP_SETTINGS_DEVICE_ERROR:APP_SETTINGS_OK;return 1;
}
