#include "App_Persistence.h"
#include "App_Settings.h"
#include "Config_Store.h"
#include "OilUsageService.h"
#include "BSP_RAM.h"
#include "stm32f4xx_hal.h"
#include "Popup_Notifications.h"
#include <string.h>
#define RIDE_MAGIC 0x31444952U
#define RIDE_BYTES 304U
/* Disk IDs never follow SettingKey ordinal values. Deleted IDs are reserved. */
static const uint16_t fields[][2]= {
    {0x1070,SK_NOTIFICATION_PREVIEW},{0x1071,SK_NOTIFICATION_SECONDS},
    {0x1007,SK_OFF_PHOTO},{0x1008,SK_DASH_BIAS},
    {0x1060,SK_THEME},{0x1061,SK_THEME_SOURCE},{0x1062,SK_THEME_DAY},
    {0x1063,SK_THEME_NIGHT},{0x1064,SK_THEME_DARK_LUX},{0x1065,SK_THEME_LIGHT_LUX},
    {
        0x1001,SK_MODE
    }, {
        0x1002,SK_BRIGHTNESS
    }, {
        0x1003,SK_BIAS
    }, {
        0x1004,SK_WALLPAPER
    },
    {
        0x1005,SK_PHOTO
    }, {
        0x1006,SK_CENTER
    }, {
        0x1010,SK_ZONE
    }, {
        0x1011,SK_UNITS
    },
    {
        0x1012,SK_SCALE
    }, {
        0x1013,SK_MODEL
    }, {
        0x1014,SK_TRIP_STOP_SPEED
    }, {
        0x1015,SK_LANGUAGE
    },
    {
        0x1020,SK_POWER
    }, {
        0x1021,SK_BT_HOLD
    }, {
        0x1022,SK_OFF_DISPLAY
    }, {
        0x1023,SK_OFF_BT
    }, {
        0x1024,SK_OFF_DEEP
    },
    {
        0x1030,SK_OIL_DISTANCE
    }, {
        0x1031,SK_OIL_HOURS
    }, {
        0x1032,SK_OIL_DAYS
    },
    {
        0x1040,SK_BELT_DISTANCE
    }, {
        0x1041,SK_BELT_HOURS
    }, {
        0x1042,SK_BELT_DAYS
    },
    {
        0x1050,SK_SERV_DISTANCE
    }, {
        0x1051,SK_SERV_HOURS
    }, {
        0x1052,SK_SERV_DAYS
    }
};
/* One stable ID catalog serves persistence and the companion wire ABI. */
uint32_t AppPersistence_FieldKey(uint32_t field)
{for(uint32_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++)if(fields[i][0]==field)return fields[i][1];return 0;}
uint32_t AppPersistence_FieldAt(uint32_t index)
{return index<sizeof(fields)/sizeof(fields[0])?fields[index][0]:0;}
uint32_t AppPersistence_FieldCount(void){return sizeof(fields)/sizeof(fields[0]);}
typedef struct  {
    uint8_t current[RIDE_BYTES],saved[RIDE_BYTES],pending[RIDE_BYTES];
    uint32_t load_done,ui_done,request,attempt_ms,started_ms,last_tick,dirty_since;
    uint32_t next_token,completed_token,pending_token,result,force_token,snapshot_token,reported_error;
    struct {
        uint32_t id,result;
    } results[8];
    uint32_t result_head;
    struct {
        uint32_t id,revision,result,ride;
    } settings[8];
    uint32_t settings_head;
    int32_t config_cache[sizeof(fields)/sizeof(fields[0])];
    uint64_t previous_trip;
    uint32_t previous_reserve,config_error;
} Persistence;
static Persistence *p;
volatile AppPersistenceStatus g_app_persistence;
static uint32_t Lock(void) {
    uint32_t m=__get_PRIMASK();
    __disable_irq();
    return m;
}
static void Unlock(uint32_t m) {
    __DMB();
    __set_PRIMASK(m);
}
static void P64(uint8_t *b,uint64_t v) {
    memcpy(b,&v,sizeof(v));
}
static uint64_t G64(const uint8_t *b) {
    uint64_t v;memcpy(&v,b,sizeof(v));return v;
}
static uint64_t Add(uint64_t a,uint64_t b) {
    return UINT64_MAX-a<b?UINT64_MAX:a+b;
}
static void Init(uint32_t now)
{
    if(p)return;
    Persistence *created=BSP_RAM_AllocateNamed(BSP_RAM_CFW_APP,sizeof(*p));
    if(!created)return;
    memset(created,0,sizeof(*created));
    created->started_ms=now;
    g_app_persistence.magic=0x31535041;
    __DMB();
    p=created;
}
static uint32_t Check(const uint8_t *b,uint32_t n)
{
    if(n!=RIDE_BYTES||Cfw_Get32(b)!=RIDE_MAGIC)return CFW_CORRUPT;
    if(Cfw_Get32(b+4)!=1)return CFW_VERSION;
    for(uint32_t i=0;i<4;++i) {
        const uint8_t *r=b+16+i*44;
        if(Cfw_Get32(r+32)>400||Cfw_Get32(r+36)>1||Cfw_Get32(r+40)>1)return CFW_CORRUPT;
    }
    for(uint32_t i=0;i<3;++i)if(Cfw_Get32(b+192+i*20)>7)return CFW_CORRUPT;
    if(Cfw_Get32(b+268)>1||Cfw_Get32(b+280)>1||Cfw_Get32(b+284)>5||Cfw_Get32(b+296)>1)return CFW_CORRUPT;
    return CFW_OK;
}
void AppPersistence_Process(uint32_t now)
{
    Init(now);
    if(!p)return;
    if(!p->load_done) {
        if(!CfwStore_Ready())return;
        uint32_t n=0,e=CfwStore_Read(CFW_RIDE,p->saved,RIDE_BYTES,&n);
        if(!e&&n)e=Check(p->saved,n);
        if(!e&&!n) {
            memset(p->saved,0,RIDE_BYTES);
            Cfw_Put32(p->saved,RIDE_MAGIC);
            Cfw_Put32(p->saved+4,1);
            Cfw_Put32(p->saved+296,1);
        }
        uint32_t m=Lock();
        g_app_persistence.ride_status=e;
        g_app_persistence.base_ign_ms=e?0:G64(p->saved+256);
        g_app_persistence.saved_ign_ms=g_app_persistence.base_ign_ms;
        g_app_persistence.ready=1;
        p->load_done=1;
        Unlock(m);
        return;
    }
    if(p->request) {
        uint32_t e=CfwStore_Result(p->request);
        if(e==CFW_PENDING)return;
        uint32_t m=Lock();
        p->request=0;
        g_app_persistence.saving=0;
        p->result=e;
        for(uint32_t i=0;i<8;++i)if(p->results[i].id&&p->results[i].result==CFW_PENDING&&
        (int32_t)(p->pending_token-p->results[i].id)>=0)p->results[i].result=e;
        if(!e) {
            memcpy(p->saved,p->pending,RIDE_BYTES);
            g_app_persistence.saved_ign_ms=G64(p->saved+256);
            g_app_persistence.last_success_ms=now;
            p->completed_token=p->pending_token;
            g_app_persistence.dirty=memcmp(p->current,p->saved,RIDE_BYTES)!=0;
        }
        else {
            ++g_app_persistence.failures;
            if(p->pending_token)g_app_persistence.force=1;
        }
        Unlock(m);
        return;
    }
    if(!p->ui_done||g_app_persistence.ride_status)return;
    uint32_t last=g_app_persistence.last_success_ms?g_app_persistence.last_success_ms:p->started_ms;
    /* A new dirty interval after sleep has not missed a checkpoint merely
     * because the preceding successful save was hours ago. */
    if((int32_t)(p->dirty_since-last)>0)last=p->dirty_since;
    g_app_persistence.overdue=g_app_persistence.dirty&&now-last>60000U;
    if(!g_app_persistence.dirty&&!g_app_persistence.force)return;
    if(g_app_persistence.force&&p->snapshot_token!=p->force_token)return;
    if(!g_app_persistence.force&&now-last<50000U)return;
    if(now-p->attempt_ms<1000U)return;
    p->attempt_ms=now;
    uint32_t m=Lock();
    memcpy(p->pending,p->current,RIDE_BYTES);
    p->pending_token=p->snapshot_token;
    uint32_t e=CfwStore_Request(CFW_RIDE,p->pending,RIDE_BYTES,&p->request);
    if(!e) {
        g_app_persistence.saving=1;
        g_app_persistence.request=p->request;
        g_app_persistence.force=0;
        p->result=0;
    }
    else {
        p->request=0;
        if(e!=CFW_BUSY&&e!=CFW_PENDING){p->result=e;++g_app_persistence.failures;}
    }
    Unlock(m);
}
uint32_t AppPersistence_Busy(void)
{
    return p&&(p->request||(!g_app_persistence.ride_status&&g_app_persistence.force));
}
uint32_t RideStore_RequestCheckpoint(uint32_t *id)
{
    if(!id||!p)return CFW_PENDING;
    if(g_app_persistence.ride_status)return g_app_persistence.ride_status;
    uint32_t m=Lock();
    if(!++p->next_token)++p->next_token;
    *id=p->force_token=p->next_token;
    g_app_persistence.force=1;
    p->results[p->result_head%8].id=*id;
    p->results[p->result_head++%8].result=CFW_PENDING;
    Unlock(m);
    return 0;
}
uint32_t RideStore_GetResult(uint32_t id)
{
    if(!p||!id)return CFW_ARGUMENT;
    uint32_t m=Lock(),result=CFW_EXPIRED;
    for(uint32_t i=0;i<8;++i)if(p->results[i].id==id) {
        result=p->results[i].result;
        break;
    }
    Unlock(m);
    return result;
}
uint32_t RideStore_GetUsage(uint64_t *base,uint64_t *committed,uint32_t *known)
{
    if(!base||!committed||!known||!g_app_persistence.ready)return 0;
    uint32_t m=Lock();
    *base=g_app_persistence.base_ign_ms;
    *committed=g_app_persistence.saved_ign_ms;
    *known=!g_app_persistence.ride_status&&Cfw_Get32(p->saved+296);
    Unlock(m);
    return 1;
}
static void ConfigUI(void)
{
    if(!g_app_settings||!g_config_store.ready||g_config_store.error)return;
    p->config_error=0;
    uint32_t first=!g_app_persistence.config_restored;
    if(first) {
        /* Validate the complete configuration before applying ANY value or
                 * filling absent defaults. A malformed late field cannot cause a
                 * partial restore followed by an automatic rewrite. */
        int32_t values[sizeof(fields)/sizeof(fields[0])];
        uint32_t stages=0;
        for(uint32_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            uint32_t key=fields[i][1],n=0;
            uint8_t b[4];
            uint32_t e=ConfigStore_Get(fields[i][0],b,4,&n);
            values[i]=g_app_settings->values[key];
            if(!e) {
                const SettingItem *item=SettingsCatalog_Find(key);
                int32_t v=(int32_t)Cfw_Get32(b);
                if(n!=4||!item||v<item->min||v>item->max) {
                    ConfigStore_Reject(CFW_CORRUPT);
                    return;
                }
                values[i]=v;
            }
            else if(e!=CFW_MISSING) {
                ConfigStore_Reject(CFW_CORRUPT);
                return;
            }
            if(key>=SK_OFF_DISPLAY&&key<=SK_OFF_DEEP)stages|=values[i];
        }
        if(!stages) {
            ConfigStore_Reject(CFW_CORRUPT);
            return;
        }
        for(uint32_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            g_app_settings->values[fields[i][1]]=values[i];
            p->config_cache[i]=INT32_MIN;
        }
    }
    for(uint32_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
        uint32_t id=fields[i][0],key=fields[i][1],n=0,revision;
        uint8_t b[4];
        int32_t v=g_app_settings->values[key];
        if(v!=p->config_cache[i]) {
            Cfw_Put32(b,(uint32_t)v);
            uint32_t e=ConfigStore_Set(id,b,4,&revision);
            if(!e)p->config_cache[i]=v;
            else p->config_error=e;
        }
        else if(!ConfigStore_Get(id,b,4,&n)&&n==4&&(int32_t)Cfw_Get32(b)!=v) {
            const SettingItem *item=SettingsCatalog_Find(key);
            int32_t next=(int32_t)Cfw_Get32(b);
            if(!item||next<item->min||next>item->max) {
                ConfigStore_Reject(CFW_CORRUPT);
                return;
            }
            g_app_settings->values[key]=p->config_cache[i]=next;
            if(key<=SK_CENTER||key==SK_OFF_PHOTO)(void)SettingsDisplay_Apply(key,next);
        }
    }
    if(first) {
        for(uint32_t k=SK_MODE;k<=SK_CENTER;++k)(void)SettingsDisplay_Apply(k,g_app_settings->values[k]);
        (void)SettingsDisplay_Apply(SK_OFF_PHOTO,g_app_settings->values[SK_OFF_PHOTO]);
        g_app_persistence.config_restored=1;
    }
}
uint32_t AppPersistence_SettingsReady(void)
{
    return g_config_store.ready&&(g_config_store.error||g_app_persistence.config_restored);
}
/* UI owner calls this after applying a request. Stable-field serialization
 * completes before publishing its revision; NOR work remains StorageTask's. */
