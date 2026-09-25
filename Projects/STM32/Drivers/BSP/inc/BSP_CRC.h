#ifndef BSP_CRC_H
#define BSP_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_CRC_POLYNOMIAL 0x04C11DB7UL
#define BSP_CRC_INITIAL 0xFFFFFFFFUL
#define BSP_CRC_HW_MAX_BYTES 4096U

/* SR1 trailer 방식이다. 입력 bytes를 little-endian 32-bit word로 조립하고
 * word의 최상위 bit부터 처리한다. 반사 및 최종 XOR는 없다. Bluetooth 전송의
 * reflected CRC32와 다른 알고리즘이므로 전송 CRC 필드에 이 값을 쓰지 않는다.
 * 모든 길이는 byte 단위다. context/input/output 영역은 서로 겹치지 않아야 한다. */
typedef enum {
    BSP_CRC_OK = 0,
    BSP_CRC_INVALID_ARGUMENT,
    BSP_CRC_PARTIAL_WORD,
    BSP_CRC_LENGTH_OVERFLOW,
    BSP_CRC_BUSY,
    BSP_CRC_TOO_LARGE,
    BSP_CRC_MISMATCH
} BSP_CRC_Status;

typedef enum {
    BSP_CRC_REQUIRE_FULL_WORDS = 0,
    BSP_CRC_PAD_FF = 1
} BSP_CRC_FinalMode;

/* 호출자별 독립 상태다. Init 이후 Update/Final을 사용한다. 동일 context의 동시
 * 호출은 호출자가 직렬화한다. 다른 context와 HW 계산은 서로 영향을 주지 않는다.
 * total_bytes는 padding을 제외한 실제 입력 누계, pending은 아직 word가 안 된 byte다. */
typedef struct {
    uint32_t state;
    uint32_t total_bytes;
    uint32_t pending_word;
    uint32_t pending_bytes;
} BSP_CRC_Context;

typedef struct {
    uint32_t software;
    uint32_t hardware;
    uint32_t bytes;
} BSP_CRC_Comparison;

/* context를 초기값으로 되돌린다. NULL은 무시한다. 레지스터/전역 CRC는 안 건드린다. */
void BSP_CRC_Init(BSP_CRC_Context *context);

/* 임의 크기의 byte 청크를 누적한다. 4-byte 경계를 넘는 청크도 이어 붙인다.
 * length=0이면 data=NULL을 허용한다. 잘못된 인수나 uint32 byte 누계 초과에서는
 * context를 바꾸지 않는다. 입력 메모리와 context의 겹침은 지원하지 않는다. */
BSP_CRC_Status BSP_CRC_Update(BSP_CRC_Context *context, const void *data, size_t length);

/* context를 변경하지 않고 CRC를 조회하므로 Final 뒤 Update도 가능하다.
 * REQUIRE_FULL_WORDS는 남은 1..3 bytes를 오류로 보고한다. PAD_FF는 마지막
 * word의 빈 상위 bytes만 FF로 채운 복사본을 계산한다. 완전 word에는 추가하지 않는다.
 * 실패하면 *result를 보존한다. 빈 입력 CRC는 FFFFFFFF다. */
BSP_CRC_Status BSP_CRC_Final(const BSP_CRC_Context *context,
                            BSP_CRC_FinalMode mode, uint32_t *result);

/* CRC 주변장치에서 독립 계산한다. data 주소의 정렬은 요구하지 않지만 length는
 * 4의 배수이며 최대 4096 bytes다. zero length/data=NULL은 빈 CRC를 계산한다.
 * 단일 MCU 코어에서 짧은 PRIMASK 구간으로 공용 소유권을 얻으며 이미 사용 중이면
 * 기다리지 않고 BUSY를 반환한다. 계산 중 IRQ는 원래 상태로 유지한다.
 * CRC clock enable의 진입 상태를 복원하고 RCC의 다른 bit는 보존한다. CRC DR/CR은
 * 변경되며 이전 누적값은 복원하지 않는다. CRC IDR/옵션 바이트/플래시는 건드리지 않는다.
 * 이 API끼리만 lock을 공유한다. HAL_CRC_*, MX_CRC_Init/DeInit, 직접 레지스터 사용과
 * 동시 호출하면 안 된다. NMI/fault handler에서는 사용하지 않는다. BSP 자체에 RTOS,
 * HAL tick, watchdog, 대기 루프 의존성은 없다. 실패 시 *result를 보존한다. */
BSP_CRC_Status BSP_CRC_TryHardwareWords(const void *data, size_t length,
                                       uint32_t *result);

/* 같은 불변 입력을 SW와 HW로 계산하여 비교한다. SW/HW가 다르면 MISMATCH와
 * 두 값을 반환한다. 그 밖의 오류는 comparison을 보존한다. HW 호출과 같은 길이/
 * 동시 사용 계약이다. 실기에 직접 호출한 결과만 실물 CRC 검증이라고 보고한다. */
BSP_CRC_Status BSP_CRC_CompareHardwareWords(const void *data, size_t length,
                                           BSP_CRC_Comparison *comparison);

#ifdef __cplusplus
}
#endif
#endif
