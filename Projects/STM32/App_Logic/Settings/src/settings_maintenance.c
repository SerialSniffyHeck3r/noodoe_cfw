#include "App_Settings.h"
#include "Ui_DashboardPresentation.h"
#if NOODOE_PRODUCT
#include "App_Persistence.h"
#endif
#include <string.h>
static const SettingItem menu[]={
 {.key=7,.name="Oil",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=8,.name="Belt",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=9,.name="Service",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1}};
#define SERVICE_ITEMS(prefix) {.key=prefix##_DISTANCE,.name="Distance interval",.kind=SETTING_NUMBER,.min=0,.max=100000,.step=100},\
 {.key=prefix##_HOURS,.name="Key ON hours",.kind=SETTING_NUMBER,.min=0,.max=10000,.step=1},\
 {.key=prefix##_DAYS,.name="Day interval",.kind=SETTING_NUMBER,.min=0,.max=3650,.step=1},\
 {.key=prefix##_RESET,.name="Service completed",.kind=SETTING_CONFIRM,.min=0,.max=1,.step=1}
static const SettingItem services[3][4]={{SERVICE_ITEMS(SK_OIL)},{SERVICE_ITEMS(SK_BELT)},{SERVICE_ITEMS(SK_SERV)}};
const SettingItem *SettingsMaintenance_Items(uint32_t m,uint32_t *n)
{if(m==4){if(n)*n=3;return menu;}if(n)*n=4;return services[m>=7&&m<=9?m-7:0];}
/* Gregorian day index uses existing pure calendar arithmetic, not wall-clock
 * milliseconds. A backwards clock/ODO leaves that criterion unknown. */
static uint32_t Day(const BSP_CalendarDateTime *t)
{
    uint32_t y=t->year,m=t->month;y-=m<=2;uint32_t era=y/400,yoe=y-era*400;
    return era*146097+yoe*365+yoe/4-yoe/100+(153*(m>2?m-3:m+9)+2)/5+t->day-1;
}
void AppSettings_Maintenance(UiDashboardDistances *d,UiDashboardMaintenance *out,uint32_t *due)
{
    if(!g_app_settings||!d||!out||!due)return;
    AppSettings *s=g_app_settings;
    memset(d,0,sizeof(*d));memset(out,0,sizeof(*out));*due=0;
    BSP_CalendarDateTime local;uint32_t day=0;
    if(s->facts.clock_valid&&BSP_Calendar_AddSeconds(&s->facts.utc,s->values[SK_ZONE]*60,&local))day=Day(&local);
    for(uint32_t n=0;n<3;++n){SettingsServiceOrigin *b=&s->origins[n];uint32_t key=SK_OIL_DISTANCE+n*4;
        uint64_t used[3]={0};uint32_t known=0,ratio=1000,have=0,unknown=0;
        if((b->valid&1)&&s->facts.odo_valid&&s->facts.odo_km>=b->odo_km){used[0]=s->facts.odo_km-b->odo_km;known|=1;
            d->valid_mask|=1U<<(UI_OIL+n);d->distance_mm[UI_OIL+n]=used[0]*1000000ULL;}
        if((b->valid&2)&&s->facts.on_valid&&s->facts.on_ms>=b->on_ms){used[1]=s->facts.on_ms-b->on_ms;known|=2;
            if(!n){out->oil_hours_valid=1;out->oil_ignition_ms=used[1];}}
        if((b->valid&4)&&s->facts.clock_valid&&day>=b->day){used[2]=day-b->day;known|=4;out->days_valid_mask|=1U<<n;out->elapsed_days[n]=(uint32_t)used[2];}
        for(uint32_t criterion=0;criterion<3;++criterion){uint64_t limit=s->values[key+criterion];if(!limit)continue;
            if(criterion==1)limit*=3600000ULL;
            if(known&(1U<<criterion)){uint32_t fraction;UiDashboard_Remaining(used[criterion],limit,&fraction);
                if(fraction<ratio)ratio=fraction;
                have=1;}else unknown=1;
        }
        if(have&&!ratio)*due|=1U<<n;
        if(have&&!unknown){out->valid_mask|=1U<<n;out->remaining_permille[n]=ratio;if(!ratio)*due|=1U<<n;}
    }
}
/* Establish only explicitly confirmed per-service origins. Never clear the
 * authoritative ODO, lifetime ignition usage or another service baseline. */
void SettingsMaintenance_Reset(uint32_t index)
{
    AppSettings *s=g_app_settings;if(!s||index>=3)return;SettingsServiceOrigin *b=&s->origins[index];
    *b=(SettingsServiceOrigin){.valid=s->facts.on_valid?2U:0U,.on_ms=s->facts.on_ms};
    if(s->facts.odo_valid){b->valid|=1;b->odo_km=s->facts.odo_km;}
    BSP_CalendarDateTime local;
    if(s->facts.clock_valid&&BSP_Calendar_AddSeconds(&s->facts.utc,s->values[SK_ZONE]*60,&local)){b->valid|=4;b->day=Day(&local);}
#if NOODOE_PRODUCT
    uint32_t id;(void)RideStore_RequestCheckpoint(&id);
#endif
}
