#ifndef ODOMETER_VIEW_H
#define ODOMETER_VIEW_H
#include "lvgl.h"
#include "Odometer_Guard.h"
uint32_t OdometerView_Create(lv_obj_t *screen);
uint32_t OdometerView_Visible(void);
void OdometerView_Render(const OdometerStatus *,uint32_t visible,uint32_t now);
uint32_t OdometerView_Button(uint32_t button,uint32_t event);
#endif
