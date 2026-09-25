#ifndef VEHICLE_SERVICE_H
#define VEHICLE_SERVICE_H
#include <stdint.h>
#include <stddef.h>

#define VEHICLE_FRAME_MAX 259U
#define VEHICLE_LINK_BAUD 115200U
#define VEHICLE_PARTIAL_TIMEOUT_MS 100U
#define VEHICLE_VALID_SPEED (1U<<0)
#define VEHICLE_VALID_ODOMETER (1U<<1)
#define VEHICLE_VALID_FUEL_OBSERVED (1U<<2)
#define VEHICLE_FUEL_MEASUREMENT_ERROR (1U<<3)
#define VEHICLE_FUEL_INFERRED (1U<<4)

/* fields의 유효성은 마지막 정상 telemetry 시각을 기준으로 한다. raw/payload2/
 * temperature_candidate/status_high는 해석 근거가 제한된 원시 정보이며 연료%
 * 또는 실제 온도로 승격하지 않는다. fuel_observed는실측0x50/0x51만유효하다. */
typedef struct {
    uint32_t sequence, valid_fields, stale, telemetry_ms, age_ms;
    uint32_t speed_kph, odometer_km, fuel_observed;
    uint32_t payload2_raw, status_raw, status_high, status_low;
    int32_t temperature_candidate_c;
    uint32_t extended_raw, extended_present;
    uint32_t frame_ms, frame_command, frame_length, raw_length;
    uint8_t raw[VEHICLE_FRAME_MAX];
} VehicleSnapshot;

/* 한 task가 소유한다. ISR/ring/DMA와 무관하며 호출자가 수신 조각을 Feed한다. */
typedef struct {
    VehicleSnapshot latest;
    uint32_t stale_ms, buffered, last_byte_ms, have_telemetry;
    uint32_t frames_ok, checksum_errors, payload_errors, discarded_bytes, partial_timeouts;
    uint8_t buffer[VEHICLE_FRAME_MAX];
    /* Stock UART parser admits only21/22/41/42 and nonempty payloads before
     * link accounting. Other valid frames remain available as raw diagnostics. */
    uint32_t link_frames_ok;
} VehicleService;

/* 모든 API는 하드웨어/할당/송신을 수행하지 않는다. now_ms는같은단조32bit ms clock이다. */
void VehicleService_Init(VehicleService *service, uint32_t stale_ms);
void VehicleService_Feed(VehicleService *service, const uint8_t *data, size_t length, uint32_t now_ms);
void VehicleService_Poll(VehicleService *service, uint32_t now_ms);
/* 성공1/잘못된인수0. 이전raw를보존하되stale이면valid_fields를0으로반환한다. */
uint32_t VehicleService_GetSnapshot(const VehicleService *service, uint32_t now_ms, VehicleSnapshot *out);
#endif
