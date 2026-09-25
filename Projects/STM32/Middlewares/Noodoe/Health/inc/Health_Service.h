#ifndef HEALTH_SERVICE_H
#define HEALTH_SERVICE_H
#include <stdint.h>
#define HEALTH_STABLE_BOOT_MS 30000U
enum { HEALTH_IO, HEALTH_STORAGE, HEALTH_GRAPHICS, HEALTH_BT, HEALTH_OWNER_COUNT };
typedef struct {
    uint32_t magic,version,registered,failed_owner,checks,healthy_since,boot_ready;
    uint32_t last_ms[HEALTH_OWNER_COUNT],progress[HEALTH_OWNER_COUNT];
} HealthDiagnostics;
extern volatile HealthDiagnostics g_health;
/* Pure fixed-memory policy. The runtime owns the100ms calling task; drivers
 * and IRQs cannot impersonate completed task iterations. No packet/frame is
 * required: an idle/degraded owner that services its loop is healthy. */
void HealthService_Init(uint32_t now_ms);
void HealthService_Register(uint32_t owner,uint32_t now_ms);
void HealthService_Progress(uint32_t owner,uint32_t now_ms);
void HealthService_BootReady(void);
uint32_t HealthService_Process(uint32_t now_ms,uint32_t sleeping);
/* Emitted once after30s of successful runtime checks, for the independent gate
 * boot-attempt journal. The platform may override; no flash writes here. */
void HealthService_StableBoot(void);
#endif
