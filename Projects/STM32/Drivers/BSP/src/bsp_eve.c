/* Noodoe 원본 V5.16의 EVE 설정 + Bridgetek FT81x 명세를 바탕으로 작성한 최소 BSP.
 * 공개 라이브러리 코드는 포함/복사하지 않았다. 참고 원본·주소·라이선스:
 * Reversing/analysis/2026-09-12-lcd-bringup/eve-reference.md.
 */
#include "bsp_eve.h"
#include "spi.h"
#include <stddef.h>

#define EVE_RAM_DL          0x00300000UL
#define EVE_CHIP_ID         0x000C0000UL
#define EVE_REG_ID          0x00302000UL
#define EVE_REG_FRAMES      0x00302004UL
#define EVE_REG_FREQUENCY   0x0030200CUL
#define EVE_REG_CPURESET    0x00302020UL
#define EVE_REG_HCYCLE      0x0030202CUL
#define EVE_REG_HOFFSET     0x00302030UL
#define EVE_REG_HSIZE       0x00302034UL
#define EVE_REG_HSYNC0      0x00302038UL
#define EVE_REG_HSYNC1      0x0030203CUL
#define EVE_REG_VCYCLE      0x00302040UL
#define EVE_REG_VOFFSET     0x00302044UL
#define EVE_REG_VSIZE       0x00302048UL
#define EVE_REG_VSYNC0      0x0030204CUL
#define EVE_REG_VSYNC1      0x00302050UL
#define EVE_REG_DLSWAP      0x00302054UL
#define EVE_REG_DITHER      0x00302060UL
#define EVE_REG_SWIZZLE     0x00302064UL
#define EVE_REG_CSPREAD     0x00302068UL
#define EVE_REG_PCLK_POL    0x0030206CUL
#define EVE_REG_PCLK        0x00302070UL
#define EVE_REG_GPIO        0x00302094UL
#define EVE_SPI_TIMEOUT_MS  25U
#define EVE_READY_WAIT_MS   500U
#define EVE_SWAP_WAIT_MS    250U

/* 하나의 호출을 한 번만 평가하고 첫 실패에서 상위 함수를 반환한다. 하위 함수가
 * g_bsp_eve에 원인/주소를 저장하므로 이후 명령으로 실패 증거를 덮어쓰지 않는다. */
#define EVE_TRY(operation) do { \
    BSP_EVE_Status eve_status_ = (operation); \
    if (eve_status_ != BSP_EVE_OK) { return eve_status_; } \
} while (0)

/* VERTEX2F는 1/16 pixel 좌표다. 이 파일의 0..480 상수만 넣으므로 signed 15bit
 * 범위를 넘지 않는다. 표시 명령 1 word이며 CPU endian과 무관하게 아래에서 직렬화한다. */
#define EVE_VERTEX(x, y) (0x40000000UL | ((uint32_t)(x) * 16UL << 15U) | ((uint32_t)(y) * 16UL))

volatile BSP_EVE_Diagnostics g_bsp_eve;

/* 실패 코드만 남기고 반환한다. GPIO/전원을 임의로 재시작하지 않아서 root 시험
 * 코드가 실제 실패 단계와 마지막 레지스터를 확인하고 후속 동작을 결정할 수 있다. */
static BSP_EVE_Status Eve_Fail(BSP_EVE_Status status)
{
    g_bsp_eve.result = (uint32_t)status;
    return status;
}

/* HAL의 ms timeout에는 실행 중인 HAL tick이 필요하다. ISR/IRQ 차단 영역에서는
 * 이 API를 사용하지 못하게 하며, 정상 태스크의 단독 SPI1 소유를 전제로 한다.
 * tick 자체의 정상 주기는 기존 bring-up 검증이 담당한다. */
static BSP_EVE_Status Eve_CheckContext(void)
{
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U)) {
        return Eve_Fail(BSP_EVE_ERROR_CONTEXT);
    }
    return BSP_EVE_OK;
}

/* 최대 8byte의 완결된 SPI transaction이다. tx/rx는 호출자 스택 버퍼이며 동기식
 * HAL TxRx가 끝난 뒤 재사용할 수 있다. read/write 모두 RX를 비워 OVR을 방지한다.
 * 성공/오류 모두 CS를 HIGH로 해제하고 HAL 상태/오류 비트를 기록한다. */