void AppPersistence_SettingsApplied(uint32_t id,uint32_t key,uint32_t apply_result)
{
    if(!p||!id)return;
    uint32_t result=CFW_ARGUMENT,revision=0,ride=0;
    if(!apply_result) {
        if(key==SK_OIL_RESET||key==SK_BELT_RESET||key==SK_SERV_RESET) {
            result=RideStore_RequestCheckpoint(&revision);
            ride=1;
        }
        else {
            uint32_t supported=key==SK_DEFAULTS;
            for(uint32_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i)if(fields[i][1]==key)supported=1;
            if(supported) {
                ConfigUI();
                result=g_config_store.error?g_config_store.error:p->config_error;
                if(!result&&!g_app_persistence.config_restored)result=CFW_PENDING;
                revision=g_config_store.revision;
            }
        }
    }
    uint32_t m=Lock(),index=p->settings_head++%8;
    p->settings[index].id=id;
    p->settings[index].revision=revision;
    p->settings[index].result=result;
    p->settings[index].ride=ride;
    Unlock(m);
}
uint32_t AppSettings_GetSaveResult(uint32_t id)
{
    if(!p||!id)return CFW_ARGUMENT;
    uint32_t m=Lock(),result=CFW_EXPIRED;
    for(uint32_t i=0;i<8;++i)if(p->settings[i].id==id) {
        result=p->settings[i].result;
        if(!result)result=p->settings[i].ride?RideStore_GetResult(p->settings[i].revision):ConfigStore_Result(p->settings[i].revision);
        break;
    }
    Unlock(m);
    return result;
}
/* Restore only durable quantities. Current boot deltas are added once; old
 * tick/remainder/debounce/IGN edge state is never copied back. */
