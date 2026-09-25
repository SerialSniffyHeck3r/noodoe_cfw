# 버튼 BSP와 LCD 시험

순정 V516의 GPIO descriptor, 이벤트 dispatcher, 공장 검사 패킷, APK 상수를 대조한 결과다. 원시 근거와 순정의 경계 조건은 [분석 보고서](../../../../analysis/2026-09-12-buttons-bringup/stock-buttons.md)에 있다.

| 실제 기능 | MCU 입력 | EXTI | 화면 위치 |
|---|---|---|---|
| 위 / UP | PD12 | EXTI15_10, priority 5 | 왼쪽, 위 화살표, 녹색 |
| 엔터 / ENTER | PA15 | EXTI15_10, priority 5 | 가운데, RETURN 표식, 노란색 |
| 아래 / DOWN | PI6 | EXTI9_5, priority 15 | 오른쪽, 아래 화살표, 파란색 |

셋 모두 LOW 눌림, 내부 pull 없음, 양에지 EXTI다. 커넥터 실물 핀 번호와 MCU 핀 번호는 다르다. PI3/PI4/PI5는 이 버튼이 아니다. BSP가 소유한 입력만 초기화하며 전원 GPIO와 다른 EXTI pending은 보존한다.

## 호출 계약

상위 태스크는 `BSP_Buttons.h`와 `BSP_Display.h`만으로 입력과 화면을 제어할 수 있다. 핀/패널/SPI/PWM의 자세한 구현은 해당 API 내부에 있다. 버튼과 디스플레이 API는 각각 한 태스크가 소유하며 여러 태스크의 동시 호출이나 ISR에서 그리기를 지원하지 않는다.

```c
#include "BSP_Buttons.h"

/* HAL timebase가 시작된 뒤 서비스 태스크에서 한 번 호출한다. */
BSP_Buttons_Init();

/* 같은 태스크의 반복 처리에서 약 5ms마다 호출한다. */
BSP_Buttons_Process();
BSP_Buttons_Event event;
while (BSP_Buttons_GetEvent(&event)) {
    switch (event.type) {
    case BSP_BUTTON_EVENT_SHORT_PRESS:
        /* event.button에 해당하는 짧은 누름 처리 */
        break;
    case BSP_BUTTON_EVENT_LONG_PRESS:
        /* 누르는 중 긴 누름 임계시간에 한 번 */
        break;
    case BSP_BUTTON_EVENT_VERY_LONG_PRESS:
        /* 누르는 중 매우 긴 누름 임계시간에 한 번 */
        break;
    case BSP_BUTTON_EVENT_RELEASE:
        /* event.duration_ms와 event.classification이 최종 결과 */
        break;
    default:
        break;
    }
}
uint32_t held = BSP_Buttons_GetCurrentDurationMs(BSP_BUTTON_ENTER);
uint32_t previous = BSP_Buttons_GetLastDurationMs(BSP_BUTTON_ENTER);
```

GPIO HAL 콜백은 main.c USER CODE의 `BSP_Buttons_IRQHandler(GPIO_Pin)`로 연결한다. ISR은 변화/원시 레벨 진단만 기록하고 태스크가 GPIO를 다시 읽어 상태와 시간을 판정한다. 관측 사이에 발생하고 끝난 짧은 펄스를 복원한다고 주장하지 않으며, 시간 정밀도는 실제 Process 호출 주기에 제한된다. 하드웨어 IRQ 연결과 상태 판정은 각각 검증한다.

## 시간과 이벤트

`inc/BSP_Buttons.h`의 다음 define을 바꾸면 된다. 모든 단위는 ms다.

| define | 기본값 | 의미 |
|---|---:|---|
| `BSP_BUTTONS_DEBOUNCE_MS` | 21 | 눌림/해제 안정화 |
| `BSP_BUTTONS_MIN_DECISION_MS` | 80 | short 판단의 최소 경과 기준, 초과 비교 |
| `BSP_BUTTONS_LONG_PRESS_MS` | 2001 | 순정의 2000ms 초과에 대응 |
| `BSP_BUTTONS_VERY_LONG_PRESS_MS` | 3000 | 이번 BSP가 추가한 매우 긴 누름 |
| `BSP_BUTTONS_EVENT_QUEUE_SIZE` | 32 | 고정 이벤트 큐 길이 |

