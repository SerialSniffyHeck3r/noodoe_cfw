#include "BSP_Display.h"
#include "bsp_eve.h"
#include "bsp_lcd_panel.h"
static uint32_t phase,started;
/* One display owner. Sleep does not reset the EVE or release its asset cache;
 * errors stay visible and the backlight remains off until recovery succeeds. */
uint32_t BSP_Display_SetSleeping(uint32_t sleeping)
{
    uint32_t now=HAL_GetTick();
    if(sleeping){
        if(phase==1)return 0;
        /* A rapid IGN reversal may interrupt wake settling. Finish it first
         * before issuing the opposite panel command. */
        if(phase==2||phase==3){uint32_t result=BSP_Display_SetSleeping(0);if(result)return result;}
        if(phase!=4){
            if(BSP_Display_SetBrightnessPercent(0)!=BSP_DISPLAY_OK)return 2;
            uint32_t result=BSP_EVE_Sleep();if(result)return result;
            phase=4; /* EVE asleep: retry only the unfinished panel operation. */
        }
        if(BSP_LCD_PanelSleep()!=HAL_OK)return 3;
        phase=1;return 0;
    }
    if(!phase)return 0;
    if(phase==1||phase==4){if(BSP_EVE_WakeBegin())return 4;phase=2;started=now;return 1;}
    if(phase==2){if(now-started<30U)return 1;
        if(BSP_EVE_WakeFinish())return 5;
        if(BSP_LCD_PanelWakeBegin()!=HAL_OK)return 6;
        phase=3;started=now;return 1;}
    if(now-started<120U)return 1;
    if(BSP_LCD_PanelWakeFinish()!=HAL_OK)return 7;
    phase=0;return 0;
}
/* Consumers must treat settling and partial sleep as unavailable too. A
 * rapid OFF/ON during those stages still requires a fresh dark-panel wake. */
uint32_t BSP_Display_IsSleeping(void){return phase!=0;}
