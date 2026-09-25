#ifndef SCREEN_WARNING_VIEW_H
#define SCREEN_WARNING_VIEW_H
#include "lvgl.h"
#include "ScreenWarningOverlay.h"
uint32_t ScreenWarningView_Create(lv_obj_t *parent);
void ScreenWarningView_Render(const ScreenWarningState *state);
#endif
