#include "BSP_Display.h"
#include "bsp_eve.h"
#include "bsp_lcd_panel.h"
#include "bsp_backlight.h"
#include <stddef.h>

/*
 * FT81x Programmer Guide §4.21/4.32/4.40/4.41/4.47의 DL encoding만 직접 구현한다.
 * https://brtchip.com/wp-content/uploads/Support/Documentation/Programming_Guides/ICs/EVE/FT81X_Series_Programmer_Guide.pdf
 * RECTS의 LINE_WIDTH는 중심선으로부터의 반경이어서 폭/높이의 pixel 계약에 적합하지 않다.
 * 직사각형은 SCISSOR로 영역을 잡아 CLEAR color만 실행하고 fullscreen scissor로 복원한다.
 * 프레임워크/폰트/외부 코드/command FIFO를 가져오지 않는다.
 */
#define DISPLAY_MAGIC       0x44535031UL
#define DISPLAY_ERROR_STAGE 0x80000000UL
#define DL_DISPLAY          0x00000000UL
#define DL_CLEAR_COLOR_RGB  0x02000000UL
#define DL_COLOR_RGB        0x04000000UL
#define DL_LINE_WIDTH       0x0E000000UL
#define DL_COLOR_A          0x10000000UL
#define DL_COLOR_MASK       0x20000000UL
#define DL_SCISSOR_XY       0x1B000000UL
#define DL_SCISSOR_SIZE     0x1C000000UL
#define DL_BEGIN_LINES      0x1F000003UL
#define DL_END              0x21000000UL
#define DL_CLEAR_COLOR_ONLY 0x26000004UL
#define DL_CLEAR_ALL        0x26000007UL
#define DL_VERTEX_FORMAT_4  0x27000004UL

/*
 * .bss의 1KiB 고정 buffer다. Size는 EVE RAM_DL 8KiB보다 작게 제한한다.
 * 각 도형은 필요한 용량을 먼저 예약 검사하므로 중간 명령만 남기는 실패가 없다.
 * 마지막 DISPLAY를 위한 1word는 모든 일반 예약에서 항상 남겨 둔다.
 */
static uint32_t display_words[BSP_DISPLAY_DL_CAPACITY];
static uint32_t display_word_count;
volatile BSP_Display_Diagnostics g_bsp_display;

/* 마지막 API 결과와 누적 실패 횟수만 기록한다. 성공해도 frame_error를 지우지 않는다. */
static BSP_Display_Status DisplayResult(BSP_Display_Status result, uint32_t backend)
{
    g_bsp_display.last_result = (uint32_t)result;
    g_bsp_display.backend_result = backend;
    if (result != BSP_DISPLAY_OK) {
        g_bsp_display.failures++;
    }
    return result;
}

/* 현재 CPU context를 읽기만 한다. ISR/마스킹 영역에서 HAL timeout과 공유 buffer를 쓰지 않는다. */
static uint32_t DisplayContextIsValid(void)
{
    return ((__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U));
}

/*
 * 도형 작성 오류는 현재 frame을 독성 상태로 표시한다. 호출자가 반환값을 놓쳐도
 * 나중 Present에서 절반짜리 화면을 출력하지 않는다. 다음 BeginFrame만 이 latch를 푼다.
 */
static BSP_Display_Status DisplayFrameError(BSP_Display_Status result)
{
    if ((g_bsp_display.frame_open != 0U) && (g_bsp_display.frame_error == 0U)) {
        g_bsp_display.frame_error = (uint32_t)result;
    }
    return DisplayResult(result, 0U);
}

/* 통신/장치 실패에서는 광원/패널을 차단한다. 실패한 init 단계와 backend 원인은 보존한다. */
static BSP_Display_Status DisplayHardwareError(BSP_Display_Status result, uint32_t backend)
{
    const uint32_t stage = g_bsp_display.stage;
    BSP_Display_Shutdown();
    g_bsp_display.failed_stage = stage;
    g_bsp_display.stage = DISPLAY_ERROR_STAGE;
    return DisplayResult(result, backend);
}

