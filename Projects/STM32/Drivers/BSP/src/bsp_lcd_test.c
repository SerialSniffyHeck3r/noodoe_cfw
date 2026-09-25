#include "bsp_lcd_test.h"
#include "BSP_Display.h"
#include "BSP_Buttons.h"
#include "bsp_bringup.h"
#include "bsp_fault.h"
#include "BSP_Watchdog.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "Graphics.h"
#include "Graphics_Test.h"
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
#include "ProductUI.h"
#include "Product_Boot.h"
#include "Power_UI.h"
#include "PowerService.h"
#endif
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
#include "NoodoeRuntime.h"
#endif

/* 화면 정책만 시험에 둔다. 핀/SPI/패널/PWM 세부 구현은 public BSP 뒤에 있다. */
#define LCD_TEST_SERVICE_MS 5U
#define LCD_TEST_BLINK_MS 500U
#define LCD_TEST_SQUARE_PX 40U
volatile BSP_LCDTestStatus g_bsp_lcd_test;
volatile BSP_LCDButtonsStatus g_bsp_lcd_buttons;

/* 실행 단계/반환값/HAL ms 시각 기록. 첫 오류는 Fail에서 고정하며 재시도하지 않는다. */
static void LCDTestStage(uint32_t stage, uint32_t result)
{
  g_bsp_lcd_test.stage = stage;
  g_bsp_lcd_test.last_result = result;
  g_bsp_lcd_test.hal_tick = HAL_GetTick();
}

/* ms를 RTOS tick으로 올림 변환한다. 정상 서비스는 5ms만 양보하므로
 * 500ms 광원 주기가 입력 판정을 막지 않는다. 오류 루프도 100ms씩 양보한다.
 * 통합 프로필은 health supervisor가 watchdog을 소유한다. 단독 LCD 시험만
 * 완료된 서비스 반복을 WAIT phase에 보고한다. */
static void LCDTestWait(uint32_t milliseconds)
{
  while (milliseconds != 0U) {
    const uint32_t slice = milliseconds > 100U ? 100U : milliseconds;
    const uint32_t ticks = (slice * osKernelGetTickFreq() + 999U) / 1000U;
    if (osDelay(ticks != 0U ? ticks : 1U) != osOK) {
      g_bsp_lcd_test.wait_errors++;
      BSP_FaultRecord(0x401U);
    }
    BSP_BringupSample();
#if !NOODOE_INTEGRATED
    (void)BSP_Watchdog_Checkpoint(HAL_GetTick(),g_bsp_bringup.heartbeat);
#endif
    g_bsp_lcd_test.hal_tick = HAL_GetTick();
    milliseconds -= slice;
  }
}

/* 깨진 화면을 정상 동작으로 오인하지 않도록 표시를 정지하고 첫 오류를
 * 보존한다. 반환하지 않지만 RTOS/tick/진단은 살아 있어 SWD로 조사할 수 있다. */
static void LCDTestFail(uint32_t code, uint32_t result)
{
  BSP_Display_Shutdown();
  g_bsp_lcd_test.backlight_on = 0U;
  g_bsp_lcd_test.requested_percent = 0U;
  g_bsp_lcd_test.error = code;
  LCDTestStage(BSP_LCD_TEST_ERROR, result);
  for (;;) { LCDTestWait(100U); }
}

/* 그리기 단계마다 결과를 검사해 실패한 frame을 계속 제출하지 않는다. */
static void LCDTestDisplayResult(BSP_Display_Status result)
{
  if ((uint32_t)result != 0U) { LCDTestFail(4U, (uint32_t)result); }
}

/* 선 API만으로 UP/ENTER/DOWN 표식을 그린다. 폰트나 상위 그래픽 라이브러리
 * 없이 방향을 알아볼 수 있게 한다. 좌표와 굵기의 단위는 화면 pixel이다. */
static void LCDTestDrawSymbols(void)
{
  static const uint16_t lines[][4] = {
    {130,98,130,128}, {117,111,130,98}, {143,111,130,98},
    {350,98,350,128}, {337,115,350,128}, {363,115,350,128},
    {252,99,252,116}, {252,116,227,116}, {237,106,227,116}, {237,126,227,116}
  };
  for (uint32_t i = 0U; i < sizeof(lines) / sizeof(lines[0]); ++i) {
    LCDTestDisplayResult(BSP_Display_DrawLine(lines[i][0], lines[i][1],
        lines[i][2], lines[i][3], 3U, 0xE8EDF4U));
  }
}

