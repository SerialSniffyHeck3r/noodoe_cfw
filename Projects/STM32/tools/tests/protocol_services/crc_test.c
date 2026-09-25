#include "BSP_CRC.h"
#include "crc_test_port.h"

volatile uint32_t crc_test_assertions;
volatile uint32_t crc_test_failure_line;
volatile uint32_t crc_test_external_result;
static uint32_t primask, clocks, dr, reset_count, write_count, fault_count;
static uint32_t observed_words[1024];
static uint32_t expected_primask, nested_trigger, nested_status, nested_result;
static uint32_t change_other_clock, inject_mismatch;

#define CHECK(condition) do { ++crc_test_assertions; if (!(condition)) { \
    crc_test_failure_line = __LINE__; return __LINE__; } } while (0)

/* 검증 모델은 nibble table을 사용한다. 제품 구현의 32회 bit loop를 호출하지 않는다.
 * 표는 polynomial long division으로 얻은 16개 항이며 word의 MS nibble부터 처리한다. */
static uint32_t ReferenceWord(uint32_t state, uint32_t word)
{
    static const uint32_t table[16] = {
        0x00000000UL, 0x04C11DB7UL, 0x09823B6EUL, 0x0D4326D9UL,
        0x130476DCUL, 0x17C56B6BUL, 0x1A864DB2UL, 0x1E475005UL,
        0x2608EDB8UL, 0x22C9F00FUL, 0x2F8AD6D6UL, 0x2B4BCB61UL,
        0x350C9B64UL, 0x31CD86D3UL, 0x3C8EA00AUL, 0x384FBDBDUL
    };
    uint32_t index;
    for (index = 0U; index < 8U; ++index) {
        state = (state << 4U) ^ table[((state >> 28U) ^ (word >> 28U)) & 15U];
        word <<= 4U;
    }
    return state;
}

/* 모의 PRIMASK를 조회한다. 제품 코드가 바깥 호출자의 IRQ 상태를 보존하는지 검사한다. */
uint32_t CrcTest_MaskGet(void) { return primask; }

/* 모의 PRIMASK를 설정한다. 이미 금지된 상태가 임의 해제되면 후속 word 검사가 잡는다. */
void CrcTest_MaskSet(uint32_t value) { primask = value; }

/* 모의 critical section에 진입한다. IRQ가 실제로 차단되는 것은 ARM 제품 경로 시험과
 * 다르므로 이 시험은 BSP의 저장/복원 제어 흐름만 확인한다. */
void CrcTest_IrqDisable(void) { primask = 1U; }

/* barrier 자체의 CPU ordering은 모델링하지 않는다. MCU 메모리 모델 증명은 범위 밖이다. */
void CrcTest_Barrier(void) { }

/* DSB 호출 경계만 제공한다. AHB bus 지연이나 CRC 실제 처리 시간은 모의하지 않는다. */
void CrcTest_Sync(void) { }

/* 현재 RCC 모형을 반환한다. CRC 외 clock bit가 보존되는지 결과에서 대조한다. */
uint32_t CrcTest_ClockRead(void) { return clocks; }

/* clock RMW가 critical section 안에서만 수행되는지 검사하며 RCC 모형을 갱신한다. */
void CrcTest_ClockWrite(uint32_t value)
{
    if (primask != 1U) { ++fault_count; }
    clocks = value;
}

/* CRC 계산 reset을 모의한다. 호출당 한 번이며 clock이 켜진 후여야 한다. */
void CrcTest_Reset(void)
{
    if ((clocks & CRC_PORT_CLOCK_BIT) == 0U || primask != expected_primask) {
        ++fault_count;
    }
    ++reset_count;
    dr = BSP_CRC_INITIAL;
}

/* DR 쓰기마다 별도 표 모델을 계산한다. 선택적으로 재진입 호출을 넣어 BUSY 즉시
 * 반환과 외부 clock bit 변경의 보존을 확인한다. 재진입 입력/출력은 원 계산과 별개다. */