/* 도형을 추가하기 전에 준비 상태와 열린 frame, 이전 오류, 정적 용량을 함께 검사한다. */
static BSP_Display_Status DisplayReserve(uint32_t count)
{
    if (DisplayContextIsValid() == 0U) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_CONTEXT);
    }
    if ((g_bsp_display.initialized == 0U) || (g_bsp_display.frame_open == 0U)) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_STATE);
    }
    if (g_bsp_display.frame_error != 0U) {
        return DisplayResult((BSP_Display_Status)g_bsp_display.frame_error, 0U);
    }
    if (count > (BSP_DISPLAY_DL_CAPACITY - 1U - display_word_count)) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_BUFFER_FULL);
    }
    return BSP_DISPLAY_OK;
}

/* Reserve 완료 후에만 호출한다. 이 작은 단위에서는 다시 범위 검사/오류 분기를 반복하지 않는다. */
static void DisplayAppend(uint32_t word)
{
    display_words[display_word_count++] = word;
    g_bsp_display.word_count = display_word_count;
}

/* RGB 상위 byte 및 사각형 끝 좌표를 검사한다. 덧셈을 32bit에서 수행하여 uint16 overflow를 피한다. */
static uint32_t DisplayRectIsValid(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t rgb)
{
    return ((rgb <= 0x00FFFFFFUL) && (w != 0U) && (h != 0U) &&
            ((uint32_t)x + w <= BSP_DISPLAY_WIDTH) &&
            ((uint32_t)y + h <= BSP_DISPLAY_HEIGHT));
}

/* 6word의 정확한 사각형 채우기다. 끝 좌표를 vertex로 만들지 않아 한 pixel 경계 오차가 없다. */
static void DisplayAppendFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t rgb)
{
    DisplayAppend(DL_SCISSOR_XY | ((uint32_t)x << 11U) | y);
    DisplayAppend(DL_SCISSOR_SIZE | ((uint32_t)w << 12U) | h);
    DisplayAppend(DL_CLEAR_COLOR_RGB | rgb);
    DisplayAppend(DL_CLEAR_COLOR_ONLY);
    DisplayAppend(DL_SCISSOR_XY);
    DisplayAppend(DL_SCISSOR_SIZE | (BSP_DISPLAY_WIDTH << 12U) | BSP_DISPLAY_HEIGHT);
}

/* pixel 좌표는 그 pixel의 중심(+0.5)으로 옮긴 뒤 VERTEX2F의 1/16 단위로 인코딩한다. */
static uint32_t DisplayVertex(uint16_t x, uint16_t y)
{
    return 0x40000000UL | (((uint32_t)x * 16U + 8U) << 15U) |
           ((uint32_t)y * 16U + 8U);
}