/* 왼쪽부터 UP/ENTER/DOWN. 첫 행은 눌림, 아래 한 칸은 long, 그 아래는
 * very-long이다. 모든 활성 칸은 정확히 40x40이며 release하면 추가 칸은
 * 사라진다. 상태가 바뀔 때만 한 frame을 만들어 EVE에 제출한다. */
static void LCDTestDrawButtons(uint32_t pressed, uint32_t long_mask, uint32_t very_long)
{
  static const BSP_Buttons_Button columns[3] = {BSP_BUTTON_UP, BSP_BUTTON_ENTER, BSP_BUTTON_DOWN};
  static const uint32_t colors[3] = {0x43DB8CU, 0xFFD166U, 0x49B6FFU};
  LCDTestDisplayResult(BSP_Display_BeginFrame(0x101820U));
  LCDTestDisplayResult(BSP_Display_DrawRect(55U, 70U, 370U, 340U, 2U, 0x71859BU));
  LCDTestDisplayResult(BSP_Display_DrawLine(75U, 145U, 405U, 145U, 2U, 0x71859BU));
  LCDTestDrawSymbols();
  for (uint32_t column = 0U; column < 3U; ++column) {
    const uint16_t x = (uint16_t)(110U + column * 110U);
    const uint32_t bit = BSP_BUTTONS_MASK(columns[column]);
    LCDTestDisplayResult(BSP_Display_FillRect(x, 170U, LCD_TEST_SQUARE_PX,
        LCD_TEST_SQUARE_PX, (pressed & bit) != 0U ? colors[column] : 0x344250U));
    if ((long_mask & bit) != 0U) {
      LCDTestDisplayResult(BSP_Display_FillRect(x, 235U, LCD_TEST_SQUARE_PX, LCD_TEST_SQUARE_PX, colors[column]));
    }
    if ((very_long & bit) != 0U) {
      LCDTestDisplayResult(BSP_Display_FillRect(x, 300U, LCD_TEST_SQUARE_PX, LCD_TEST_SQUARE_PX, colors[column]));
    }
  }
  LCDTestDisplayResult(BSP_Display_Present());
  g_bsp_lcd_buttons.redraws++;
}

/* 같은 태스크에서 큐를 소비하는 상위 코드 예제다. 제품 코드는 switch에서
 * 원하는 동작을 호출하면 된다. RELEASE는 최종 시간, SHORT/LONG/VERY_LONG은
 * 각각 이벤트 횟수를 남기므로 화면뿐 아니라 SWD로도 처리 결과를 볼 수 있다. */
static void LCDTestConsumeButtonEvents(void)
{
  BSP_Buttons_Event event;
  while (BSP_Buttons_GetEvent(&event) != 0U) {
    const uint32_t button = (uint32_t)event.button;
    g_bsp_lcd_buttons.consumed_events++;
    g_bsp_lcd_buttons.last_button = button;
    g_bsp_lcd_buttons.last_event = (uint32_t)event.type;
    g_bsp_lcd_buttons.last_duration_ms = event.duration_ms;
    if (button >= 3U) { LCDTestFail(9U, button); }
    switch (event.type) {
      case BSP_BUTTON_EVENT_RELEASE:
        g_bsp_lcd_buttons.last_duration_ms_by_button[button] = event.duration_ms; break;
      case BSP_BUTTON_EVENT_SHORT_PRESS:
        g_bsp_lcd_buttons.short_count[button]++; break;
      case BSP_BUTTON_EVENT_LONG_PRESS:
        g_bsp_lcd_buttons.long_count[button]++; break;
      case BSP_BUTTON_EVENT_VERY_LONG_PRESS:
        g_bsp_lcd_buttons.very_long_count[button]++; break;
      default: break; /* PRESS는 GetState 기반 그림과 위 공통 진단에서 관측한다. */
    }
  }
}

/* 실제 scanout 진척을 읽는다. 오류 때 이전 값을 새 관측처럼 사용하지 않는다. */
static void LCDTestSampleFrames(void)
{
  uint32_t frames = 0U;
  const BSP_Display_Status result = BSP_Display_ReadFrames(&frames);
  if ((uint32_t)result != 0U) { LCDTestFail(7U, (uint32_t)result); }
  g_bsp_lcd_test.eve_frames = frames;
}

