#include "App_Settings.h"
#include "Trip_Computer.h"
#if NOODOE_PRODUCT
#include "App_Persistence.h"
#endif
#include <string.h>
#include <stdio.h>
AppSettings *g_app_settings;
static struct {uint32_t id,result;} completed[8];
static uint32_t completed_head,remote_ready,pending_remote;
void SettingsMaintenance_Reset(uint32_t index);
/* Session values deliberately do not call SettingsService's NOR APIs. The
 * state is allocated once by the product owner and can be supplied by tests. */
void AppSettings_Init(AppSettings *s)
{
    if(!s)return;
    memset(s,0,sizeof(*s));memset(completed,0,sizeof(completed));completed_head=remote_ready=pending_remote=0;g_app_settings=s;s->magic=0x53544531;s->version=2;
    s->values[SK_BRIGHTNESS]=25;s->effective_brightness=25;s->values[SK_WALLPAPER]=1;
    s->values[SK_CENTER]=100;s->values[SK_ZONE]=540;s->values[SK_SCALE]=200;
    s->values[SK_THEME_DAY]=420;s->values[SK_THEME_NIGHT]=1140;
    s->values[SK_THEME_DARK_LUX]=100;s->values[SK_THEME_LIGHT_LUX]=300;
    s->values[SK_NOTIFICATION_SECONDS]=5;s->values[SK_POWER]=60;s->values[SK_BT_HOLD]=60;
    s->values[SK_OFF_DISPLAY]=s->values[SK_OFF_BT]=s->values[SK_OFF_DEEP]=1;
    s->values[SK_TRIP_STOP_SPEED]=TRIP_STOP_SPEED_DEFAULT_KPH;
}
uint32_t AppSettings_RequestChange(uint32_t key,int32_t value,uint32_t *id)
{
    const SettingItem *item=SettingsCatalog_Find(key);if(!id||!g_app_settings||!item)return APP_SETTINGS_ARGUMENT;
#if NOODOE_PRODUCT
    if(!AppPersistence_SettingsReady())return APP_SETTINGS_BUSY;
#endif
    if(item->kind==SETTING_READONLY||item->kind==SETTING_DISABLED||item->kind==SETTING_MESSAGE)return APP_SETTINGS_UNAVAILABLE;
    if(value<item->min||value>item->max)return APP_SETTINGS_ARGUMENT;
    if((key==SK_THEME_DARK_LUX&&value>=AppSettings_Value(SK_THEME_LIGHT_LUX))||
       (key==SK_THEME_LIGHT_LUX&&value<=AppSettings_Value(SK_THEME_DARK_LUX)))return APP_SETTINGS_ARGUMENT;
    if((item->kind==SETTING_CONFIRM||item->kind==SETTING_ACTION)&&value!=1)return APP_SETTINGS_ARGUMENT;
    if(key==SK_PHOTO&&!(g_app_settings->facts.photo_mask&(1U<<value)))return APP_SETTINGS_UNAVAILABLE;
    uint32_t mask=SettingsDevice_Lock();AppSettings *s=g_app_settings;
    if(s->pending_id){SettingsDevice_Unlock(mask);return APP_SETTINGS_BUSY;}
    /* Reject an empty sequence while still holding the request lock. */
    if(key>=SK_OFF_DISPLAY&&key<=SK_OFF_DEEP&&!value){
        uint32_t others=0;
        for(uint32_t k=SK_OFF_DISPLAY;k<=SK_OFF_DEEP;++k)if(k!=key&&s->values[k])++others;
        if(!others){SettingsDevice_Unlock(mask);return APP_SETTINGS_LAST_STAGE;}
    }
    if(!++s->next_id)++s->next_id;
    s->pending_key=key;s->pending_value=value;s->pending_id=s->next_id;s->clock_id=0;
    *id=s->pending_id;SettingsDevice_Unlock(mask);return APP_SETTINGS_OK;
}
void AppSettings_RemoteAllowed(uint32_t ready){__atomic_store_n(&remote_ready,ready,__ATOMIC_RELEASE);}
uint32_t AppSettings_RequestRemote(uint32_t key,int32_t value,uint32_t *id)
{
 uint32_t m=SettingsDevice_Lock(),r=APP_SETTINGS_BUSY;
 if(__atomic_load_n(&remote_ready,__ATOMIC_ACQUIRE)){r=AppSettings_RequestChange(key,value,id);if(!r)pending_remote=*id;}
 SettingsDevice_Unlock(m);return r;
}
uint32_t AppSettings_GetResult(uint32_t id,uint32_t *out)
{if(!g_app_settings||!out||!id)return 0;uint32_t m=SettingsDevice_Lock(),done=0;for(uint32_t i=0;i<8;i++)if(completed[i].id==id){*out=completed[i].result;done=1;}SettingsDevice_Unlock(m);return done;}
void AppSettings_GetSnapshot(AppSettings *out)
{if(!out||!g_app_settings)return;uint32_t m=SettingsDevice_Lock();*out=*g_app_settings;SettingsDevice_Unlock(m);}
int32_t AppSettings_Value(uint32_t key)
{
    AppSettings *s=g_app_settings;if(!s||key>=SK_COUNT)return 0;
    if(key==SK_DATE||key==SK_TIME){BSP_CalendarDateTime local;
        if(s->facts.clock_valid&&BSP_Calendar_AddSeconds(&s->facts.utc,s->values[SK_ZONE]*60,&local))
            return key==SK_DATE?(int32_t)(local.year*10000+local.month*100+local.day):(int32_t)(local.hour*100+local.minute);
        return key==SK_DATE?20000102:0;
    }if(key==SK_PHONE)return s->facts.phone;return s->values[key];
}
/* Process is UI-owned. RTC submission returns immediately and completion is
 * polled; a timeout does not claim rollback of a partially written RTC. */
