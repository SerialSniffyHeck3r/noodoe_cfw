#ifndef GRAPHICS_BRINGUP_HUD_H
#define GRAPHICS_BRINGUP_HUD_H
#include "lvgl.h"
#include <stdint.h>
/* UI owner only. Six compact result rows rotate across three pages. */
uint32_t GraphicsBringupHUD_Init(lv_obj_t *parent);
void GraphicsBringupHUD_Process(uint32_t now_ms);
/* 0 selects automatic rotation;1..3 pins a results page. May be called from
 * another task: only the graphics owner touches LVGL objects at its next poll. */
uint32_t GraphicsBringupHUD_SelectPage(uint32_t page);
extern volatile uint32_t g_graphics_bringup_page;
#endif
