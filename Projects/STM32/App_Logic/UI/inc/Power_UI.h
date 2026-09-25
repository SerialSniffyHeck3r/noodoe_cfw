#ifndef POWER_UI_H
#define POWER_UI_H
#include "Ui_State.h"
#include "NoodoeRuntime.h"
#include "SpeedHome_Model.h"
#include "Ui_DashboardPresentation.h"
typedef struct {uint32_t magic,version,state,policy,entered_ms,compose,display_asleep,error;
 uint32_t summary_oil_valid,summary_oil_permille;uint64_t ride_ms,distance_mm;
 uint32_t ring_progress,shell_progress,welcome_active,wake_pending;
 uint32_t off_phase,session_open,completed,off_phase_ms;
 uint64_t finished_ride_ms,finished_distance_mm;} PowerUIDiagnostics;
extern volatile PowerUIDiagnostics g_power_ui;
uint32_t PowerUI_Init(void);
/* UI-owner updates policy from true device facts. Return1 permits rendering
 * this cycle; static OFF pages only compose on a minute or state change. */
uint32_t PowerUI_Update(UiState *state,const NoodoeSystemSnapshot *system,const VehicleSnapshot *vehicle,uint32_t now);
void PowerUI_SetDistanceScale(uint32_t q16);
void PowerUI_Input(SpeedHomeInput *input);
void PowerUI_Render(UiState *state,const SpeedHomeModel *model,UiDashboardPresentation *footer,const UiDashboardMaintenance *maintenance,uint32_t now);
uint32_t PowerUI_GraphicsDue(void);
void PowerUI_AfterGraphics(uint32_t now_ms);
uint32_t PowerUI_WaitMs(void);
#endif
