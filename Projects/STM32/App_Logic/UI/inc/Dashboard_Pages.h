#ifndef DASHBOARD_PAGES_H
#define DASHBOARD_PAGES_H
#include "Ui_DashboardPresentation.h"
#include "Trip_Computer.h"
#include "Phone_Content.h"
#include "Phone_Trail.h"
/* Semantic page model: fixed rendering geometry belongs to Graphics. Copies
 * contain no LVGL pointers. art points to the owner's current immutable copy;
 * renderer copies its pixels before this call returns. No heap/I/O allowed. */
typedef struct {
    uint32_t key,kind,grey,selection,known,ratio_permille,art_revision,direction;
    char title[32],lines[5][64],numbers[6][16],note[40];
    uint32_t art_valid,plot_count,visual_key,reply_count,reply_selection;const uint8_t *art;
    int16_t plot[PHONE_TRAIL_POINTS][2];
    int16_t grid[PHONE_TRAIL_GRID_LINES][4];uint32_t grid_count;
    int32_t heading_east,heading_north;uint32_t map_scale;uint64_t plot_gaps;
} DashboardPage;
typedef struct {
    const TripComputer *trips;const PhoneContentSlot *phone;const PhoneTrail *trail;
    uint32_t phone_slot,now_ms,units,date,weekday;
    
    uint32_t development_mask;
} DashboardPageFacts;
/* Converts domain snapshots and parent overlays into one central section.
 * key changes only with navigable mode, not every telemetry update. */
void DashboardPages_Present(const UiState *state,const UiDashboardPresentation *shell,
    const DashboardPageFacts *facts,DashboardPage *page);
#endif
