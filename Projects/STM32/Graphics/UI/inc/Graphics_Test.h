#ifndef GRAPHICS_TEST_H
#define GRAPHICS_TEST_H

#include <stdint.h>

/* 화면 자동 전환 주기(ms). UI 모듈은 task/HAL/BSP를 직접 호출하지 않고
 * Graphics 포트와 LVGL tick만 사용한다. 같은 LVGL 소유 task에서 API를 호출한다. */
#ifndef GRAPHICS_TEST_CASE_MS
#define GRAPHICS_TEST_CASE_MS 8000U
#endif

/* 합성 값의 표본 갱신 간격이다.33ms는약30fps 목표이며 실제 GPU 처리량 보장이
 * 아니다. 동작 속도는 elapsed time에서 계산하므로 주기를 바꿔도 빨라지지 않는다. */
#ifndef GRAPHICS_TEST_UPDATE_MS
#define GRAPHICS_TEST_UPDATE_MS 33U
#endif
#if GRAPHICS_TEST_UPDATE_MS < 1
#error "GRAPHICS_TEST_UPDATE_MS must be at least 1 millisecond"
#endif

#define GRAPHICS_TEST_DIAGNOSTIC_MAGIC 0x47545354UL /* GTST */
#define GRAPHICS_TEST_DIAGNOSTIC_VERSION 1U

typedef enum {
    GRAPHICS_TEST_NOT_VISITED = 0,
    GRAPHICS_TEST_RUNNING = 1,
    GRAPHICS_TEST_RENDERED = 2,
    GRAPHICS_TEST_SKIPPED = 3,
    GRAPHICS_TEST_ERROR = 4
} GraphicsTest_State;

/* 지원 예상과 실행 관측을 분리한다. BASIC은 단순 스타일 경로만 시험한다는 뜻이며
 * RENDERED는 포트가 frame 제출을 알렸다는 뜻이다. 두 값 모두 육안 PASS가 아니다. */
typedef enum {
    GRAPHICS_TEST_SUPPORT_BASIC = 0,
    GRAPHICS_TEST_SUPPORT_LIMITED = 1,
    GRAPHICS_TEST_SUPPORT_UNSUPPORTED = 2,
    GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED = 3
} GraphicsTest_Support;

typedef enum {
    GRAPHICS_TEST_CAP_RECT = 1U << 0,
    GRAPHICS_TEST_CAP_TEXT = 1U << 1,
    GRAPHICS_TEST_CAP_LINE = 1U << 2,
    GRAPHICS_TEST_CAP_ARC = 1U << 3,
    GRAPHICS_TEST_CAP_IMAGE = 1U << 4,
    GRAPHICS_TEST_CAP_CLIP = 1U << 5,
    GRAPHICS_TEST_CAP_LAYER = 1U << 6,
    GRAPHICS_TEST_CAP_SHADOW = 1U << 7,
    GRAPHICS_TEST_CAP_GRADIENT = 1U << 8,
    GRAPHICS_TEST_CAP_MASK = 1U << 9,
    GRAPHICS_TEST_CAP_VECTOR = 1U << 10
} GraphicsTest_Capability;

/* BSP enum과 우연한 숫자 일치에 의존하지 않도록 이 UI의 입력 계약을 공개한다.
 * root는 BSP 이벤트를 이 enum으로 변환하여 넘긴다. DOWN/ENTER 순서를 주의한다. */
typedef enum {
    GRAPHICS_TEST_BUTTON_UP = 0,
    GRAPHICS_TEST_BUTTON_DOWN = 1,
    GRAPHICS_TEST_BUTTON_ENTER = 2,
    GRAPHICS_TEST_BUTTON_COUNT = 3
} GraphicsTest_Button;

typedef enum {
    GRAPHICS_TEST_BUTTON_PRESS = 1,
    GRAPHICS_TEST_BUTTON_RELEASE = 2,
    GRAPHICS_TEST_BUTTON_SHORT = 3,
    GRAPHICS_TEST_BUTTON_LONG = 4,
    GRAPHICS_TEST_BUTTON_VERY_LONG = 5
} GraphicsTest_ButtonEvent;

/* id는 0부터 시작하는 안정적인 배열 인덱스다. 화면 번호는 id+1로 표시한다.
 * title/features/note는 프로그램 수명 동안 유효한 정적 문자열이다. */
typedef struct {
    uint32_t id;
    const char *title;
    const char *features;
    uint32_t capabilities;
    GraphicsTest_Support support;
    const char *note;
} GraphicsTest_CaseInfo;

/* 방문 기록은 case 객체 삭제 후에도 남는다. frame 수는 포트의 실제 제출 통지를
 * 받은 수이며, 검증 PASS/화면 판독 결과를 만들지 않는다. */
