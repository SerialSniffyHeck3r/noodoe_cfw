#ifndef SPEED_HOME_MODEL_H
#define SPEED_HOME_MODEL_H
#include <stdint.h>
/* Presentation latency only. Every owner update retargets from the current
 * interpolated angle; raw speed remains available for telemetry/speed locks. */
#define SPEED_HOME_INTERPOLATION_MS 150U
/* Hardware-independent input. valid_mask bits0/1 mean speed/ODO; stale values
 * are retained for diagnosis but must not be rendered as fresh. hour/minute are local display time; the App applies the selected UTC offset. */
typedef struct {
    uint32_t now_ms,valid_mask,speed_kph,odometer_km,units;
    uint32_t clock_valid,hour,minute;
    uint32_t session_valid,session_peak_kph,session_average_kph10,session_generation;
} SpeedHomeInput;
typedef struct {
    uint32_t initialized,speed_valid,odo_valid,clock_valid,units,max_kph;
    uint32_t arc_value,arc_color,target,from,transition_ms,last_now;
    uint32_t session_valid,peak_ratio,average_ratio,peak_color,session_generation,arc_override;
    char clock[6],odo[7],unit[5];
} SpeedHomeModel;
/* max_kph is a display scale, not inferred vehicle top speed. Invalid input
 * never becomes0km/h; ratio uses km/h regardless of displayed distance units. */
void SpeedHome_InitModel(SpeedHomeModel *m,uint32_t now_ms,uint32_t max_kph);
void SpeedHome_UpdateModel(SpeedHomeModel *m,const SpeedHomeInput *input);
/* Call only for a composed driving frame, after any startup override. The
 * visual peak remembers the displayed arc, never the raw telemetry maximum. */
void SpeedHome_RecordDisplayed(SpeedHomeModel *m);
/* Presentation-only startup override. Reset interpolation to the shown angle
 * so the first newly accepted speed starts from zero, not a hidden old target.
 * Raw speed/ODO/clock validity and domain state are never changed. */
void SpeedHome_OverrideArc(SpeedHomeModel *m,uint32_t value,uint32_t now_ms);
#endif

