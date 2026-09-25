#include "BSP_CRC.h"

/* 시험 빌드만 레지스터 경계를 대역으로 바꾼다. 제품 빌드는 CMSIS의 실제 레지스터와
 * IRQ 명령을 사용한다. 계산 알고리즘/잠금/clock 복원 제어 흐름은 양쪽이 동일하다. */
#ifdef BSP_CRC_TEST_PORT
#include "crc_test_port.h"
#else
#include "stm32f4xx.h"
#define CRC_PORT_MASK_GET() __get_PRIMASK()
#define CRC_PORT_MASK_SET(value) __set_PRIMASK(value)
#define CRC_PORT_IRQ_DISABLE() __disable_irq()
#define CRC_PORT_BARRIER() __DMB()
#define CRC_PORT_SYNC() __DSB()
#define CRC_PORT_CLOCK_READ() (RCC->AHB1ENR)
#define CRC_PORT_CLOCK_WRITE(value) (RCC->AHB1ENR = (value))
#define CRC_PORT_CLOCK_BIT RCC_AHB1ENR_CRCEN
#define CRC_PORT_RESET() (CRC->CR = CRC_CR_RESET)
#define CRC_PORT_WORD_WRITE(value) (CRC->DR = (value))
#define CRC_PORT_RESULT_READ() (CRC->DR)
#endif

/* CRC는 MCU에 하나뿐이다. 소프트웨어 context는 이 전역을 사용하지 않는다.
 * 이 flag의 검사/변경은 IRQ를 막은 짧은 구간에서만 하며 spin 대기를 하지 않는다. */
static volatile uint32_t hardware_owned;

/* 완성된 하나의 32-bit word를 처리한다. unsigned shift는 32-bit를 넘는 항을
 * 버린다. 각 단계에서 이동 전 bit31을 검사하여 x^32 항에 polynomial을 XOR한다.
 * SR1 독립 검증 자료의 polynomial long division과 같은 결과를 내는 bit 구현이다. */
static uint32_t ProcessWord(uint32_t state, uint32_t word)
{
    uint32_t bit;
    state ^= word;
    for (bit = 0U; bit < 32U; ++bit) {
        uint32_t carry = state & 0x80000000UL;
        state <<= 1U;
        if (carry != 0U) {
            state ^= BSP_CRC_POLYNOMIAL;
        }
    }
    return state;
}

/* 정렬되지 않은 포인터를 uint32_t*로 변환하지 않고 byte 순서를 명시한다.
 * 입력 data[0]은 word의 bit0..7이다. CPU의 unaligned load 옵션에 의존하지 않는다. */
static uint32_t ReadWordLE(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U)
           | ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

/* 공개 Init 계약: 다른 context와 하드웨어에는 부작용이 없으며 zero length Final은
 * FFFFFFFF를 반환한다. pending_word의 사용하지 않은 상위 bit도 0으로 초기화한다. */
void BSP_CRC_Init(BSP_CRC_Context *context)
{
    if (context != NULL) {
        context->state = BSP_CRC_INITIAL;
        context->total_bytes = 0U;
        context->pending_word = 0U;
        context->pending_bytes = 0U;
    }
}

/* 읽기 시작 전에 길이/상태를 검사하므로 실패는 부분 누적을 남기지 않는다.
 * word 단위 경로가 큰 이미지의 byte별 분기 비용을 줄이고 앞뒤 1..3 bytes만
 * pending_word에 모은다. 누계 단위는 padding을 포함하지 않은 byte다. */