void CrcTest_WordWrite(uint32_t value)
{
    static const uint8_t probe[4] = {0x78U, 0x56U, 0x34U, 0x12U};
    if ((clocks & CRC_PORT_CLOCK_BIT) == 0U || primask != expected_primask) {
        ++fault_count;
    }
    if (write_count < 1024U) { observed_words[write_count] = value; }
    ++write_count;
    if (nested_trigger != 0U) {
        nested_trigger = 0U;
        nested_result = 0xDEADBEEFUL;
        nested_status = (uint32_t)BSP_CRC_TryHardwareWords(probe, 4U, &nested_result);
    }
    if (change_other_clock != 0U) { clocks |= 1UL << 19U; }
    dr = ReferenceWord(dr, value);
}

/* 결과 레지스터를 읽는다. 오류 주입은 비교 API가 불일치를 노출하는지 확인한다. */
uint32_t CrcTest_ResultRead(void) { return dr ^ inject_mismatch; }

/* 각 시험의 peripheral 경계를 초기화한다. BSP 내부 lock은 직접 변경하지 않으므로
 * 앞 호출이 lock을 풀지 않으면 다음 정상 호출이 BUSY로 실패한다. */
static void ResetPort(uint32_t mask, uint32_t enabled)
{
    primask = mask;
    expected_primask = mask;
    clocks = 0xA0000005UL | (enabled != 0U ? CRC_PORT_CLOCK_BIT : 0U);
    dr = 0x11223344UL;
    reset_count = 0U;
    write_count = 0U;
    fault_count = 0U;
    nested_trigger = 0U;
    nested_status = 0U;
    change_other_clock = 0U;
    inject_mismatch = 0U;
}

/* 독립 long-division으로 고정한 알려진 답, 끝 CRC word를 포함한 residue 0,
 * endian 순서 및 software/HW 분리 계약을 실제 BSP 구현으로 검사한다. */
static uint32_t TestKnownVectors(void)
{
    static const uint8_t zero[4] = {0U, 0U, 0U, 0U};
    static const uint8_t ff[4] = {255U, 255U, 255U, 255U};
    static const uint8_t le[4] = {0x78U, 0x56U, 0x34U, 0x12U};
    static const uint8_t digits[9] = "123456789";
    BSP_CRC_Context context;
    uint32_t result, i;
    uint8_t trailer[4];
    BSP_CRC_Init(NULL);
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0xFFFFFFFFUL);
    CHECK(BSP_CRC_Update(&context, NULL, 0U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Update(&context, zero, 4U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0xC704DD7BUL);
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Update(&context, ff, 4U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0U);
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Update(&context, le, 4U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0xDF8A8A2BUL);
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Update(&context, digits, 8U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0xFEFC54F9UL);
    for (i = 0U; i < 4U; ++i) { trailer[i] = (uint8_t)(result >> (i * 8U)); }
    CHECK(BSP_CRC_Update(&context, trailer, 4U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
    CHECK(result == 0U);
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Update(&context, digits, 9U) == BSP_CRC_OK);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_PAD_FF, &result) == BSP_CRC_OK);
    CHECK(result == 0xD9020D98UL);
    CHECK(context.total_bytes == 9U && context.pending_bytes == 1U);
    return 0U;
}

/* 가능한 모든 단일 분할 위치와 1..19-byte 청크에서 같은 실제 256-byte 입력을
 * 계산한다. 두 context를 교차 갱신하고 Final 뒤 이어 쓰기를 검증한다. */
