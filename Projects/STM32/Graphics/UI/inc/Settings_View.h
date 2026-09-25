#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H
#include "lvgl.h"
#include "Settings_UI.h"
uint32_t SettingsView_Create(lv_obj_t *screen);
void SettingsView_Hide(void);
/* UI owner supplies state; renderer never edits values or accesses hardware. */
void SettingsView_Render(const SettingsUI *state,uint32_t quick,uint32_t now);
void SettingsPosition_Draw(lv_layer_t *layer,uint32_t category_q,uint32_t row_q,uint32_t count,uint32_t alpha,uint32_t category_visible);
void SettingsMotion_Draw(lv_layer_t *layer,uint32_t elapsed,uint32_t alpha);
#endif
