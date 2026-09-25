#include "App_Settings.h"
/* Feature-owned immutable settings descriptors. Bounds apply equally to UI
 * and public requests; changing focus never changes a device. */
static const SettingItem items[]={
 {.key=SK_MODE,.name="Brightness mode",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_BRIGHTNESS,.name="Brightness",.kind=SETTING_NUMBER,.min=5,.max=100,.step=5},
 {.key=SK_BIAS,.name="Auto offset",.kind=SETTING_NUMBER,.min=-2,.max=2,.step=1},
 {.key=SK_DASH_BIAS,.name="Dashboard light offset",.kind=SETTING_NUMBER,.min=-5,.max=5,.step=1},
 {.key=SK_WALLPAPER,.name="Wallpaper",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_PHOTO,.name="Photo",.kind=SETTING_CHOICE,.min=0,.max=2,.step=1},
 {.key=SK_OFF_PHOTO,.name="Key OFF photo",.kind=SETTING_CHOICE,.min=0,.max=3,.step=1},
 {.key=SK_CENTER,.name="Center brightness",.kind=SETTING_NUMBER,.min=0,.max=100,.step=5},
 {.key=SK_THEME,.name="Theme",.kind=SETTING_CHOICE,.min=0,.max=2,.step=1},
 {.key=SK_THEME_SOURCE,.name="Auto theme source",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_THEME_DAY,.name="Light starts",.kind=SETTING_NUMBER,.min=0,.max=1439,.step=15},
 {.key=SK_THEME_NIGHT,.name="Dark starts",.kind=SETTING_NUMBER,.min=0,.max=1439,.step=15},
 {.key=SK_THEME_DARK_LUX,.name="Dark threshold",.kind=SETTING_NUMBER,.min=0,.max=9999,.step=10},
 {.key=SK_THEME_LIGHT_LUX,.name="Light threshold",.kind=SETTING_NUMBER,.min=1,.max=10000,.step=10},
 {.key=SK_AUTO_STATUS,.name="Ambient sensor",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
 {.key=SK_SENSOR_RETRY,.name="Retry ambient sensor",.kind=SETTING_ACTION,.min=1,.max=1,.step=1},
};
const SettingItem *SettingsDisplay_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}

#include "Graphics.h"
#include "Wallpaper.h"
#include "AmbientService.h"
#include "DashService.h"
#include "BacklightPolicy.h"
#include <stdio.h>
/* UI-owner finite requests. Automatic PWM is processed separately and never
 * writes calibration or substitutes a fabricated sensor sample. */
uint32_t SettingsDisplay_Apply(uint32_t key,int32_t value)
{
    AppSettings *s=g_app_settings;if(!s)return APP_SETTINGS_UNAVAILABLE;
    if(key==SK_MODE||key==SK_BRIGHTNESS){
        uint32_t apply=key==SK_MODE?!value:!s->values[SK_MODE];
        uint32_t percent=key==SK_BRIGHTNESS?(uint32_t)value:(uint32_t)s->values[SK_BRIGHTNESS];
        if(apply){if(Graphics_SetBrightnessPercent(percent))return APP_SETTINGS_DEVICE_ERROR;s->effective_brightness=percent;}
    }else if(key==SK_WALLPAPER){if(!Wallpaper_SetEnabled(value))return APP_SETTINGS_ARGUMENT;}
    else if(key==SK_PHOTO){if(!Wallpaper_SelectPhoto(value))return APP_SETTINGS_UNAVAILABLE;}
    else if(key==SK_CENTER){if(!Wallpaper_SetCenterBrightness(value))return APP_SETTINGS_ARGUMENT;}
    else if(key==SK_OFF_PHOTO){if(!Wallpaper_SetOffPhoto(value))return APP_SETTINGS_ARGUMENT;}
    return APP_SETTINGS_OK;
}

/* Calibrated sensor levels drive the local CFW brightness policy.
 * CFW adds dwell/bias/slew; active=false cannot relight a sleeping panel. */
void SettingsDisplay_Tick(uint32_t now,uint32_t active)
{
    static BacklightPolicy policy;
    AppSettings *s=g_app_settings;
    if(!s)return;
    DashService_SetLightBias(s->values[SK_DASH_BIAS]);
    Dash_Snapshot d;DashService_GetSnapshot(&d);
    uint32_t valid=d.calibration_valid&&d.light_source==DASH_LIGHT_LIVE&&
        now-d.light_sample_ms<=AMBIENT_SAMPLE_STALE_MS;
    uint32_t value=BacklightPolicy_Step(&policy,now,active,s->values[SK_MODE],valid,d.raw_light_index,
        s->values[SK_BIAS],s->values[SK_BRIGHTNESS],s->effective_brightness);
    if(value!=s->effective_brightness&&!Graphics_SetBrightnessPercent(value))s->effective_brightness=value;
}
void SettingsDisplay_SensorText(char *out,uint32_t size)
{
    Ambient_Snapshot a;AmbientService_GetSnapshot(&a);
    if(a.driver.valid&&!a.stale)snprintf(out,size,"%lu lux",(unsigned long)(a.driver.millilux/1000U));
    else if(a.pending_id)snprintf(out,size,"Checking sensor...");
    else snprintf(out,size,"Sensor error %lu / manual",(unsigned long)a.driver.error);
}
/* Explicit normal probe; existing worker owns I2C and bounded retries. No
 * diagnostic pull-up experiment, supply GPIO or bus access from the UI. */
uint32_t SettingsDisplay_Probe(uint32_t now)
{
    static uint32_t id,started;
    if(!id){if(AmbientService_RequestProbe(BSP_AMBIENT_DEFAULT_HZ,&id))return APP_SETTINGS_DEVICE_ERROR;
        started=now;return APP_SETTINGS_BUSY;}
    Ambient_Snapshot a;AmbientService_GetSnapshot(&a);
    if(a.completed_id!=id&&now-started<5000U)return APP_SETTINGS_BUSY;
    uint32_t success=a.completed_id==id&&!a.completed_result&&a.driver.ready;id=0;
    return success?APP_SETTINGS_OK:APP_SETTINGS_DEVICE_ERROR;
}
