#ifndef NOODOE_UI_STATE_INTERNAL_H
#define NOODOE_UI_STATE_INTERNAL_H
#include "Ui_State.h"
#include <string.h>
uint32_t Ui_Emit(UiState *s,uint32_t kind,uint32_t arg,uint32_t value);
void Ui_ContextChanged(UiState *s);
void UiCalls_Update(UiState *s,const PhoneCallsSnapshot *input,uint32_t allowed);
uint32_t UiCalls_Navigate(UiState *s,uint32_t button,uint32_t hold);
void Ui_PowerEvent(UiState *s,const UiEvent *e);
void Ui_WarningEvent(UiState *s,const UiEvent *e);
void Ui_InputEvent(UiState *s,const UiEvent *e);
void Ui_Navigate(UiState *s,uint32_t button,uint32_t long_press);
void Ui_DashboardNavigate(UiState *s,uint32_t button,uint32_t long_press);
void Ui_StartWarning(UiState *s,uint32_t now);
#endif