/* 프로젝트 끝까지 보존하는 시험 진입. main 태스크에는 LCDTest(); 한 줄만
 * 두고, 여기서는 public BSP로 초기화/입력/표시를 수행한다. 버튼은 5ms마다,
 * 그림은 상태 변화 때, 광원은 500ms마다 처리하며 동적 할당은 하지 않는다. */
void LCDTestLegacy(void)
{
  uint32_t last_blink;
  uint32_t old_visual = 0xFFFFFFFFU;
  g_bsp_lcd_test.magic = 0x4C434454U;
  g_bsp_lcd_test.version = 1U;
  g_bsp_lcd_test.core_clock = SystemCoreClock;
  g_bsp_lcd_test.init_attempts = 1U;
  g_bsp_lcd_buttons.magic = 0x4C425431U; /* LBT1 */
  g_bsp_lcd_buttons.version = 1U;
  BSP_BringupMark(6U);
  BSP_BringupSample();
  LCDTestStage(BSP_LCD_TEST_START, 0U);
  /* facade가 광원 OFF, panel prepare, EVE, panel init 순서를 소유한다. */
  const BSP_Display_Status result = BSP_Display_Init();
  if ((uint32_t)result != 0U) { LCDTestFail(1U, (uint32_t)result); }
  BSP_Buttons_Init();
  LCDTestDisplayResult(BSP_Display_SetBrightnessPercent(25U));
  g_bsp_lcd_test.backlight_on = 1U;
  g_bsp_lcd_test.requested_percent = 25U;
  g_bsp_lcd_test.transitions = 1U;
  last_blink = HAL_GetTick();
  LCDTestSampleFrames();
  LCDTestStage(BSP_LCD_TEST_BLINK, 0U);
  for (;;) {
    BSP_Buttons_Process();
    LCDTestConsumeButtonEvents();
    uint32_t pressed = 0U, long_mask = 0U, very_long = 0U;
    for (uint32_t button = 0U; button < (uint32_t)BSP_BUTTON_COUNT; ++button) {
      BSP_Buttons_State state;
      if (BSP_Buttons_GetState((BSP_Buttons_Button)button, &state) == 0U) { LCDTestFail(10U, button); }
      if (state.pressed != 0U) {
        const uint32_t bit = BSP_BUTTONS_MASK(button);
        pressed |= bit;
        if (state.long_event_sent != 0U) { long_mask |= bit; }
        if (state.very_long_event_sent != 0U) { very_long |= bit; }
      }
    }
    g_bsp_lcd_buttons.service_count++;
    g_bsp_lcd_buttons.pressed_mask = pressed;
    g_bsp_lcd_buttons.long_mask = long_mask;
    g_bsp_lcd_buttons.very_long_mask = very_long;
    const uint32_t visual = pressed | (long_mask << 3U) | (very_long << 6U);
    if (visual != old_visual) {
      LCDTestDrawButtons(pressed, long_mask, very_long);
      old_visual = visual;
    }
    const uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_blink) >= LCD_TEST_BLINK_MS) {
      const uint32_t on = g_bsp_lcd_test.backlight_on == 0U ? 1U : 0U;
      LCDTestDisplayResult(BSP_Display_SetBrightnessPercent(on != 0U ? 25U : 0U));
      g_bsp_lcd_test.backlight_on = on;
      g_bsp_lcd_test.requested_percent = on != 0U ? 25U : 0U;
      g_bsp_lcd_test.transitions++;
      last_blink = now;
      LCDTestSampleFrames();
    }
    LCDTestWait(LCD_TEST_SERVICE_MS);
  }
}

/* 보드 BSP 이벤트를 GraphicsTest의 입력 enum으로 명시적으로 변환한다.
 * UI와 하드웨어 header가 서로 include하지 않아도 연결을 바꿀 수 있다. */
#if !NOODOE_PRODUCT
static void LCDTestGraphicsButton(uint32_t button, uint32_t event, uint32_t duration, void *context)
{
  (void)context;
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
  /* Product subscribes directly to the middleware event stream. */
  (void)button;(void)event;(void)duration;
#else
  if (button < BSP_BUTTON_COUNT && event >= BSP_BUTTON_EVENT_PRESS && event <= BSP_BUTTON_EVENT_VERY_LONG_PRESS) {
    GraphicsTest_ObserveButton((GraphicsTest_Button)button, (GraphicsTest_ButtonEvent)event, duration);
  }
#endif
}
#endif

