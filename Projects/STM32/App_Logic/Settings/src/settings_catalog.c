#include "App_Settings.h"
static const SettingItem root[]={
 {.key=1,.name="Display",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=2,.name="Date & time",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},
 {.key=3,.name="Vehicle",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=4,.name="Maintenance",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},
 {.key=5,.name="Connections",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=6,.name="System",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1},{.key=11,.name="Debug",.kind=SETTING_SUBMENU,.min=0,.max=0,.step=1}};
/* Fixed tree depth2. Catalog navigation contains no mutable selection state. */
const SettingItem *SettingsCatalog_Items(uint32_t menu,uint32_t *n)
{
    switch(menu){case 0:if(n)*n=7;return root;case 1:return SettingsDisplay_Items(n);
    case 2:return SettingsTime_Items(n);case 3:return SettingsVehicle_Items(n);
    case 4:case 7:case 8:case 9:return SettingsMaintenance_Items(menu,n);
    case 5:return SettingsConnections_Items(n);case 6:return SettingsSystem_Items(n);case 10:return SettingsPower_Items(n);case 11:return SettingsDebug_Items(n);
    default:if(n)*n=0;return 0;}
}
const char *SettingsCatalog_Title(uint32_t menu)
{static const char *const titles[]={"Settings","Display","Date & time","Vehicle","Maintenance","Connections","System","Oil service","Belt service","Service","Power sequence","Debug"};return menu<12?titles[menu]:"Settings";}
uint32_t SettingsCatalog_Parent(uint32_t menu)
{return menu==10?6U:(menu>=7&&menu<=9?4U:0U);}
const SettingItem *SettingsCatalog_Find(uint32_t key)
{
    for(uint32_t m=1;m<12;++m){uint32_t n;const SettingItem *items=SettingsCatalog_Items(m,&n);
        for(uint32_t i=0;i<n;++i)if(items[i].kind!=SETTING_SUBMENU&&items[i].key==key)return &items[i];}
    return 0;
}
