#include "App_Settings.h"
/* Feature-owned immutable settings descriptors. Bounds apply equally to UI
 * and public requests; changing focus never changes a device. */
static const SettingItem items[]={
 {.key=SK_UNITS,.name="Distance units",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_SCALE,.name="Ring maximum",.kind=SETTING_NUMBER,.min=20,.max=400,.step=10},
 {.key=SK_TRIP_STOP_SPEED,.name="Stop threshold",.kind=SETTING_NUMBER,.min=0,.max=10,.step=1},
 {.key=SK_MODEL,.name="Vehicle",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
};
const SettingItem *SettingsVehicle_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}
