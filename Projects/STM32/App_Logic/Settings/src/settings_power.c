#include "App_Settings.h"
#include "Ui_State.h"
/* Durations are residence times, not IGN debounce or summary time. The final
 * enabled stage holds until ignition, including a zero-minute final hold. */
static const SettingItem items[]={
 {.key=SK_OFF_DISPLAY,.name="Screen hold",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_POWER,.name="Screen duration",.kind=SETTING_NUMBER,.min=0,.max=1440,.step=1},
 {.key=SK_OFF_BT,.name="BT only",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_BT_HOLD,.name="BT-only duration",.kind=SETTING_NUMBER,.min=0,.max=1440,.step=1},
 {.key=SK_OFF_DEEP,.name="All off (STOP)",.kind=SETTING_CHOICE,.min=0,.max=1,.step=1},
 {.key=SK_OFF_FINAL_WAIT,.name="Wake condition",.kind=SETTING_READONLY,.min=0,.max=0,.step=1},
};
const SettingItem *SettingsPower_Items(uint32_t *n)
{if(n)*n=sizeof(items)/sizeof(items[0]);return items;}
uint32_t SettingsPower_EnabledMask(void)
{
    return (AppSettings_Value(SK_OFF_DISPLAY)?UI_OFF_STAGE_DISPLAY:0U)|
        (AppSettings_Value(SK_OFF_BT)?UI_OFF_STAGE_BT:0U)|
        (AppSettings_Value(SK_OFF_DEEP)?UI_OFF_STAGE_DEEP:0U);
}