- PRESS는 안정화 뒤 한 번 발생한다. 매우 짧은 안정 입력은 RELEASE 분류 NONE으로 끝날 수 있다.
- short는 해제 확정 시 한 번 발생한다. 최소 판단 기준은 hold + 설정된 해제 안정화 시간이므로 물리적으로 80ms를 반드시 누르라는 뜻이 아니다.
- LONG과 VERY_LONG은 누르는 중 각각 임계시간에 한 번 발생한다. VERY_LONG까지 누르면 LONG도 앞서 발생한다. 최종 RELEASE 분류는 하나이며 긴 누름 뒤 SHORT를 다시 내지 않는다.
- 해제 후보가 생기면 보고하는 hold 기간은 그 시각에서 멈춘다. 태스크 지연이나 해제 안정화 대기 때문에 길게 누름으로 승격하지 않는다.
- `GetState`, `IsPressed`, `GetPressedMask`, 현재/직전 기간, 직전 분류 getter를 제공한다. 큐가 가득 차면 새 이벤트를 버리고 overflow/sequence로 알린다. 상태 getter는 계속 갱신된다. 자동 반복 이벤트는 없다.

순정과 핀/극성/양에지 EXTI/주요 임계값은 맞췄다. 순정 입력 생성기의 완전한 복제는 아니다. 눌림 안정화, 실제 hold에 따른 long 분류, 태스크 지연에 독립적인 short 최소 판정, VERY_LONG과 상태 API는 확장/보완이다. 순정의 PH9 및 IGN gate와 IGN ON 시 ENTER+DOWN 조합 동작은 상위 전원/앱 정책이므로 버튼 BSP에 넣지 않았다. 순정도 세 버튼을 독립적으로 처리한다.

## 화면 사용 예

```c
#include "BSP_Display.h"
/* 각 반환값이 BSP_DISPLAY_OK인지 검사해야 한다. LCDTest가 오류 처리 예제다. */
BSP_Display_Init();
BSP_Display_BeginFrame(0x101820);
BSP_Display_FillRect(110, 170, 40, 40, 0x43DB8C);
BSP_Display_Present();
BSP_Display_SetBrightnessPercent(25);
```

Init/Shutdown, BeginFrame/Present, FillRect/DrawRect/DrawLine, 밝기, 해상도 상수, 프레임/진단 조회가 공개 API다. 256-word 고정 display-list 버퍼이며 MCU framebuffer나 그래픽 프레임워크를 사용하지 않는다. 좌표 범위/버퍼 초과 등의 오류가 있는 프레임은 제출하지 않는다. 새 BeginFrame으로 다시 작성할 수 있다. 패널/EVE/백라이트 하위 파일은 검증된 전송 구현으로 유지한다.

## 영구 보존 시험과 재생성

main.c의 실제 태스크 본문은 계속 `LCDTest();` 한 줄이다. `bsp_lcd_test.c`는 프로젝트 끝까지 보존한다. 첫 행의 40×40 칸은 해당 버튼을 누르면 켜지고, long에서 아래에 한 칸, very-long에서 그 아래에 한 칸이 나타난다. 해제하면 추가 칸은 사라진다. 기존 25%/OFF 500ms 깜빡임은 그대로다.

현재 IOC와 생성 IRQ에는 PD12/PA15/PI6의 양에지 EXTI/NOPULL이 이미 들어 있다. 이번 연결은 USER CODE 콜백과 독립 BSP만 추가하므로 별도 GUI 재생성이 필요하지 않다. `check_project.ps1`은 콜백과 IOC/생성 IRQ를 검사한다. 이 정적 검사는 실제 GUI 재생성 시험을 대신하지 않는다.

`tools/bringup.ps1 -LCDTest`는 버튼 GPIO 모드/EXTI routing/양에지 mask, 서비스 진척, LCD frame, PWM과 기존 BL 인계를 검사한다. 물리 버튼을 실제 눌러 MCU 입력·이벤트·화면이 일치하는지는 별도 관측이다. 화면 제출 중 DLSWAP=2인 순간은 정상 과도 상태이므로, 그 순간에 halt한 진단을 영구 실패로 해석하지 않는다.
