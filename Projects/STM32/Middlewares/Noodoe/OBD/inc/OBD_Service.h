#ifndef OBD_SERVICE_H
#define OBD_SERVICE_H
#include <stdint.h>
#include <stddef.h>

#define OBD_RESPONSE_MAX 512U
#define OBD_COMMAND_MAX 16U
#define OBD_PID_COUNT 8U
typedef enum {
    OBD_RESULT_NONE=0, OBD_RESULT_OK, OBD_RESULT_NO_DATA, OBD_RESULT_ADAPTER_ERROR,
    OBD_RESULT_TIMEOUT, OBD_RESULT_OVERFLOW, OBD_RESULT_MALFORMED, OBD_RESULT_NEGATIVE_RESPONSE
} ObdResult;
typedef enum {
    OBD_DISCONNECTED=0, OBD_INITIALIZING, OBD_READY, OBD_WAITING, OBD_DRAINING, OBD_FAILED
} ObdPhase;
typedef enum {
    OBD_VALUE_RPM=0, OBD_VALUE_SPEED, OBD_VALUE_COOLANT, OBD_VALUE_INTAKE_TEMP,
    OBD_VALUE_THROTTLE, OBD_VALUE_LOAD, OBD_VALUE_MAP, OBD_VALUE_MAF
} ObdValueIndex;

/* values단위:index0 RPM*4(rawAB),1km/h,2/3섭씨,4/5permille,6kPa,7mg/s.
 * PIDs는0C,0D,05,0F,11,04,0B,10. 원본bytes와각필드시각을함께보존한다.
 * headers-off다중ECU응답은첫정상응답을선택하고multiple_replies로명시한다. */
typedef struct {
    uint32_t connected, phase, last_result, last_pid, last_result_ms;
    uint32_t valid_fields, stale_fields, value_ms[OBD_PID_COUNT];
    int32_t values[OBD_PID_COUNT];
    uint32_t raw_values[OBD_PID_COUNT], supported_01_20, support_known;
    uint32_t completed, timeouts, adapter_errors, malformed, overflows, multiple_replies;
    uint32_t raw_length;
    char command[OBD_COMMAND_MAX], raw[OBD_RESPONSE_MAX];
} ObdSnapshot;
typedef struct {
    ObdSnapshot latest;
    uint32_t connected, phase, init_index, poll_index, current_pid, is_init;
    uint32_t started_ms, ready_ms, timeout_ms, stale_ms, poll_interval_ms;
    uint32_t used, overflow;
    char response[OBD_RESPONSE_MAX];
} ObdService;

/* adapter 설정은 ATI/ATE0/ATL0/ATS0/ATH0/ATM0/ATTP0, 읽기는 Mode01만 생성한다.
 * ECU 쓰기/DTC 삭제/adapter EEPROM 저장 명령은 생성하지 않는다. 송신은 호출자가 맡는다. */
void ObdService_Init(ObdService *service, uint32_t timeout_ms, uint32_t stale_ms, uint32_t poll_interval_ms);
void ObdService_SetConnected(ObdService *service, uint32_t connected, uint32_t now_ms);
void ObdService_Poll(ObdService *service, uint32_t now_ms);
/* 송신 가능할 때만 CR 종단 command와 NUL을 복사하고 CR 포함 byte 수를 반환한다.
 * 0이면 송신이 없다. 한 명령을 받은 뒤 prompt까지 다음 명령은 없으며, buffer가
 * 부족하면 상태를 진행하지 않는다. 반환 직후 실제 transport로 송신해야 한다. */
size_t ObdService_TakeCommand(ObdService *service, char *out, size_t capacity, uint32_t now_ms);
void ObdService_Feed(ObdService *service, const uint8_t *data, size_t length, uint32_t now_ms);
uint32_t ObdService_GetSnapshot(const ObdService *service, uint32_t now_ms, ObdSnapshot *out);
#endif
