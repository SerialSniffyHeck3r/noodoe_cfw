#ifndef BSP_AMBIENT_H
#define BSP_AMBIENT_H
#include <stdint.h>
typedef struct {
    uint32_t magic, ready, error, manufacturer, device, configuration;
    uint32_t raw, millilux, valid, sample_ms, samples, failures;
    /* Original 48-byte prefix stays stable for SWD/HUD. SR1 is read alone,
     * never followed by a diagnostic SR2 read that might acknowledge ADDR.
     * HAL polling can report TIMEOUT while SR1 still records ARLO. */
    uint32_t phase, hal_status, hal_error, sr1, elapsed_ms, bus_hz, enabled;
} BSP_Ambient_Diagnostics;
typedef enum {
    BSP_AMBIENT_PHASE_NONE=0, BSP_AMBIENT_PHASE_DEINIT,
    BSP_AMBIENT_PHASE_INIT, BSP_AMBIENT_PHASE_ANALOG_FILTER,
    BSP_AMBIENT_PHASE_DIGITAL_FILTER, BSP_AMBIENT_PHASE_MANUFACTURER,
    BSP_AMBIENT_PHASE_DEVICE, BSP_AMBIENT_PHASE_ENABLE,
    BSP_AMBIENT_PHASE_CONFIG, BSP_AMBIENT_PHASE_RESULT
} BSP_Ambient_Phase;
#define BSP_AMBIENT_DEFAULT_HZ 400000U
#define BSP_AMBIENT_SLOW_PROBE_HZ 80000U
#define BSP_AMBIENT_IO_TIMEOUT_MS 20U
extern volatile BSP_Ambient_Diagnostics g_bsp_ambient;
/* AmbientService's low-priority worker exclusively owns these synchronous
 * operations. Upper tasks use queued service requests. Result0 succeeds;
 * 2/3/4=HAL ERROR/BUSY/TIMEOUT,5=ID mismatch,6=invalid sample,7=argument,
 * 8=not ready,9=forbidden context. Read/write transfer timeout is20ms;
 * installed HAL additionally uses25ms for its initial BUSY wait,with tick
 * granularity/scheduler latency. These are not hard real-time wall bounds.
 * Probe permits80/100/400kHz on I2C3 only; Init retains IOC-default400kHz.
 * No supply GPIO, manual pull-up/recovery pulse, or MFi I2C1 is touched. */
uint32_t BSP_Ambient_Init(void);
uint32_t BSP_Ambient_Probe(uint32_t bus_hz);
uint32_t BSP_Ambient_Poll(void);
uint32_t BSP_Ambient_SetEnabled(uint32_t enabled);
#endif