BSP_Display_Status BSP_Display_Init(void)
{
    HAL_StatusTypeDef hal;
    BSP_EVE_Status eve;
    if (DisplayContextIsValid() == 0U) {
        return DisplayResult(BSP_DISPLAY_ERROR_CONTEXT, 0U);
    }
    BSP_Display_CaptureInvalidate();
    g_bsp_display = (BSP_Display_Diagnostics){0};
    g_bsp_display.magic = DISPLAY_MAGIC;
    g_bsp_display.version = 1U;
    g_bsp_display.width = BSP_DISPLAY_WIDTH;
    g_bsp_display.height = BSP_DISPLAY_HEIGHT;
    display_word_count = 0U;

    /* 기존 실제 시험 순서를 facade 안으로만 이동한다. lowlevel 설정/명령은 그대로 사용한다. */
    g_bsp_display.stage = 1U;
    hal = BSP_BacklightInit();
    if (hal != HAL_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_BACKLIGHT, (uint32_t)hal);
    }
    g_bsp_display.stage = 2U;
    hal = BSP_LCD_PanelPrepare();
    if (hal != HAL_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_PANEL, (uint32_t)hal);
    }
    g_bsp_display.stage = 3U;
    eve = BSP_EVE_Init();
    if (eve != BSP_EVE_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_EVE, (uint32_t)eve);
    }
    g_bsp_display.stage = 4U;
    hal = BSP_LCD_PanelInit();
    if (hal != HAL_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_PANEL, (uint32_t)hal);
    }
    g_bsp_display.initialized = 1U;
    g_bsp_display.stage = 5U;
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_BeginFrame(uint32_t clear_rgb)
{
    if (DisplayContextIsValid() == 0U) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_CONTEXT);
    }
    if (g_bsp_display.initialized == 0U) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_STATE);
    }
    if (clear_rgb > 0x00FFFFFFUL) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_ARGUMENT);
    }
    /* 작성 중이던 frame을 명시적으로 버리고 오류 latch를 초기화한다. 장치 쓰기는 아직 없다. */
    display_word_count = 0U;
    g_bsp_display.word_count = 0U;
    g_bsp_display.frame_error = 0U;
    g_bsp_display.frame_open = 1U;
    DisplayAppend(DL_COLOR_MASK | 0x0FU);
    DisplayAppend(DL_SCISSOR_XY);
    DisplayAppend(DL_SCISSOR_SIZE | (BSP_DISPLAY_WIDTH << 12U) | BSP_DISPLAY_HEIGHT);
    DisplayAppend(DL_CLEAR_COLOR_RGB | clear_rgb);
    DisplayAppend(DL_CLEAR_ALL);
    DisplayAppend(DL_COLOR_A | 0xFFU);
    DisplayAppend(DL_VERTEX_FORMAT_4);
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t rgb)
{
    BSP_Display_Status result;
    if (DisplayRectIsValid(x, y, w, h, rgb) == 0U) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_ARGUMENT);
    }
    result = DisplayReserve(6U);
    if (result != BSP_DISPLAY_OK) {
        return result;
    }
    DisplayAppendFill(x, y, w, h, rgb);
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      uint16_t thickness, uint32_t rgb)
{
    BSP_Display_Status result;
    if ((DisplayRectIsValid(x, y, w, h, rgb) == 0U) || (thickness == 0U)) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_ARGUMENT);
    }
    if (((uint32_t)thickness * 2U >= w) || ((uint32_t)thickness * 2U >= h)) {
        return BSP_Display_FillRect(x, y, w, h, rgb);
    }
    result = DisplayReserve(24U);
    if (result != BSP_DISPLAY_OK) {
        return result;
    }
    /* 네 변을 안쪽의 네 사각형으로 추가한다. 모서리를 중복하지 않고 바깥 폭/높이를 유지한다. */
    DisplayAppendFill(x, y, w, thickness, rgb);
    DisplayAppendFill(x, (uint16_t)(y + h - thickness), w, thickness, rgb);
    DisplayAppendFill(x, (uint16_t)(y + thickness), thickness, (uint16_t)(h - 2U * thickness), rgb);
    DisplayAppendFill((uint16_t)(x + w - thickness), (uint16_t)(y + thickness),
                      thickness, (uint16_t)(h - 2U * thickness), rgb);
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                      uint16_t width, uint32_t rgb)
{
    BSP_Display_Status result;
    if ((x0 >= BSP_DISPLAY_WIDTH) || (x1 >= BSP_DISPLAY_WIDTH) ||
        (y0 >= BSP_DISPLAY_HEIGHT) || (y1 >= BSP_DISPLAY_HEIGHT) ||
        (width == 0U) || (width > BSP_DISPLAY_WIDTH) || (rgb > 0x00FFFFFFUL)) {
        return DisplayFrameError(BSP_DISPLAY_ERROR_ARGUMENT);
    }
    result = DisplayReserve(6U);
    if (result != BSP_DISPLAY_OK) {
        return result;
    }
    DisplayAppend(DL_COLOR_RGB | rgb);
    DisplayAppend(DL_LINE_WIDTH | ((uint32_t)width * 8U));
    DisplayAppend(DL_BEGIN_LINES);
    DisplayAppend(DisplayVertex(x0, y0));
    DisplayAppend(DisplayVertex(x1, y1));
    DisplayAppend(DL_END);
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_Present(void)
{
    BSP_Display_Status result = DisplayReserve(0U);
    BSP_EVE_Status eve;
    if (result != BSP_DISPLAY_OK) {
        g_bsp_display.frame_open = 0U;
        return result;
    }
    /* Reserve는 종단 1word를 항상 남겨 두었다. 닫힌 frame은 성공 후 다시 제출하지 못한다. */
    DisplayAppend(DL_DISPLAY);
    g_bsp_display.frame_open = 0U;
    eve = BSP_EVE_SubmitDisplayList(display_words, display_word_count);
    if (eve != BSP_EVE_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_EVE, (uint32_t)eve);
    }
    g_bsp_display.frames_presented++;
    g_bsp_display.last_frames = BSP_EVE_GetDiagnostics()->frames;
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_DrawTestWindow(void)
{
    BSP_Display_Status result;
    /* 작은 편의 함수도 공개 API의 좌표/용량 검사를 통과한다. 첫 오류 뒤에는 진행하지 않는다. */
#define DISPLAY_TEST_TRY(call) do { result = (call); if (result != BSP_DISPLAY_OK) { return result; } } while (0)
    DISPLAY_TEST_TRY(BSP_Display_BeginFrame(0x102030UL));
    DISPLAY_TEST_TRY(BSP_Display_FillRect(60U, 100U, 360U, 280U, 0x1E3650UL));
    DISPLAY_TEST_TRY(BSP_Display_FillRect(60U, 100U, 360U, 45U, 0x28A9D5UL));
    DISPLAY_TEST_TRY(BSP_Display_DrawRect(60U, 100U, 360U, 280U, 2U, 0xF0F4F8UL));
    DISPLAY_TEST_TRY(BSP_Display_DrawLine(90U, 180U, 390U, 350U, 2U, 0xFFCC40UL));
    DISPLAY_TEST_TRY(BSP_Display_DrawLine(390U, 180U, 90U, 350U, 2U, 0xFFCC40UL));
    DISPLAY_TEST_TRY(BSP_Display_DrawLine(90U, 265U, 390U, 265U, 2U, 0x4CE6A6UL));
#undef DISPLAY_TEST_TRY
    return BSP_Display_Present();
}

BSP_Display_Status BSP_Display_SetBrightnessPercent(uint32_t percent)
{
    HAL_StatusTypeDef result;
    if (DisplayContextIsValid() == 0U) {
        return DisplayResult(BSP_DISPLAY_ERROR_CONTEXT, 0U);
    }
    if (g_bsp_display.initialized == 0U) {
        return DisplayResult(BSP_DISPLAY_ERROR_STATE, 0U);
    }
    if (percent > 100U) {
        return DisplayResult(BSP_DISPLAY_ERROR_ARGUMENT, 0U);
    }
    result = BSP_BacklightSetPercent(percent);
    if (result != HAL_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_BACKLIGHT, (uint32_t)result);
    }
    g_bsp_display.requested_brightness = percent;
    g_bsp_display.actual_brightness = (percent == 100U) ? 99U : percent;
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

BSP_Display_Status BSP_Display_ReadFrames(uint32_t *frames)
{
    uint32_t value;
    BSP_EVE_Status result;
    if (frames == NULL) {
        return DisplayResult(BSP_DISPLAY_ERROR_ARGUMENT, 0U);
    }
    if (DisplayContextIsValid() == 0U) {
        return DisplayResult(BSP_DISPLAY_ERROR_CONTEXT, 0U);
    }
    if (g_bsp_display.initialized == 0U) {
        return DisplayResult(BSP_DISPLAY_ERROR_STATE, 0U);
    }
    result = BSP_EVE_ReadFrames(&value);
    if (result != BSP_EVE_OK) {
        return DisplayHardwareError(BSP_DISPLAY_ERROR_EVE, (uint32_t)result);
    }
    *frames = value;
    g_bsp_display.last_frames = value;
    return DisplayResult(BSP_DISPLAY_OK, 0U);
}

void BSP_Display_Shutdown(void)
{
    /* panel shutdown는 backlight shutdown도 수행하며 HAL tick에 의존하지 않는다. */
    BSP_LCD_PanelShutdown();
    display_word_count = 0U;
    g_bsp_display.word_count = 0U;
    g_bsp_display.frame_open = 0U;
    g_bsp_display.initialized = 0U;
    g_bsp_display.actual_brightness = 0U;
    g_bsp_display.requested_brightness = 0U;
    g_bsp_display.stage = 0U;
}

const volatile BSP_Display_Diagnostics *BSP_Display_GetDiagnostics(void)
{
    return &g_bsp_display;
}