void AppSettings_Process(const SettingsFacts *facts,uint32_t now)
{
    AppSettings *s=g_app_settings;if(!s)return;if(facts)s->facts=*facts;
    if(!s->pending_id)return;
    uint32_t key=s->pending_key,result=0;int32_t value=s->pending_value;
    if(pending_remote==s->pending_id){pending_remote=0;
        if(!__atomic_load_n(&remote_ready,__ATOMIC_ACQUIRE)){result=APP_SETTINGS_BUSY;goto complete;}}
    if(key==SK_REBOOT){result=SettingsSystem_Restart(now);if(result==APP_SETTINGS_BUSY)return;}
    else if(key==SK_DATE||key==SK_TIME){
        if(!s->clock_id){result=SettingsTime_Request(key,value,&s->clock_id);s->started_ms=now;if(!result)return;}
        else if(!SettingsTime_Result(s->clock_id,&result)){if(now-s->started_ms<5000U)return;result=APP_SETTINGS_DEVICE_ERROR;}
    }else if(key<=SK_CENTER||key==SK_OFF_PHOTO)result=SettingsDisplay_Apply(key,value);
    else if(SettingsConnections_IsAction(key)){
        result=SettingsConnections_Apply(key,value,now);
        if(result==APP_SETTINGS_BUSY)return;
    }
    else if(key==SK_SENSOR_RETRY){result=SettingsDisplay_Probe(now);if(result==APP_SETTINGS_BUSY)return;}
    else if(key==SK_DEFAULTS){
        result=SettingsDisplay_Apply(SK_MODE,0);if(!result){s->values[SK_THEME]=s->values[SK_THEME_SOURCE]=0;
            s->values[SK_THEME_DAY]=420;s->values[SK_THEME_NIGHT]=1140;s->values[SK_THEME_DARK_LUX]=100;s->values[SK_THEME_LIGHT_LUX]=300;
            s->values[SK_MODE]=0;result=SettingsDisplay_Apply(SK_BRIGHTNESS,25);}
        if(!result){s->values[SK_MODE]=s->values[SK_BIAS]=s->values[SK_PHOTO]=s->values[SK_DASH_BIAS]=0;
            s->values[SK_TRIP_STOP_SPEED]=TRIP_STOP_SPEED_DEFAULT_KPH;
            s->values[SK_NOTIFICATION_PREVIEW]=0;s->values[SK_NOTIFICATION_SECONDS]=5;
            s->values[SK_BRIGHTNESS]=25;s->values[SK_CENTER]=100;s->values[SK_WALLPAPER]=1;
            (void)SettingsDisplay_Apply(SK_PHOTO,0);
            s->values[SK_OFF_PHOTO]=0;(void)SettingsDisplay_Apply(SK_OFF_PHOTO,0);
            (void)SettingsDisplay_Apply(SK_CENTER,100);(void)SettingsDisplay_Apply(SK_WALLPAPER,1);}
    }else if(key==SK_OIL_RESET||key==SK_BELT_RESET||key==SK_SERV_RESET)SettingsMaintenance_Reset((key-SK_OIL_RESET)/4U);
complete:
    if(!result&&key<SK_COUNT)s->values[key]=value;
#if NOODOE_PRODUCT
    if(!SettingsConnections_IsAction(key)&&key!=SK_SENSOR_RETRY&&key!=SK_REBOOT)AppPersistence_SettingsApplied(s->pending_id,key,result);
#endif
    uint32_t mask=SettingsDevice_Lock();s->result=result;s->completed_id=s->pending_id;uint32_t at=completed_head++%8;completed[at].id=s->pending_id;completed[at].result=result;s->pending_id=0;SettingsDevice_Unlock(mask);
}
/* Bounded display formatting. Numeric values do not get leading zeroes,
 * except date/time fields; all geometry is owned by the renderer. */