BSP_CRC_Status BSP_CRC_Update(BSP_CRC_Context *context, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t remaining = length;
    if (context == NULL || (data == NULL && length != 0U)
        || context->pending_bytes > 3U
        || (context->total_bytes & 3U) != context->pending_bytes) {
        return BSP_CRC_INVALID_ARGUMENT;
    }
    if (length > UINT32_MAX - context->total_bytes) {
        return BSP_CRC_LENGTH_OVERFLOW;
    }
    while (remaining != 0U) {
        if (context->pending_bytes == 0U && remaining >= 4U) {
            context->state = ProcessWord(context->state, ReadWordLE(bytes));
            bytes += 4U;
            remaining -= 4U;
        } else {
            context->pending_word |= (uint32_t)*bytes << (context->pending_bytes * 8U);
            ++bytes;
            --remaining;
            ++context->pending_bytes;
            if (context->pending_bytes == 4U) {
                context->state = ProcessWord(context->state, context->pending_word);
                context->pending_word = 0U;
                context->pending_bytes = 0U;
            }
        }
    }
    context->total_bytes += (uint32_t)length;
    return BSP_CRC_OK;
}

/* Final은 원본 상태를 봉인하지 않는다. 따라서 부분 word FF padding 조회 후 실제
 * 다음 bytes가 오면 원래 pending bytes 뒤로 정상 누적된다. padding 모드는 파일
 * 포맷 정책을 대신 추정하지 않으며 호출자가 명시적으로 선택해야 한다. */
BSP_CRC_Status BSP_CRC_Final(const BSP_CRC_Context *context,
                            BSP_CRC_FinalMode mode, uint32_t *result)
{
    uint32_t value;
    if (context == NULL || result == NULL || context->pending_bytes > 3U
        || (context->total_bytes & 3U) != context->pending_bytes
        || (mode != BSP_CRC_REQUIRE_FULL_WORDS && mode != BSP_CRC_PAD_FF)) {
        return BSP_CRC_INVALID_ARGUMENT;
    }
    if (context->pending_bytes != 0U && mode == BSP_CRC_REQUIRE_FULL_WORDS) {
        return BSP_CRC_PARTIAL_WORD;
    }
    value = context->state;
    if (context->pending_bytes != 0U) {
        /* pending_bytes는 1..3이므로 shift 폭은 8/16/24이고 32-bit shift가 없다. */
        uint32_t word = context->pending_word
                        | (0xFFFFFFFFUL << (8U * context->pending_bytes));
        value = ProcessWord(value, word);
    }
    *result = value;
    return BSP_CRC_OK;
}

/* 두 HW 공개 API의 공통 인수 검증이다. 단위는 byte이며 HW는 padding을 하지 않는다.
 * BUSY 확인 전에도 입력이 잘못되면 명확한 인수 오류를 반환한다. */
static BSP_CRC_Status ValidateHardware(const void *data, size_t length)
{
    if (data == NULL && length != 0U) {
        return BSP_CRC_INVALID_ARGUMENT;
    }
    if ((length & 3U) != 0U) {
        return BSP_CRC_PARTIAL_WORD;
    }
    if (length > BSP_CRC_HW_MAX_BYTES) {
        return BSP_CRC_TOO_LARGE;
    }
    return BSP_CRC_OK;
}

/* clock 켜기와 소유권 획득을 하나의 짧은 critical section에서 한다. 기존 PRIMASK를
 * 정확히 복원하므로 IRQ가 금지된 호출자에서 임의로 IRQ를 켜지 않는다. CRC clock을
 * 켠 뒤 read-back과 DSB를 수행하여 peripheral 접근 전에 RCC 쓰기를 완료한다. */
static BSP_CRC_Status TryAcquireHardware(uint32_t *clock_was_enabled)
{
    uint32_t mask = CRC_PORT_MASK_GET();
    uint32_t clocks;
    CRC_PORT_IRQ_DISABLE();
    if (hardware_owned != 0U) {
        CRC_PORT_MASK_SET(mask);
        return BSP_CRC_BUSY;
    }
    hardware_owned = 1U;
    CRC_PORT_BARRIER();
    clocks = CRC_PORT_CLOCK_READ();
    *clock_was_enabled = clocks & CRC_PORT_CLOCK_BIT;
    CRC_PORT_CLOCK_WRITE(clocks | CRC_PORT_CLOCK_BIT);
    (void)CRC_PORT_CLOCK_READ();
    CRC_PORT_SYNC();
    CRC_PORT_MASK_SET(mask);
    return BSP_CRC_OK;
}

