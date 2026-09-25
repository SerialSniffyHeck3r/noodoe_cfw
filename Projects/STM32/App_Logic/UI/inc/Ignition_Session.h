#ifndef IGNITION_SESSION_H
#define IGNITION_SESSION_H
#include <stdint.h>
/* Session time includes stops. Missing speed intervals do not invent distance. */
typedef struct {uint64_t distance_mm,ride_ms;uint32_t on,last_ms,speed,valid,partial,distance_fraction,distance_scale_q16;
 uint64_t finished_distance_mm,finished_ride_ms;uint32_t finished_partial,completed;
 /* Valid-speed time includes stationary samples, excludes telemetry gaps.
  * Integral is twice kph*ms, independent of trip distance calibration. */
 uint64_t speed_ms2,known_ms;uint32_t peak_kph,have_speed;} IgnitionSession;
/* on is the committed logical session latch, never the raw IGN pin. Falling
 * snapshots the complete ride and clears only its active counters once. */
void IgnitionSession_Tick(IgnitionSession *s,uint32_t now,uint32_t on,uint32_t speed_valid,uint32_t kph);
#endif