static BSP_EVE_Status Eve_Transfer(uint8_t *tx, uint8_t *rx, uint16_t length)
{
    HAL_StatusTypeDef status;
    if ((tx == NULL) || (rx == NULL) || (length == 0U) || (length > 8U)) {
        return Eve_Fail(BSP_EVE_ERROR_ARGUMENT);
    }
    EVE_TRY(Eve_CheckContext());
    if ((hspi1.Instance != SPI1) || (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY)) {
        return Eve_Fail(BSP_EVE_ERROR_NOT_READY);
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, length, EVE_SPI_TIMEOUT_MS);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    g_bsp_eve.transfer_count++;
    g_bsp_eve.spi_status = (uint32_t)status;
    g_bsp_eve.spi_error = HAL_SPI_GetError(&hspi1);
    return (status == HAL_OK) ? BSP_EVE_OK : Eve_Fail(BSP_EVE_ERROR_SPI);
}

/* FT81x memory write: 주소는 상위 byte부터 3byte, write bit는 첫 byte의 bit7,
 * 데이터는 little-endian이다. 1/2/4byte 레지스터와 RAM_DL word만 지원하며 길이가
 * 잘못되면 CS를 내리지 않는다. STM32 플래시나 EVE 비휘발성 저장소는 접근하지 않는다. */
static BSP_EVE_Status Eve_Write(uint32_t address, uint32_t value, uint8_t width)
{
    uint8_t tx[8] = {0U}, rx[8] = {0U};
    if ((address > 0x003FFFFFUL) || ((width != 1U) && (width != 2U) && (width != 4U))) {
        return Eve_Fail(BSP_EVE_ERROR_ARGUMENT);
    }
    tx[0] = (uint8_t)((address >> 16U) | 0x80U);
    tx[1] = (uint8_t)(address >> 8U);
    tx[2] = (uint8_t)address;
    for (uint8_t index = 0U; index < width; index++) {
        tx[3U + index] = (uint8_t)(value >> (8U * index));
    }
    g_bsp_eve.last_address = address;
    g_bsp_eve.last_value = value;
    return Eve_Transfer(tx, rx, (uint16_t)(3U + width));
}

/* FT81x memory read에는 주소3byte 다음 dummy1byte가 필요하다. 유효 데이터는
 * rx[4]부터이며 low byte 먼저 조합한다. 전송 실패 시 *value를 바꾸지 않는다. */
static BSP_EVE_Status Eve_Read(uint32_t address, uint32_t *value, uint8_t width)
{
    uint8_t tx[8] = {0U}, rx[8] = {0U};
    uint32_t result = 0U;
    if ((value == NULL) || (address > 0x003FFFFFUL) ||
        ((width != 1U) && (width != 2U) && (width != 4U))) {
        return Eve_Fail(BSP_EVE_ERROR_ARGUMENT);
    }
    tx[0] = (uint8_t)(address >> 16U);
    tx[1] = (uint8_t)(address >> 8U);
    tx[2] = (uint8_t)address;
    g_bsp_eve.last_address = address;
    EVE_TRY(Eve_Transfer(tx, rx, (uint16_t)(4U + width)));
    for (uint8_t index = 0U; index < width; index++) {
        result |= (uint32_t)rx[4U + index] << (8U * index);
    }
    *value = result;
    g_bsp_eve.last_value = result;
    return BSP_EVE_OK;
}

/* host command는 주소형 memory transaction과 달리 command, parameter, 0의
 * 3byte다. 순정의 44/62/00 명령은 parameter=0이며 CS 한 번으로 각각 보낸다. */
static BSP_EVE_Status Eve_HostCommand(uint8_t command)
{
    uint8_t tx[3] = {command, 0U, 0U}, rx[3] = {0U};
    g_bsp_eve.last_address = 0U;
    g_bsp_eve.last_value = command;
    return Eve_Transfer(tx, rx, 3U);
}

/* REG_ID/CPURESET/DLSWAP의 byte 값이 expected가 될 때까지 최대 timeout_ms를
 * 기다린다. tick wrap은 unsigned 뺄셈으로 처리하고, 회수 제한도 별도로 둔다.
 * SPI 오류는 즉시 반환하며 값 불일치는 전용 timeout 오류로 구분한다. */