/* 진입 때 꺼져 있던 CRC clock만 다시 끈다. 현재 RCC 값을 RMW하므로 계산 도중 다른
 * 주변장치가 켠 clock bit는 보존한다. lock 해제 전에 MMIO 처리를 끝내 다음 호출이
 * 이전 계산과 겹치지 않게 한다. DR 값은 저장 가능한 context로 취급하지 않는다. */
static void ReleaseHardware(uint32_t clock_was_enabled)
{
    uint32_t mask = CRC_PORT_MASK_GET();
    CRC_PORT_IRQ_DISABLE();
    if (clock_was_enabled == 0U) {
        CRC_PORT_CLOCK_WRITE(CRC_PORT_CLOCK_READ() & ~CRC_PORT_CLOCK_BIT);
    }
    CRC_PORT_SYNC();
    hardware_owned = 0U;
    CRC_PORT_BARRIER();
    CRC_PORT_MASK_SET(mask);
}

/* clock와 lock을 확보한 동안만 RESET 및 DR를 쓴다. 최대로 1024 words를 처리하고
 * busy flag를 polling하지 않으므로 계산 길이가 제한된다. CRC reset은 CRC 계산기
 * 리셋이며 MCU 리셋이 아니다. 타깃 메모리에는 쓰기/삭제를 수행하지 않는다. */
BSP_CRC_Status BSP_CRC_TryHardwareWords(const void *data, size_t length,
                                       uint32_t *result)
{
    const uint8_t *bytes = (const uint8_t *)data;
    BSP_CRC_Status status;
    uint32_t clock_was_enabled;
    uint32_t value;
    size_t index;
    if (result == NULL) {
        return BSP_CRC_INVALID_ARGUMENT;
    }
    status = ValidateHardware(data, length);
    if (status != BSP_CRC_OK) {
        return status;
    }
    status = TryAcquireHardware(&clock_was_enabled);
    if (status != BSP_CRC_OK) {
        return status;
    }
    CRC_PORT_RESET();
    for (index = 0U; index < length; index += 4U) {
        CRC_PORT_WORD_WRITE(ReadWordLE(bytes + index));
    }
    value = CRC_PORT_RESULT_READ();
    ReleaseHardware(clock_was_enabled);
    *result = value;
    return BSP_CRC_OK;
}

/* 소프트웨어 계산에 HW 소유권을 잡아 두지 않는다. HW가 다른 호출에 사용 중이면
 * BUSY로 반환하고 comparison을 건드리지 않는다. mismatch는 두 계산이 완료된
 * 진단 결과이므로 비교 값을 제공한다. 입력은 전체 함수가 반환할 때까지 불변이다. */
BSP_CRC_Status BSP_CRC_CompareHardwareWords(const void *data, size_t length,
                                           BSP_CRC_Comparison *comparison)
{
    BSP_CRC_Context context;
    BSP_CRC_Comparison next;
    BSP_CRC_Status status;
    if (comparison == NULL) {
        return BSP_CRC_INVALID_ARGUMENT;
    }
    status = ValidateHardware(data, length);
    if (status != BSP_CRC_OK) {
        return status;
    }
    BSP_CRC_Init(&context);
    status = BSP_CRC_Update(&context, data, length);
    if (status != BSP_CRC_OK) {
        return status;
    }
    status = BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &next.software);
    if (status != BSP_CRC_OK) {
        return status;
    }
    status = BSP_CRC_TryHardwareWords(data, length, &next.hardware);
    if (status != BSP_CRC_OK) {
        return status;
    }
    next.bytes = (uint32_t)length;
    *comparison = next;
    return next.software == next.hardware ? BSP_CRC_OK : BSP_CRC_MISMATCH;
}
