#ifndef OIL_USAGE_SERVICE_H
#define OIL_USAGE_SERVICE_H
#include <stdint.h>
typedef struct {
    uint32_t magic,version,seq,ready,restored,persistent,save_result,gaps;
    uint32_t now_ms,ign_valid,ign_on,pending;
    uint64_t total_ms,committed_ms,boot_on_ms;
} OilUsageSnapshot;
extern volatile OilUsageSnapshot g_oil_usage;
/* I/O owner calls every2ms with actual debounced BSP IGN. settings_ready means
 * the storage worker finished Init, not merely started reading the journal. */
void OilUsageService_Process(uint32_t now_ms,uint32_t ign_valid,uint32_t ign_on,uint32_t settings_ready);
/* Nonblocking task snapshot. persistent means this accumulated value has a
 * provisioned journal backend; committed_ms is the last verified checkpoint.
 * A new device starts at this firmware's first tracking origin, not an inferred
 * historical oil change. No trip/maintenance reset is performed here. */
uint32_t OilUsageService_Get(OilUsageSnapshot *out);
#endif