static BSP_EVE_Status Eve_WaitByte(uint32_t address, uint8_t expected,
                                   uint32_t timeout_ms, BSP_EVE_Status timeout_error)
{
    uint32_t started = HAL_GetTick();
    for (uint32_t attempt = 0U; attempt <= timeout_ms; attempt++) {
        uint32_t value;
        EVE_TRY(Eve_Read(address, &value, 1U));
        if (address == EVE_REG_ID) { g_bsp_eve.reg_id = value; }
        if (address == EVE_REG_CPURESET) { g_bsp_eve.cpu_reset = value; }
        if (address == EVE_REG_DLSWAP) { g_bsp_eve.dl_swap = value; }
        if (value == expected) { return BSP_EVE_OK; }
        if ((HAL_GetTick() - started) >= timeout_ms) { break; }
        HAL_Delay(1U);
    }
    return Eve_Fail(timeout_error);
}

/* 다른 전원/버튼/SWD GPIO를 바꾸지 않고 PA4 CS와 PB1 PDN만 설정한다.
 * 출력 레벨을 먼저 HIGH로 기록하고 출력 모드로 바꿔 불필요한 LOW 펄스를 줄인다. */
static void Eve_InitControlPins(void)
{
    GPIO_InitTypeDef pin = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    pin.Pin = GPIO_PIN_4;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_PULLUP;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &pin);
    pin.Pin = GPIO_PIN_1;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &pin);
}

/* 원본 0801FD78..0801FF24의 480x480 timing을 레지스터별 폭으로 기록한다.
 * PCLK는 0으로 정지한 상태여야 하며 마지막 시작은 별도 단계에서 수행한다.
 * 이 값은 일반 480x480 패널 preset이 아니라 이 모듈의 순정 값이다. */
static BSP_EVE_Status Eve_WriteTiming(void)
{
    static const struct { uint32_t address; uint16_t value; uint8_t width; } settings[] = {
        {EVE_REG_HCYCLE, 550U, 2U}, {EVE_REG_HOFFSET, 37U, 2U},
        {EVE_REG_HSYNC0, 0U, 2U}, {EVE_REG_HSYNC1, 4U, 2U},
        {EVE_REG_VCYCLE, 505U, 2U}, {EVE_REG_VOFFSET, 18U, 2U},
        {EVE_REG_VSYNC0, 0U, 2U}, {EVE_REG_VSYNC1, 2U, 2U},
        {EVE_REG_SWIZZLE, 0U, 1U}, {EVE_REG_PCLK_POL, 0U, 1U},
        {EVE_REG_HSIZE, 480U, 2U}, {EVE_REG_VSIZE, 480U, 2U},
        {EVE_REG_CSPREAD, 0U, 1U}, {EVE_REG_DITHER, 1U, 1U}
    };
    for (size_t index = 0U; index < sizeof(settings) / sizeof(settings[0]); index++) {
        EVE_TRY(Eve_Write(settings[index].address, settings[index].value, settings[index].width));
    }
    return BSP_EVE_OK;
}

/* 8KiB RAM_DL에만 bounded 개수의 word를 little-endian으로 쓴다. 마지막 word는
 * DISPLAY(0)여야 GPU가 오래된 꼬리 명령을 실행하지 않는다. 첫/마지막 word를 readback
 * 하여 단순한 ID 응답과 실제 RAM 쓰기 성공을 구분한다. swap은 여기서 시작하지 않는다. */
static BSP_EVE_Status Eve_WriteDisplayList(const uint32_t *words, size_t count)
{
    uint32_t check;
    if ((words == NULL) || (count == 0U) || (count > 2048U) || (words[count - 1U] != 0U)) {
        return Eve_Fail(BSP_EVE_ERROR_ARGUMENT);
    }
    for (size_t index = 0U; index < count; index++) {
        EVE_TRY(Eve_Write(EVE_RAM_DL + (uint32_t)index * 4U, words[index], 4U));
    }
    EVE_TRY(Eve_Read(EVE_RAM_DL, &check, 4U));
    if (check != words[0]) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    EVE_TRY(Eve_Read(EVE_RAM_DL + (uint32_t)(count - 1U) * 4U, &check, 4U));
    if (check != 0U) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    g_bsp_eve.dl_words = (uint32_t)count;
    return BSP_EVE_OK;
}

