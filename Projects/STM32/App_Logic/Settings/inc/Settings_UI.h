#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H
#include "App_Settings.h"
#include "Ui_State.h"
#include "Settings_Motion.h"
#include "Scene_Transition.h"
typedef struct {uint32_t magic,version,open,leaving,menu,row,mode,field,confirm,category;
 uint32_t rows[12],return_card,return_selection,return_footer,last_card,last_selection,last_footer;
 uint32_t blocked,pressed,repeated,press_ms[3],repeat_ms[3],tokens[3],request_id,message_until;
 int32_t edit;uint32_t edit_key;char message[32];SettingsMotion motion;SceneTransition transition;ScenePose pose;
} SettingsUI;
extern SettingsUI *g_settings_ui;
/* Product owner only. Observes global BSP events but never consumes the BSP
 * queue. Return1 means this exact event belongs to the settings child. */
void SettingsUI_Init(SettingsUI *s,uint32_t boot_held);
uint32_t SettingsUI_Button(UiState *parent,uint32_t key,uint32_t type,uint32_t duration,uint32_t now);
void SettingsUI_Process(UiState *parent,uint32_t now,uint32_t valid,uint32_t speed);
uint32_t SettingsUI_Quick(const UiState *parent);
void SettingsUI_Close(UiState *parent,uint32_t now);
/* Editor mode1 selects a field/Apply/Back; mode3 adjusts the selected field.
 * Returning from adjustment never applies the retained draft implicitly. */
uint32_t SettingsUI_FieldCount(uint32_t key);
#endif
