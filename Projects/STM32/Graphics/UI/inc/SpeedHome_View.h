#ifndef SPEED_HOME_VIEW_H
#define SPEED_HOME_VIEW_H
#include "SpeedHome_Model.h"
#include "Ui_DashboardPresentation.h"
#include "lvgl.h"
/* Fixed480px geometry, no content-sized boxes. All functions run only on the
 * Graphics task. View never reads a device or drives the UI state machine. */
uint32_t SpeedHome_Create(lv_obj_t *parent);
void SpeedHome_Destroy(void);
/* Borrowed parent for all riding menus/cards/warnings; NULL before creation.
 * Graphics-task only. Children use local coordinates and are clipped by this
 * fixed viewport. Do not delete, resize, reposition or enable overflow on the
 * parent; do not create riding overlays on Graphics_GetScreen()/top layer.
 * Destroy/recreate invalidates this pointer along with all view children. */
lv_obj_t *SpeedHome_GetContentRoot(void);
/* Return its fixed absolute screen rectangle when created; NULL output or an
 * absent view returns0 without changing output. No allocation/device access. */
uint32_t SpeedHome_GetContentArea(lv_area_t *area);
void SpeedHome_Render(const SpeedHomeModel *model,const UiDashboardPresentation *presentation,uint32_t now_ms);
#include "Scene_Transition.h"
void SpeedHome_ApplyScene(const ScenePose *pose,uint32_t quick);
void SpeedHome_GetScene(ScenePose *pose);
#endif