void AppSettings_FormatValue(uint32_t key,int32_t v,char *out,uint32_t size)
{
    if(!out||!size||!g_app_settings)return;
    if(SettingsDebug_Format(key,out,size)||SettingsConnections_Format(key,out,size))return;
    AppSettings *s=g_app_settings;const char *text=0;
    switch(key){
    case SK_OFF_PHOTO:if(!v){snprintf(out,size,"Same as riding");return;}
        snprintf(out,size,"Photo %ld%s",(long)v-1,(s->facts.photo_mask&(1U<<(v-1)))?"":" / fallback");return;
    case SK_TRIP_STOP_SPEED:snprintf(out,size,v?"< %ld km/h":"%ld km/h only",(long)v);return;
    case SK_THEME:text=v==2?"Auto":v?"Light":"Dark";break;
    case SK_THEME_SOURCE:text=v?"Time schedule":"Ambient sensor";break;
    case SK_THEME_DAY:case SK_THEME_NIGHT:snprintf(out,size,"%02ld:%02ld",(long)v/60,(long)v%60);return;
    case SK_THEME_DARK_LUX:case SK_THEME_LIGHT_LUX:snprintf(out,size,"%ld lux",(long)v);return;
    case SK_NOTIFICATION_PREVIEW:text=v?"On":"Off";break;
    case SK_NOTIFICATION_SECONDS:snprintf(out,size,"%ld s",(long)v);return;
    case SK_MODE:text=v?"Auto":"Manual";break;case SK_WALLPAPER:text=v?"On":"Off";break;
    case SK_UNITS:text=v?"mi / mph":"km / km/h";break;case SK_MODEL:text="Dashboard UART";break;
    case SK_DEV_MESSAGE:text="this is easter egg!";break;
    case SK_VERSION:text="Noodoe CFW / Settings v1";break;case SK_LANGUAGE:text="English";break;
    case SK_POWER:case SK_BT_HOLD:snprintf(out,size,"%ld min",(long)v);return;
    case SK_OFF_DISPLAY:case SK_OFF_BT:case SK_OFF_DEEP:text=v?"Enabled":"Skipped";break;
    case SK_OFF_FINAL_WAIT:text="Until IGN ON";break;
    case SK_AUTO_STATUS:SettingsDisplay_SensorText(out,size);return;
    case SK_SENSOR_RETRY:text="Check sensor connection";break;
    case SK_REBOOT:case SK_DEFAULTS:case SK_OIL_RESET:case SK_BELT_RESET:case SK_SERV_RESET:text="Confirm to apply";break;
    default:break;}
    if(text){snprintf(out,size,"%s",text);return;}
    if(key==SK_BIAS){snprintf(out,size,"%s%ld",v>0?"+":"",(long)v);return;}
    if(key==SK_ZONE){snprintf(out,size,"UTC%c%02ld:%02ld",v<0?'-':'+',(long)(v<0?-v:v)/60,(long)(v<0?-v:v)%60);return;}
    if(key==SK_DATE){snprintf(out,size,"%04ld-%02ld-%02ld",(long)v/10000,(long)v/100%100,(long)v%100);return;}
    if(key==SK_TIME){snprintf(out,size,"%02ld:%02ld",(long)v/100,(long)v%100);return;}
    if(key==SK_PHOTO){snprintf(out,size,"Photo %ld%s",(long)v+1,(s->facts.photo_mask&(1U<<v))?"":" / empty");return;}
    if(key==SK_PHONE){snprintf(out,size,"Phone %ld",(long)v+1);return;}
    const char *unit="";
    if(key==SK_BRIGHTNESS||key==SK_CENTER)unit="%";else if(key==SK_SCALE)unit=" km/h";
    else if(key>=SK_OIL_DISTANCE&&key<=SK_SERV_RESET){uint32_t c=(key-SK_OIL_DISTANCE)%4;
        if(!v){snprintf(out,size,"Off");return;}unit=c==0?" km":c==1?" h":" days";}
    snprintf(out,size,"%ld%s",(long)v,unit);
}

void AppSettings_Format(uint32_t key,char *out,uint32_t size)
{AppSettings_FormatValue(key,AppSettings_Value(key),out,size);}
