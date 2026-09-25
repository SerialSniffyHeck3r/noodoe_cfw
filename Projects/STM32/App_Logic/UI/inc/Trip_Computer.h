#ifndef TRIP_COMPUTER_H
#define TRIP_COMPUTER_H
#include <stdint.h>
#define TRIP_COUNT 4U
#define TRIP_REFUEL_DEBOUNCE_MS 3000U
#define TRIP_STOP_SPEED_DEFAULT_KPH 5U
#define TRIP_STOP_SPEED_MAX_KPH 10U
typedef enum { TRIP_A, TRIP_B, TRIP_TODAY, TRIP_REFUEL } TripKind;
/* Speed-derived distance is an estimate; ODO remains authoritative UART data.
 * moving+stopped is measured IGN-on time with fresh telemetry. Missing time is
 * explicit and never silently counted as a stop. All counters survive IGN. */
typedef struct {
    uint64_t distance_mm,moving_ms,stopped_ms,unknown_ms;
    uint32_t max_kph,valid,partial;
} TripRecord;
typedef struct {
    TripRecord records[TRIP_COUNT];
    uint32_t last_ms,last_speed,last_valid,last_ign,have_tick,date;
    uint32_t remainder,refuel_count,fuel_known,fuel_base,fuel_candidate;
    uint32_t fuel_pending,fuel_since,fuel_last_ms;
    uint32_t stop_speed_kph,last_stopped;
    /* Runtime calibration only; never serialize timing/ODO anchors. */
    uint64_t odo_raw_mm;
    uint32_t odo_last,odo_anchor,odo_clean,distance_q16,scale_remainder;
} TripComputer;
void TripComputer_Init(TripComputer *state);
/* Owner-only policy update. Classifies future sample intervals; accumulated
 * distance/time is never reclassified. Zero selects exactly stationary. */
uint32_t TripComputer_SetStopSpeed(TripComputer *state,uint32_t kph);
/* date is validated local YYYYMMDD, or0 when RTC is unknown. Calls from the
 * same owner use uint32 monotonic milliseconds; gaps >1500ms are unmeasured. */
void TripComputer_Tick(TripComputer *state,uint32_t now,uint32_t ign,
    uint32_t speed_valid,uint32_t speed_kph,uint32_t date);
/* Normalized, confirmed0..5 bars only. A stable rise >=2 from the last stable
 * level resets REFUEL exactly once. Noise/invalid/gaps restart debounce. */
uint32_t TripComputer_Fuel(TripComputer *state,uint32_t now,uint32_t valid,uint32_t bars);
/* Manual reset is limited to A/B. REFUEL reset is a sensor event, TODAY a date. */
uint32_t TripComputer_Reset(TripComputer *state,uint32_t trip);
/* Observe only fresh dashboard ODO. Complete consecutive km intervals set
 * distance scale; speed/max-speed display and time classification are unchanged. */
void TripComputer_Odometer(TripComputer *state,uint32_t valid,uint32_t odo_km);
#endif