static void Restore(TripComputer *t,UiState *u)
{
    if(g_app_persistence.ride_status) {
        for(uint32_t i=0;i<4;++i) {
            t->records[i].partial=1;
            t->records[i].valid=0;
        }
        p->ui_done=1;
        return;
    }
    const uint8_t *b=p->saved;
    for(uint32_t i=0;i<4;++i) {
        TripRecord *r=&t->records[i];
        const uint8_t *q=b+16+i*44;
        if(i==TRIP_TODAY&&t->date&&t->date!=Cfw_Get32(b+8))continue;
        r->distance_mm=Add(r->distance_mm,G64(q));
        r->moving_ms=Add(r->moving_ms,G64(q+8));
        r->stopped_ms=Add(r->stopped_ms,G64(q+16));
        r->unknown_ms=Add(r->unknown_ms,G64(q+24));
        if(r->max_kph<Cfw_Get32(q+32))r->max_kph=Cfw_Get32(q+32);
        r->valid|=Cfw_Get32(q+36);
        r->partial|=Cfw_Get32(q+40);
    }
    if(!t->date)t->date=Cfw_Get32(b+8);
    for(uint32_t i=0;i<3;++i) {
        SettingsServiceOrigin *o=&g_app_settings->origins[i];
        const uint8_t *q=b+192+i*20;
        o->valid=Cfw_Get32(q);
        o->odo_km=Cfw_Get32(q+4);
        o->day=Cfw_Get32(q+8);
        o->on_ms=G64(q+12);
    }
    t->refuel_count=Cfw_Get32(b+276);
    t->fuel_known=Cfw_Get32(b+280);
    t->fuel_base=Cfw_Get32(b+284);
    u->reserve_active=Cfw_Get32(b+268);
    if(u->reserve_active)u->dashboard.footer=UI_RESV;
    g_app_persistence.reserve_mm=G64(b+288);
    p->previous_reserve=u->reserve_active;
    p->previous_trip=t->records[0].distance_mm;
    p->ui_done=1;
    g_app_persistence.ride_restored=1;
    ++g_app_persistence.restore_count;
}
void AppPersistence_UI(uint32_t now,TripComputer *t,UiState *u)
{
    if(!p||!t||!u||!g_app_settings)return;
    ConfigUI();
    uint32_t error=g_config_store.error?g_config_store.error:g_app_persistence.ride_status;
    if(!error&&(g_app_persistence.overdue||g_config_store.last_result))error=CFW_IO;
    if(error&&error!=p->reported_error&&u->power==IGN_ON) {
        if(ShowToastMessages(error==CFW_MISSING?"CFW storage not installed":"Storage error - history not saved",5))p->reported_error=error;
    }
    if(!p->load_done)return;
    if(!p->ui_done)Restore(t,u);
    if(g_app_persistence.ride_status) {
        /* Continuing telemetry must not turn lost history into a plausible
                 * zero-based cumulative trip. Retain observations but mark unknown. */
        for(uint32_t i=0;i<4;++i) {
            t->records[i].valid=0;
            t->records[i].partial=1;
        }
        return;
    }
    uint8_t b[RIDE_BYTES]= {
        0
    };
    Cfw_Put32(b,RIDE_MAGIC);
    Cfw_Put32(b+4,1);
    Cfw_Put32(b+8,t->date);
    for(uint32_t i=0;i<4;++i) {
        TripRecord *r=&t->records[i];
        uint8_t *q=b+16+i*44;
        P64(q,r->distance_mm);
        P64(q+8,r->moving_ms);
        P64(q+16,r->stopped_ms);
        P64(q+24,r->unknown_ms);
        Cfw_Put32(q+32,r->max_kph);
        Cfw_Put32(q+36,r->valid);
        Cfw_Put32(q+40,r->partial);
    }
    for(uint32_t i=0;i<3;++i) {
        SettingsServiceOrigin *o=&g_app_settings->origins[i];
        uint8_t *q=b+192+i*20;
        Cfw_Put32(q,o->valid);
        Cfw_Put32(q+4,o->odo_km);
        Cfw_Put32(q+8,o->day);
        P64(q+12,o->on_ms);
    }
    OilUsageSnapshot usage= {
        0
    };
    (void)OilUsageService_Get(&usage);
    P64(b+256,usage.total_ms);
    Cfw_Put32(b+296,usage.ready);
    Cfw_Put32(b+268,u->reserve_active);
    Cfw_Put32(b+276,t->refuel_count);
    Cfw_Put32(b+280,t->fuel_known);
    Cfw_Put32(b+284,t->fuel_base);
    uint64_t distance=t->records[0].distance_mm;
    if(u->reserve_active&&p->previous_reserve&&distance>=p->previous_trip)g_app_persistence.reserve_mm=Add(g_app_persistence.reserve_mm,distance-p->previous_trip);
    if(!u->reserve_active||!p->previous_reserve)g_app_persistence.reserve_mm=0;
    p->previous_trip=distance;
    p->previous_reserve=u->reserve_active;
    P64(b+288,g_app_persistence.reserve_mm);
    uint32_t m=Lock();
    memcpy(p->current,b,RIDE_BYTES);
    p->snapshot_token=p->force_token;
    uint32_t dirty=memcmp(p->saved,b,RIDE_BYTES)!=0;
    if(dirty&&!g_app_persistence.dirty)p->dirty_since=now;
    g_app_persistence.dirty=dirty;
    p->last_tick=now;
    Unlock(m);
}