/* 공개 Init: 순정 PDN/host/timing/PCLK/SPI속도 순서를 재현한다. REG_CPURESET 대기와
 * RAM readback은 실패 진단을 위해 추가했다. 생성 MX_SPI1_Init 내부 HAL init 실패는
 * 프로젝트 공통 Error_Handler가 처리한다. 일반 SPI/응답 실패는 enum으로 반환한다. */
BSP_EVE_Status BSP_EVE_Init(void)
{
    uint32_t value;
    BSP_EVE_Status final_status;
    static const uint32_t blank[] = {0x02000000UL, 0x26000007UL, 0x00000000UL};
    g_bsp_eve = (BSP_EVE_Diagnostics){0};
    g_bsp_eve.magic = 0x45564531UL;
    g_bsp_eve.version = 1U;
    EVE_TRY(Eve_CheckContext());
    g_bsp_eve.phase = 1U;
    Eve_InitControlPins();
    g_bsp_eve.phase = 2U;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_Delay(20U);
    MX_SPI1_Init(); /* 순정 최초 통신: APB2 84MHz /8 = 10.5MHz, boot 상한 11MHz 미만. */
    g_bsp_eve.phase = 3U;
    EVE_TRY(Eve_HostCommand(0x44U));
    EVE_TRY(Eve_HostCommand(0x62U)); /* 순정 legacy clock command; 실제 주파수는 아래에서 읽는다. */
    EVE_TRY(Eve_HostCommand(0x00U));
    HAL_Delay(300U);
    g_bsp_eve.phase = 4U;
    EVE_TRY(Eve_WaitByte(EVE_REG_ID, 0x7CU, EVE_READY_WAIT_MS, BSP_EVE_ERROR_ID_TIMEOUT));
    g_bsp_eve.phase = 5U;
    EVE_TRY(Eve_WaitByte(EVE_REG_CPURESET, 0U, EVE_READY_WAIT_MS, BSP_EVE_ERROR_CPU_TIMEOUT));
    EVE_TRY(Eve_Read(EVE_CHIP_ID, &value, 4U));
    g_bsp_eve.chip_id = value;
    EVE_TRY(Eve_Read(EVE_REG_FREQUENCY, &value, 4U));
    g_bsp_eve.frequency_hz = value;
    EVE_TRY(Eve_Write(EVE_REG_PCLK, 0U, 1U));
    g_bsp_eve.phase = 6U;
    EVE_TRY(Eve_WriteTiming());
    g_bsp_eve.phase = 7U;
    EVE_TRY(Eve_WriteDisplayList(blank, sizeof(blank) / sizeof(blank[0])));
    EVE_TRY(Eve_Write(EVE_REG_DLSWAP, 2U, 1U));
    /* board revision>=3(현재 strap6)의 LCD reset은 PC13이다. EVE GPIO7만 clear하고
     * 나머지 EVE GPIO bit는 보존한다. 구 revision의 패널 reset 용도와 혼용하지 않는다. */
    EVE_TRY(Eve_Read(EVE_REG_GPIO, &value, 1U));
    EVE_TRY(Eve_Write(EVE_REG_GPIO, value & ~0x80UL, 1U));
    g_bsp_eve.phase = 8U;
    EVE_TRY(Eve_Write(EVE_REG_PCLK, 3U, 1U));
    EVE_TRY(Eve_WaitByte(EVE_REG_DLSWAP, 0U, EVE_SWAP_WAIT_MS, BSP_EVE_ERROR_SWAP_TIMEOUT));
    /* 순정0801FF84처럼 통신만 /4로 올린다. MCU SYSCLK나 EVE pixel divider는 그대로다. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) { return Eve_Fail(BSP_EVE_ERROR_SPI); }
    EVE_TRY(Eve_Read(EVE_REG_HSIZE, &value, 2U));
    if (value != 480U) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    EVE_TRY(Eve_Read(EVE_REG_VSIZE, &value, 2U));
    if (value != 480U) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    g_bsp_eve.ready = 1U;
    final_status = BSP_EVE_RefreshDiagnostics();
    if (final_status != BSP_EVE_OK) {
        g_bsp_eve.ready = 0U;
        return final_status;
    }
    g_bsp_eve.phase = 9U;
    return BSP_EVE_OK;
}

/* 고정된 시험 도형만 정의한다. RECTS는 제목 띠, LINE_STRIP은 닫힌 창 테두리,
 * LINES는 중앙 교차선이다. 좌표/색 변화가 보이면 RGB 및 scanout 경로를 구분하기 쉽다.
 * 외부 이미지·폰트·GPU command FIFO를 사용하지 않는다. */
BSP_EVE_Status BSP_EVE_DrawTestWindow(void)
{
    static const uint32_t drawing[] = {
        0x02102030UL, 0x26000007UL, 0x27000004UL, 0x100000FFUL,
        0x041E3650UL, 0x0E000010UL, 0x1F000009UL,
        EVE_VERTEX(60U, 100U), EVE_VERTEX(420U, 380U), 0x21000000UL,
        0x0428A9D5UL, 0x1F000009UL,
        EVE_VERTEX(60U, 100U), EVE_VERTEX(420U, 145U), 0x21000000UL,
        0x04F0F4F8UL, 0x0E000020UL, 0x1F000004UL,
        EVE_VERTEX(60U, 100U), EVE_VERTEX(420U, 100U),
        EVE_VERTEX(420U, 380U), EVE_VERTEX(60U, 380U),
        EVE_VERTEX(60U, 100U), 0x21000000UL,
        0x04FFCC40UL, 0x1F000003UL,
        EVE_VERTEX(90U, 180U), EVE_VERTEX(390U, 350U),
        EVE_VERTEX(390U, 180U), EVE_VERTEX(90U, 350U), 0x21000000UL,
        0x044CE6A6UL, 0x1F000003UL,
        EVE_VERTEX(90U, 265U), EVE_VERTEX(390U, 265U), 0x21000000UL,
        0x00000000UL
    };
    return BSP_EVE_SubmitDisplayList(drawing, (uint32_t)(sizeof(drawing) / sizeof(drawing[0])));
}

/*
 * RAM_DL 접근/FRAME swap의 소유권을 한곳에 모은다. 앞선 swap이 아직 pending이면
 * RAM_DL을 덮어쓰지 않고 최대 250ms 먼저 기다린다. buffer에는 trusted BSP가 만든
 * DL만 들어온다. 명령 전체의 의미를 검증하는 범용 bytecode sandbox는 아니다.
 * 기존 DrawTestWindow의 phase/result/dl_words/ready 및 진단 레이아웃은 유지한다.
 */
BSP_EVE_Status BSP_EVE_SubmitDisplayList(const uint32_t *words, uint32_t count)
{
    if ((words == NULL) || (count == 0U) || (count > 2048U) || (words[count - 1U] != 0U)) {
        return Eve_Fail(BSP_EVE_ERROR_ARGUMENT);
    }
    if (g_bsp_eve.ready == 0U) { return Eve_Fail(BSP_EVE_ERROR_NOT_READY); }
    EVE_TRY(Eve_CheckContext());
    g_bsp_eve.phase = 10U;
    EVE_TRY(Eve_WaitByte(EVE_REG_DLSWAP, 0U, EVE_SWAP_WAIT_MS, BSP_EVE_ERROR_SWAP_TIMEOUT));
    EVE_TRY(Eve_WriteDisplayList(words, count));
    EVE_TRY(Eve_Write(EVE_REG_DLSWAP, 2U, 1U));
    EVE_TRY(Eve_WaitByte(EVE_REG_DLSWAP, 0U, EVE_SWAP_WAIT_MS, BSP_EVE_ERROR_SWAP_TIMEOUT));
    EVE_TRY(BSP_EVE_RefreshDiagnostics());
    g_bsp_eve.phase = 9U;
    return BSP_EVE_OK;
}

/* NULL/전송 실패를 처리한 뒤에만 caller의 byte를 바꾼다. ID가7C인지 판단하는
 * 정책은 Init/RefreshDiagnostics에 있으며 이 함수는 관측한 raw byte를 반환한다. */
BSP_EVE_Status BSP_EVE_ReadId(uint8_t *id)
{
    uint32_t value;
    if (id == NULL) { return Eve_Fail(BSP_EVE_ERROR_ARGUMENT); }
    EVE_TRY(Eve_Read(EVE_REG_ID, &value, 1U));
    *id = (uint8_t)value;
    g_bsp_eve.reg_id = value;
    return BSP_EVE_OK;
}

/* 읽기 transaction 하나(주소+dummy+4byte, HAL timeout25ms)로 프레임 수를 얻는다.
 * 값이 증가한다는 판단은 시간 간격을 아는 호출자가 수행하며 여기서는 raw 값을 보존한다. */
BSP_EVE_Status BSP_EVE_ReadFrames(uint32_t *frames)
{
    uint32_t value;
    if (frames == NULL) { return Eve_Fail(BSP_EVE_ERROR_ARGUMENT); }
    if (g_bsp_eve.ready == 0U) { return Eve_Fail(BSP_EVE_ERROR_NOT_READY); }
    EVE_TRY(Eve_Read(EVE_REG_FRAMES, &value, 4U));
    *frames = value;
    g_bsp_eve.frames = value;
    return BSP_EVE_OK;
}

/* 현재 EVE 레지스터를 읽어 RAM 진단에 저장한다. FRAMES는 EVE scanout 증거로
 * 반복 조회 시 증가하는지 확인할 수 있고, 화면 픽셀의 육안 확인과는 별개다. */
BSP_EVE_Status BSP_EVE_RefreshDiagnostics(void)
{
    uint32_t value;
    if (g_bsp_eve.ready == 0U) { return Eve_Fail(BSP_EVE_ERROR_NOT_READY); }
    EVE_TRY(Eve_Read(EVE_REG_ID, &value, 1U)); g_bsp_eve.reg_id = value;
    if (value != 0x7CU) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    EVE_TRY(Eve_Read(EVE_REG_CPURESET, &value, 1U)); g_bsp_eve.cpu_reset = value;
    if (value != 0U) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    EVE_TRY(Eve_Read(EVE_REG_FRAMES, &value, 4U)); g_bsp_eve.frames = value;
    EVE_TRY(Eve_Read(EVE_REG_FREQUENCY, &value, 4U)); g_bsp_eve.frequency_hz = value;
    EVE_TRY(Eve_Read(EVE_REG_DLSWAP, &value, 1U)); g_bsp_eve.dl_swap = value;
    EVE_TRY(Eve_Read(EVE_REG_PCLK, &value, 1U)); g_bsp_eve.pclk = value;
    if (value != 3U) { return Eve_Fail(BSP_EVE_ERROR_VERIFY); }
    g_bsp_eve.result = BSP_EVE_OK;
    return BSP_EVE_OK;
}

/* 진단 포인터 반환만 수행하므로 debugger나 읽기 전용 진단 코드가 장치를 건드리지
 * 않고 상태를 표시할 수 있다. 태스크 동시 갱신 중에는 여러 필드의 원자 snapshot이 아니다. */
const volatile BSP_EVE_Diagnostics *BSP_EVE_GetDiagnostics(void)
{
    return &g_bsp_eve;
}

/* FT81x SLEEP0x42 preserves RAM; POWERDOWN/PD_N would destroy cache identity.
 * Refuse a pending coprocessor command or display-list swap before stopping PCLK. */
uint32_t BSP_EVE_Sleep(void)
{
    uint32_t read=0,write=0,swap=0;
    if(Eve_Read(0x3020F8U,&read,2)||Eve_Read(0x3020FCU,&write,2)||Eve_Read(EVE_REG_DLSWAP,&swap,1))return 2;
    if(read!=write||swap)return 1;
    if(Eve_Write(EVE_REG_PCLK,0,1)||Eve_HostCommand(0x42U))return 3;
    return 0;
}
uint32_t BSP_EVE_WakeBegin(void){return Eve_HostCommand(0x00U);}
uint32_t BSP_EVE_WakeFinish(void)
{
    uint8_t id=0;if(BSP_EVE_ReadId(&id)||id!=0x7CU)return 1;
    return Eve_Write(EVE_REG_PCLK,g_bsp_eve.pclk,1);
}
