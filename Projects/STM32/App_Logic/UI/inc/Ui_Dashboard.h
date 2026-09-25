#ifndef NOODOE_UI_DASHBOARD_H
#define NOODOE_UI_DASHBOARD_H
#include <stdint.h>
/* Child of the product state, not another task/event consumer. This is the
 * only card/footer storage. Power, global menus and warnings belong to parent. */
/* Stable IDs: retired OBD id4 is never reused. UI_CARD_MASK is the
 * supported navigation set; the visible carousel maps it to seven positions. */
typedef enum { UI_BLANK, UI_TRIP, UI_NOTIFICATIONS, UI_MUSIC, UI_RETIRED_OBD,
    UI_REMOTE, UI_PHONE_GPS, UI_SYSTEM, UI_CALLS, UI_CARD_COUNT } UiCard;
#define UI_CARD_MASK (((1U<<UI_CARD_COUNT)-1U)&~((1U<<UI_RETIRED_OBD)|(1U<<UI_REMOTE)))
typedef enum { UI_ODO, UI_TRIP1, UI_TRIP2, UI_RESV, UI_OIL, UI_BELT,
    UI_SERV, UI_FOOTER_COUNT } UiFooter;
typedef struct {
    uint32_t card,footer,footer_before_reserve,remote_active;
    uint32_t selection; /* Local item; never aliases the parent's menu row. */
    uint32_t gps_heading_up,gps_zoom;
    uint32_t item_direction; /* 0/1: next/down;2: previous/up. Presentation only. */
} UiDashboardState;
/* Initialize sanitized preferences. No heap, renderer, clock or device access. */
void UiDashboard_Init(UiDashboardState *state,uint32_t card_mask,uint32_t footer_mask,
    uint32_t preferred_card,uint32_t preferred_footer,uint32_t reserve_active);
const char *UiDashboard_CardTitle(uint32_t card);
const char *UiDashboard_FooterTitle(uint32_t footer);
/* Cumulative millimetres with explicit validity. ODO ignores bit0 here: its
 * authoritative input remains the dashboard UART. Never invent missing trips. */
typedef struct {
    uint32_t valid_mask;
    uint64_t distance_mm[UI_FOOTER_COUNT];
} UiDashboardDistances;
/* Domain-selected remaining fraction after combining distance/time/date rules.
 * Indices0/1/2 are OIL/BELT/SERV. Ratio0..1000 means0..100%; zero is due.
 * Explicit validity avoids displaying a fabricated maintenance history. */
typedef struct {
    uint32_t valid_mask,remaining_permille[3];
    uint32_t days_valid_mask,elapsed_days[3]; /* BELT/SERV calendar-day history, not remaining days. */
    uint32_t oil_hours_valid;
    uint64_t oil_ignition_ms; /* Actual IGN-on accumulation since the oil usage origin. */
} UiDashboardMaintenance;
/* Pure helper for domain callers: total0 is unknown. Overdue saturates at0.
 * Returns1 and fills remaining for a valid interval; no persistence/reset. */
uint32_t UiDashboard_Remaining(uint64_t used,uint64_t total,uint32_t *remaining);
#endif
