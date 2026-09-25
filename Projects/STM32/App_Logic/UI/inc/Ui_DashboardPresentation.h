#ifndef NOODOE_UI_DASHBOARD_PRESENTATION_H
#define NOODOE_UI_DASHBOARD_PRESENTATION_H
#include "Ui_State.h"
/* A complete immutable-per-frame view model. The renderer owns no navigation
 * decisions and can be replaced without changing the child state or data API. */
typedef struct {
    char title[24],line[32],hint[32],number[12];
    char footer_title[8],footer_value[9],unit[5];
    uint32_t footer_valid,card,footer;
    uint32_t maintenance,remaining_valid,remaining_permille;
    char auxiliary_title[6],auxiliary_value[8];
    uint32_t reserve_distance_warning; /* Valid reserve trip strictly >20km, independent of display unit. */
} UiDashboardPresentation;
/* Constant-time bounded formatting; distances are millimetres. Neither this
 * API nor its input changes trip counters or creates persistent history. */
void UiDashboard_Present(const UiState *state,const UiDashboardDistances *distances,
    uint32_t odometer_km,uint32_t odometer_valid,uint32_t units,uint32_t uart_live,
    UiDashboardPresentation *out);
/* Merge a separately published domain status into the already formatted page.
 * This never changes selected mode, trip values or any device state. */
void UiDashboard_PresentMaintenance(const UiDashboardMaintenance *maintenance,UiDashboardPresentation *out);
#endif