static uint32_t TestStreaming(void)
{
    uint8_t buffer[258];
    BSP_CRC_Context first, second;
    uint32_t i, split, width, offset, result, result2, reference;
    for (i = 0U; i < 258U; ++i) { buffer[i] = (uint8_t)(i - 1U); }
    for (split = 0U; split <= 256U; ++split) {
        BSP_CRC_Init(&first);
        BSP_CRC_Init(&second);
        CHECK(BSP_CRC_Update(&first, buffer + 1U, split) == BSP_CRC_OK);
        CHECK(BSP_CRC_Update(&second, buffer + 1U, 16U) == BSP_CRC_OK);
        CHECK(BSP_CRC_Update(&first, buffer + 1U + split, 256U - split) == BSP_CRC_OK);
        CHECK(BSP_CRC_Final(&first, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
        CHECK(result == 0xB7EC66F4UL);
        CHECK(BSP_CRC_Final(&second, BSP_CRC_REQUIRE_FULL_WORDS, &result2) == BSP_CRC_OK);
        CHECK(result2 == 0x081B46CAUL);
    }
    for (width = 1U; width <= 19U; ++width) {
        BSP_CRC_Init(&first);
        for (offset = 0U; offset < 256U; offset += width) {
            uint32_t count = 256U - offset < width ? 256U - offset : width;
            CHECK(BSP_CRC_Update(&first, buffer + 1U + offset, count) == BSP_CRC_OK);
        }
        CHECK(BSP_CRC_Final(&first, BSP_CRC_PAD_FF, &result) == BSP_CRC_OK);
        CHECK(result == 0xB7EC66F4UL);
    }
    for (width = 1U; width <= 3U; ++width) {
        BSP_CRC_Init(&first);
        CHECK(BSP_CRC_Update(&first, buffer + 1U, width) == BSP_CRC_OK);
        result = 0x76543210UL;
        CHECK(BSP_CRC_Final(&first, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_PARTIAL_WORD);
        CHECK(result == 0x76543210UL);
        CHECK(BSP_CRC_Final(&first, BSP_CRC_PAD_FF, &result) == BSP_CRC_OK);
        reference = 0xFFFFFFFFUL;
        for (i = 0U; i < width; ++i) {
            reference = (reference & ~(0xFFUL << (8U * i))) | ((uint32_t)buffer[i + 1U] << (8U * i));
        }
        CHECK(result == ReferenceWord(0xFFFFFFFFUL, reference));
        CHECK(first.total_bytes == width && first.pending_bytes == width);
        CHECK(BSP_CRC_Update(&first, buffer + 1U + width, 16U - width) == BSP_CRC_OK);
        CHECK(BSP_CRC_Final(&first, BSP_CRC_REQUIRE_FULL_WORDS, &result) == BSP_CRC_OK);
        CHECK(result == 0x081B46CAUL);
    }
    return 0U;
}

/* 잘못된 길이/인수에서는 메모리와 출력이 보존되는지 확인한다. overflow 입력은
 * 큰 포인터 범위를 실제 읽지 않아도 사전 검사에서 거부되어야 한다. */
static uint32_t TestInvalid(void)
{
    BSP_CRC_Context context;
    BSP_CRC_Comparison comparison = {1U, 2U, 3U};
    uint32_t result = 0x1234UL;
    uint8_t byte = 0U;
    BSP_CRC_Init(&context);
    CHECK(BSP_CRC_Update(NULL, &byte, 1U) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_Update(&context, NULL, 1U) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(context.total_bytes == 0U && context.state == BSP_CRC_INITIAL);
    CHECK(BSP_CRC_Final(NULL, BSP_CRC_PAD_FF, &result) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_PAD_FF, NULL) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_Final(&context, (BSP_CRC_FinalMode)99, &result) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(result == 0x1234UL);
    context.total_bytes = UINT32_MAX - 3U;
    CHECK(BSP_CRC_Update(&context, &byte, 4U) == BSP_CRC_LENGTH_OVERFLOW);
    CHECK(context.total_bytes == UINT32_MAX - 3U && context.state == BSP_CRC_INITIAL);
    context.pending_bytes = 4U;
    CHECK(BSP_CRC_Update(&context, NULL, 0U) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_Final(&context, BSP_CRC_PAD_FF, &result) == BSP_CRC_INVALID_ARGUMENT);
    ResetPort(0U, 0U);
    CHECK(BSP_CRC_TryHardwareWords(NULL, 4U, &result) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_TryHardwareWords(&byte, 3U, &result) == BSP_CRC_PARTIAL_WORD);
    CHECK(BSP_CRC_TryHardwareWords(&byte, 4100U, &result) == BSP_CRC_TOO_LARGE);
    CHECK(BSP_CRC_TryHardwareWords(&byte, 4U, NULL) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_CompareHardwareWords(&byte, 4U, NULL) == BSP_CRC_INVALID_ARGUMENT);
    CHECK(BSP_CRC_CompareHardwareWords(&byte, 3U, &comparison) == BSP_CRC_PARTIAL_WORD);
    CHECK(comparison.software == 1U && comparison.hardware == 2U && comparison.bytes == 3U);
    CHECK(reset_count == 0U && write_count == 0U && result == 0x1234UL);
    CHECK(clocks == 0xA0000005UL && primask == 0U);
    return 0U;
}

/* 실제 HW 계산 경로를 모의 레지스터에서 실행한다. 정렬/word 순서, 원래 켜진/꺼진
 * clock, IRQ 허용/금지 상태, 중복 호출, 다른 RCC bit, 불일치 보고를 각각 검사한다. */
static uint32_t TestHardwareBoundary(void)
{
    uint8_t bytes[4097];
    BSP_CRC_Comparison comparison;
    uint32_t i, mask, enabled, result;
    for (i = 0U; i < 4097U; ++i) { bytes[i] = (uint8_t)(i - 1U); }
    for (mask = 0U; mask < 2U; ++mask) {
        for (enabled = 0U; enabled < 2U; ++enabled) {
            ResetPort(mask, enabled);
            nested_trigger = 1U;
            change_other_clock = 1U;
            CHECK(BSP_CRC_TryHardwareWords(bytes + 1U, 16U, &result) == BSP_CRC_OK);
            CHECK(result == 0x081B46CAUL);
            CHECK(observed_words[0] == 0x03020100UL && observed_words[3] == 0x0F0E0D0CUL);
            CHECK(reset_count == 1U && write_count == 4U && fault_count == 0U);
            CHECK(nested_status == BSP_CRC_BUSY && nested_result == 0xDEADBEEFUL);
            CHECK(primask == mask);
            CHECK(clocks == (0xA0080005UL | (enabled != 0U ? CRC_PORT_CLOCK_BIT : 0U)));
            CHECK(BSP_CRC_TryHardwareWords(NULL, 0U, &result) == BSP_CRC_OK);
            CHECK(result == BSP_CRC_INITIAL && reset_count == 2U);
        }
    }
    ResetPort(0U, 0U);
    CHECK(BSP_CRC_CompareHardwareWords(bytes + 1U, 4096U, &comparison) == BSP_CRC_OK);
    CHECK(comparison.software == comparison.hardware && comparison.bytes == 4096U);
    CHECK(write_count == 1024U && reset_count == 1U && fault_count == 0U);
    inject_mismatch = 1U;
    CHECK(BSP_CRC_CompareHardwareWords(bytes + 1U, 16U, &comparison) == BSP_CRC_MISMATCH);
    CHECK(comparison.software == 0x081B46CAUL && comparison.hardware == 0x081B46CBUL);
    CHECK(primask == 0U && clocks == 0xA0000005UL);
    return 0U;
}

/* ELF 진입점: hardware나 HAL 호출 대신 경계 모형 위의 실제 BSP C를 실행한다.
 * 실패 위치를 반환하므로 runner가 어느 CHECK에서 실패했는지 그대로 출력한다. */
uint32_t crc_test_main(void)
{
    uint32_t result;
    crc_test_assertions = 0U;
    crc_test_failure_line = 0U;
    result = TestKnownVectors(); if (result != 0U) { return result; }
    result = TestStreaming(); if (result != 0U) { return result; }
    result = TestInvalid(); if (result != 0U) { return result; }
    return TestHardwareBoundary();
}

/* Python runner가 읽은 원본 OTA 파일을 실제 소프트웨어 C에 넘기는 별도 진입점이다.
 * 메모리를 수정하지 않고 389-byte 청크로 처리하여 실제 trailer 범위의 중간 경계가
 * word 경계와 달라도 맞는지 확인한다. 파일 선택/기대 CRC는 runner가 독립 비교한다. */
uint32_t crc_test_external(const uint8_t *data, uint32_t length)
{
    BSP_CRC_Context context;
    uint32_t offset = 0U;
    BSP_CRC_Status status;
    BSP_CRC_Init(&context);
    while (offset < length) {
        uint32_t size = length - offset < 389U ? length - offset : 389U;
        status = BSP_CRC_Update(&context, data + offset, size);
        if (status != BSP_CRC_OK) { return (uint32_t)status; }
        offset += size;
    }
    return (uint32_t)BSP_CRC_Final(&context, BSP_CRC_REQUIRE_FULL_WORDS,
                                  (uint32_t *)&crc_test_external_result);
}
