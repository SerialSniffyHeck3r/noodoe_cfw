#ifndef PHONE_TRAIL_H
#define PHONE_TRAIL_H
#include <stdint.h>
#define PHONE_TRAIL_POINTS 48U
typedef struct {int32_t lat,lon;uint32_t gap;} PhoneTrailPoint;
#define PHONE_TRAIL_GRID_LINES 22U
typedef struct {PhoneTrailPoint points[PHONE_TRAIL_POINTS];uint32_t count,last_ms,live,session,rejected,sample_ms,have_sample;int32_t east,north;uint32_t heading_valid;
    PhoneTrailPoint anchor,display,from,target;
    int32_t display_course,from_course,target_course,display_east,display_north;
    uint32_t camera_ms,camera_ready;
} PhoneTrail;
/* Presentation-only 300ms camera. Accepted fixes remain unchanged; no caller
 * may use this interpolated state for vehicle/trip distance accumulation. */
void PhoneTrail_RenderStep(PhoneTrail *s,uint32_t now);
uint32_t PhoneTrail_ProjectGrid(const PhoneTrail *s,int16_t lines[PHONE_TRAIL_GRID_LINES][4],uint32_t w,uint32_t h,uint32_t up,uint32_t zoom);
void PhoneTrail_Init(PhoneTrail *trail);
/* Only the primary phone owner feeds this cache. Invalid fixes stop the live
 * marker; the previous route remains visible until the next confirmed ride.
 * Reset with PhoneTrail_Init only at SESSION_START, never on a radio gap. */
void PhoneTrail_Feed(PhoneTrail *trail,uint32_t now,uint32_t valid,int32_t lat_e7,int32_t lon_e7);
/* Integer equirectangular north-up projection with cosine longitude correction,
 * dateline wrap, square bounds and no map tiles. Returns plotted count. */
uint32_t PhoneTrail_Project(const PhoneTrail *trail,int16_t xy[PHONE_TRAIL_POINTS][2],uint32_t width,uint32_t height);
/* Course is motion bearing, not magnetic phone orientation. heading_valid is
 * supplied only for accurate moving fixes. Older senders use displacement. */
void PhoneTrail_FeedHeading(PhoneTrail *s,uint32_t sample_ms,uint32_t valid,int32_t lat,int32_t lon,uint32_t heading_valid,uint32_t course_mdeg);
uint32_t PhoneTrail_ProjectView(const PhoneTrail *s,int16_t xy[PHONE_TRAIL_POINTS][2],uint32_t w,uint32_t h,uint32_t heading_up,uint32_t zoom);
uint32_t PhoneTrail_Scale(uint32_t zoom);
#endif
