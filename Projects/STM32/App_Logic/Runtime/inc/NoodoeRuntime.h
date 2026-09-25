#ifndef NOODOE_RUNTIME_H
#define NOODOE_RUNTIME_H
#include <stdint.h>
#include "Vehicle_Service.h"
#include "GNSS_Service.h"

typedef struct {
    uint32_t magic,version,started,start_error,io_heartbeat,storage_heartbeat;
    uint32_t graphics_heartbeat,io_ms,storage_ms,graphics_ms;
    uint32_t dash_result,storage_result,ambient_result,clock_result,bluetooth_result;
    uint32_t io_stack_free,storage_stack_free,heap_free,heap_min,reserved_obd_send_failures;
} NoodoeRuntime_Diagnostics;
extern volatile NoodoeRuntime_Diagnostics g_noodoe_runtime;
/* Called once from LCDTest after display startup. Creates separate bounded
 * I/O and storage workers. DashService now owns stock-compatible UART TX;
 * startup does not format NOR or change bootloader metadata. */
uint32_t NoodoeRuntime_Start(void);
/* Called by the graphics owner after successful progress, not by a timer. */
void NoodoeRuntime_GraphicsHeartbeat(void);
/* Copies stable snapshots; these do not let callers mutate parser state. */
uint32_t NoodoeRuntime_GetVehicle(VehicleSnapshot *out);
uint32_t NoodoeRuntime_GetGnss(GnssSnapshot *out);
/* Device owners publish these facts; UI reads a stable cached copy instead of
 * touching RTC/GPIO or reading partially updated volatile driver diagnostics.
 * Clock is UTC, validity is separate from successful I/O. */
typedef struct {
    uint32_t power_ms,ign_valid,ign_on;
    uint32_t clock_ms,clock_valid,year,month,day,hour,minute,second;
    uint32_t links; /* bit0 phone; bits1..3 reserved/0; actual link only */
    uint32_t weekday; /* Monday1..Sunday7, derived Gregorian date;0 invalid. */
} NoodoeSystemSnapshot;
uint32_t NoodoeRuntime_GetSystem(NoodoeSystemSnapshot *out);
#endif
