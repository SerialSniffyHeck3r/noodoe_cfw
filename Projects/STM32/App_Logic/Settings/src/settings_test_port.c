#include "Settings_TestPort.h"
#include "App_DataConfig.h"
#include "ButtonEvents.h"
#include "BSP_Buttons.h"
volatile SettingsTestMailbox g_settings_test;
#if DATA_DEBUG
static uint32_t long_sent;
#endif
/* The same single-owner publish path fans out to all existing observers.
 * Hold duration is measured by MCU time, not forged in an immediate RELEASE. */
void SettingsTestPort_Process(uint32_t now)
{
    g_settings_test.magic=0x53545031;g_settings_test.version=1;g_settings_test.now_ms=now;
#if DATA_DEBUG
    if(g_settings_test.active){
        uint32_t dt=now-g_settings_test.started_ms;
        if(!long_sent&&dt>=BSP_BUTTONS_LONG_PRESS_MS){
            long_sent=1;ButtonEvent e={g_settings_test.button,BSP_BUTTON_EVENT_LONG_PRESS,now,dt,0};ButtonEvents_Publish(&e);
        }
        if(dt>=g_settings_test.hold_ms){
            ButtonEvent e={g_settings_test.button,BSP_BUTTON_EVENT_RELEASE,now,dt,0};ButtonEvents_Publish(&e);
            g_settings_test.active=0;g_settings_test.result=0;g_settings_test.ack=g_settings_test.request;
        }
    }else if(g_settings_test.request!=g_settings_test.ack){
        if(g_settings_test.button>=3||g_settings_test.hold_ms<50||g_settings_test.hold_ms>5000){g_settings_test.result=2;g_settings_test.ack=g_settings_test.request;return;}
        g_settings_test.started_ms=now;g_settings_test.active=1;long_sent=0;
        ButtonEvent e={g_settings_test.button,BSP_BUTTON_EVENT_PRESS,now,0,0};ButtonEvents_Publish(&e);
    }
#else
    g_settings_test.result=3;g_settings_test.ack=g_settings_test.request;
#endif
}