typedef struct {
    uint32_t id;
    uint32_t state;
    uint32_t visits;
    uint32_t rendered_visits;
    uint32_t frames;
    uint32_t ui_events;
    uint32_t allocation_failures;
} GraphicsTest_Result;

/* SWD 수집에 맞춰 진단 필드는 모두 uint32_t로 유지한다. UI 변경 중 읽으면
 * 필드 전체가 같은 시각의 원자적 스냅샷이라는 보장은 없다. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t current_case;
    uint32_t case_count;
    uint32_t state;
    uint32_t auto_advance;
    uint32_t case_started_ms;
    uint32_t process_count;
    uint32_t transition_count;
    uint32_t rendered_frames;
    uint32_t last_frame_sequence;
    uint32_t button_events;
    uint32_t blocked_button_commands;
    uint32_t boot_held_mask;
    uint32_t ui_events;
    uint32_t allocation_failures;
    uint32_t demo_speed;
} GraphicsTest_Diagnostics;

extern volatile GraphicsTest_Diagnostics g_graphics_test;
/* 방문한 모든 case의 누계를 SWD에서 읽기 위한 심볼. 개수는 GetCaseCount와
 * ELF 심볼 크기로 확인한다. 상위 코드는 GetResults로 읽고 직접 변경하지 않는다. */
extern GraphicsTest_Result g_graphics_test_results[];

/* Graphics_Init 성공 후 한 번 호출한다. 성공1/실패0. 필요한 LVGL 화면 객체를
 * 만들고 첫 case를 시작한다. 이미 초기화됐다면 기존 화면을 정리한 뒤 재시작한다. */
uint32_t GraphicsTest_Init(void);

/* Graphics_Shutdown(lv_deinit)보다 먼저 호출한다. UI root/animation/공통 style의
 * LVGL 할당을 반환하고 정적 참조를 비운다. 이미 종료했다면 아무 작업도 하지 않는다.
 * 이후 Graphics_Init→GraphicsTest_Init 순서로 새 세션을 만들 수 있다. */
void GraphicsTest_Shutdown(void);

/* LVGL 소유 task에서 반복 호출한다. 애니메이션 값/표시/자동전환만 갱신하며
 * lv_timer_handler, task delay, 하드웨어 접근을 호출하지 않는다. */
void GraphicsTest_Process(void);

/* 수동 전환은 성공1/잘못된 번호·초기화 실패0. 이전 case의 객체와 UI 소유
 * animation을 정리한다. auto 모드의 8초 시작 시각도 현재로 다시 잡는다. */
uint32_t GraphicsTest_Next(void);
uint32_t GraphicsTest_Previous(void);
uint32_t GraphicsTest_Select(uint32_t index);
void GraphicsTest_SetAutoAdvance(uint32_t enabled);
/* 자동 값/애니메이션은 계속 실행하면서 현재 case의 시간 기반 전환만 고정한다.
 * DOWN/UP으로 다음 case를 고르면 고정은 해제되어 기존41개 순회를 재개한다. */
void GraphicsTest_SetCasePinned(uint32_t enabled);
uint32_t GraphicsTest_IsCasePinned(void);

uint32_t GraphicsTest_GetCaseCount(void);
const GraphicsTest_CaseInfo *GraphicsTest_GetCaseInfo(uint32_t index);
const GraphicsTest_Result *GraphicsTest_GetResults(uint32_t *count);
const volatile GraphicsTest_Diagnostics *GraphicsTest_GetDiagnostics(void);

/* Init 직후 최초 BSP raw LOW mask를 전달한다. boot-held 버튼은 한 번 놓고,
 * 그 뒤 새 PRESS가 있어야 UI 명령을 만든다. 자동 모드의 UP/DOWN SHORT만
 * 이전/다음 case로 이동한다. ENTER는 정상 PRESS 뒤 duration>=2001ms RELEASE에서
 * auto/manual을 전환한다. 수동 모드의 focus/edit/짧은 ENTER click은 Graphics
 * encoder 포트가 단독 처리한다. UP 고정 LOW는 모든 UI 명령에서 차단한다. */
void GraphicsTest_SetBootHeldMask(uint32_t mask);
void GraphicsTest_ObserveButton(GraphicsTest_Button button,
                                GraphicsTest_ButtonEvent event,
                                uint32_t duration_ms);

/* Graphics 포트가 성공적인 frame 제출 뒤 단조 frame sequence를 전달한다.
 * 같은 sequence의 중복은 무시한다. 호출이 없으면 case는 RUNNING에 머물러
 * 단지 객체 생성만으로 RENDERED를 주장하지 않는다. UI에서 이 함수를 스스로
 * 호출하여 성공을 꾸미지 않는다. */
void GraphicsTest_NotifyRendered(uint32_t frame_sequence);

#endif