/* 상위 그래픽 기능 순회 시험도 기존 LCDTest 진입 안에서만 구동한다.
 * main RTOS task는 여전히 LCDTest(); 한 줄이고 Cube 생성 파일을 변경하지 않는다.
 * 이전 bring-up을 다시 실행하려면 프로젝트 define LCD_TEST_USE_LEGACY=1로 빌드한다. */
void LCDTest(void)
{
#if !NOODOE_PRODUCT && !NOODOE_INTEGRATED
  (void)BSP_Watchdog_StartEarly();
  (void)BSP_Watchdog_Init(HAL_GetTick(),WATCHDOG_WAIT,0);
#endif
#if defined(LCD_TEST_USE_LEGACY) && LCD_TEST_USE_LEGACY
  LCDTestLegacy();
#elif NOODOE_PRODUCT
  ProductBoot_Run();
#else
  g_bsp_lcd_test.magic = 0x4C434454U;
  g_bsp_lcd_test.version = 1U;
  g_bsp_lcd_test.core_clock = SystemCoreClock;
  g_bsp_lcd_test.init_attempts = 1U;
  BSP_BringupMark(6U);
  BSP_BringupSample();
  LCDTestStage(BSP_LCD_TEST_START, 0U);
  Graphics_Status status = Graphics_Init();
  if (status != GRAPHICS_OK) { LCDTestFail(11U, (uint32_t)status); }
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
  if (!ProductUI_Init(HAL_GetTick(),g_graphics.input_boot_held_mask)) { LCDTestFail(12U, 2U); }
#else
  if (!GraphicsTest_Init()) { LCDTestFail(12U, 1U); }
  /* 이번 실물 동작 시험은 호의 채움/감쇠를 계속 관측한다. DOWN으로 기존
   *41개 기능 순회를 재개할 수 있고 main의LCDTest 한 줄 계약은 유지한다. */
  GraphicsTest_SetCasePinned(1U);
  GraphicsTest_SetBootHeldMask(g_graphics.input_boot_held_mask);
#endif
  Graphics_SetButtonCallback(LCDTestGraphicsButton, NULL);
  g_bsp_lcd_test.backlight_on = 1U;
  g_bsp_lcd_test.requested_percent = GRAPHICS_DEFAULT_BRIGHTNESS;
  g_bsp_lcd_test.transitions = 1U;
  LCDTestStage(BSP_LCD_TEST_BLINK, 0U); /* ABI 단계는 보존; 새 시험에서는 고정광원이다. */
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
  /* I/O/storage errors are reported independently; the healthy display remains
   * available while individual peripherals are being diagnosed. */
  (void)NoodoeRuntime_Start();
#endif
  for (;;) {
    status = GRAPHICS_OK;
    /* UI 객체 교체 중에도 SWD가 완성된 표본으로 오인하지 않게 같은 sequence
     * 경계를 사용한다. 이것은 lock이 아니며 소유 task 하나의 진단 표식이다. */
    ++g_graphics.sample_seq;
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
    ProductUI_Process(HAL_GetTick());
    if(PowerUI_GraphicsDue())status=Graphics_Process();
    else if(g_power_ui.state==IGN_OFF_AWAKE)Graphics_ServiceCapture();
    PowerUI_AfterGraphics(HAL_GetTick());
#else
    status=Graphics_Process();
    GraphicsTest_NotifyRendered(g_graphics.render_count);
    GraphicsTest_Process();
#endif
    g_bsp_lcd_test.eve_frames = g_graphics.eve_frames;
    g_bsp_lcd_test.requested_percent = Graphics_GetBrightnessPercent();
    ++g_graphics.sample_seq;
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
    NoodoeRuntime_GraphicsHeartbeat();
#endif
    if(status!=GRAPHICS_OK)LCDTestFail(13U,(uint32_t)status);
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
    BSP_BringupSample();
    PowerService_Wait(POWER_OWNER_GRAPHICS,PowerUI_WaitMs());
#else
    LCDTestWait(LCD_TEST_SERVICE_MS);
#endif
  }
#endif
}
