#ifndef SPEED_HOME_STARTUP_H
#define SPEED_HOME_STARTUP_H
#include <stdint.h>
#define SPEED_HOME_STARTUP_LEG_MS 800U
typedef enum { SPEED_START_LIVE, SPEED_START_ARMED, SPEED_START_HOME_FRAME,
    SPEED_START_UP, SPEED_START_PEAK_FRAME, SPEED_START_DOWN,
    SPEED_START_ZERO_FRAME, SPEED_START_PACKET } SpeedHomeStartupPhase;
/* UI-owner state, also readable through SWD. Angles use0..10000; all times
 * are monotonic32-bit milliseconds. No allocation, peripheral or telemetry
 * mutation occurs here. Frame acknowledgement is supplied by the renderer. */
typedef struct {
    uint32_t magic,version,phase,epoch,value,start_ms,frame_before,cutoff_ms;
    uint32_t accepted_telemetry_ms,starts,completed,cancelled,up_ms,down_ms;
} SpeedHomeStartup;
void SpeedHomeStartup_Init(SpeedHomeStartup *s);
/* Every IGN epoch, including provisional OFF, is observed before composition.
 * cold is the app's sweep qualifier: cold boot, display sleep, or completed
 * lit standby. It is deliberately broader than the Welcome qualifier.
 * Repeated facts cannot rearm a running or completed sweep. */
void SpeedHomeStartup_Ignition(SpeedHomeStartup *s,uint32_t epoch,uint32_t on,uint32_t cold);
uint32_t SpeedHomeStartup_WaitsForFrame(const SpeedHomeStartup *s);
/* Return1 overrides only the displayed ring with s->value. Real speed validity
 * and trip/safety data remain untouched. Fresh telemetry must be stamped
 * strictly AFTER the completed zero frame; other UART commands cannot release
 * this gate. Without a new packet, the zero ring is held indefinitely. */
uint32_t SpeedHomeStartup_Step(SpeedHomeStartup *s,uint32_t now,uint32_t home_ready,
    uint32_t frame,uint32_t presented,uint32_t speed_valid,uint32_t telemetry_ms);
#endif
