#ifndef GRAPHICS_ARC_SWEEP_H
#define GRAPHICS_ARC_SWEEP_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 이 시험은 실제 차량 속도를 받지 않는 독립 DEMO다. 숫자0..160과 별개로
 * arc에0..10000 값을 보내므로 정수 km/h 변경이 arc 해상도를 제한하지 않는다.
 * 현재 LVGL/EVE arc backend의 각도는 최종적으로 정수 degree로 양자화된다.
 * 30fps 제출은 Graphics 포트가 담당하며 이 모듈은 timer/task를 만들지 않는다. */
#define GRAPHICS_ARC_SWEEP_HALF_PERIOD_MS 4000U
#define GRAPHICS_ARC_SWEEP_PERIOD_MS 8000U
#define GRAPHICS_ARC_SWEEP_VALUE_MAX 10000U
#define GRAPHICS_ARC_SWEEP_SPEED_MAX 160U
#define GRAPHICS_ARC_SWEEP_DIAGNOSTIC_MAGIC 0x47415331UL /* GAS1 */

/* SWD와 상위 시험에서 읽을 수 있는 정수 진단. phase_ms는 현재8초 주기의
 * 위치이며 direction은0=채우기/1=비우기다. updates는 실제 arc 값 변경 횟수다.
 * error:0=정상,1=인자/부모 원점,2=표시 영역,3=객체 할당 실패.
 * rendered 여부는 Graphics의 실제 frame 진단에서 확인한다. 이 구조의
 * process_count/updates가 증가했다는 것만으로 GPU 제출 성공을 주장하지 않는다. */
typedef struct {
    uint32_t magic, version, initialized, error;
    uint32_t phase_ms, direction, value, speed;
    uint32_t cycles, process_count, updates, hud_updates;
} GraphicsArcSweep_Diagnostics;

extern volatile GraphicsArcSweep_Diagnostics g_graphics_arc_sweep;

/* Graphics/LVGL 소유 task에서만 호출한다. parent는 표시 좌표 원점(0,0)에
 * content가 시작되는480x480 이상 부모다. center/radius는 실제 pixel이며
 * radius128..239만 받는다. arc 전체 원과 모든 label 사각형을 현재 Graphics
 * 표시 영역 안에서 검사한다. 성공1, 실패0; 실패 시 만든 자식만 정리한다.
 * 재호출은 이전 시험 객체를 먼저 정리한다. 부모 자체는 삭제/변경하지 않는다.
 * 투명480x480 자식 하나를 소유하므로 기존 chrome은 호출자가 숨겨야 한다. */
uint32_t GraphicsArcSweep_Init(lv_obj_t *parent, int32_t center_x,
                               int32_t center_y, int32_t outer_radius);

/* 기본480px 원형 계기판용. (0,0)의480x480 arc, 외곽반경240, pad0으로
 * 테두리를 채운다. LVGL 정수 중심240과 활성 mask 중심239.5의 차이는 최종
 * 원형 clipping이 처리한다. parent/label/수명/실패 규칙은 Init과 같고,
 * 임의 크기의 완전 포함 원에는 기존 Init을 사용한다. UI task 전용이다. */
uint32_t GraphicsArcSweep_InitViewport(lv_obj_t *parent);

/* Init 이후 경과한 누적 ms를 전달한다. 호출 횟수/지연에 관계없이4초 채우기,
 * 4초 비우기가 반복된다. unsigned ms wrap도 이전 표본과의 차이로 처리한다.
 * 생성된 arc/숫자 label만 변경하고 FPS/CPU HUD는 최대1초에 한 번 갱신한다.
 * BSP/HAL/RTOS/실차 입력을 사용하지 않는다. 미초기화 상태에서는 아무 일도 안 한다. */
void GraphicsArcSweep_Process(uint32_t elapsed_ms);

/* Graphics_Shutdown 전에 소유 task에서 호출한다. 이 모듈의 자식만 즉시
 * 삭제하며 부모가 이미 자식을 삭제한 경우에도 안전하게 아무 일도 안 한다.
 * LV_EVENT_DELETE에서 참조를 비우므로 lv_obj_clean(parent)도 허용한다. */
void GraphicsArcSweep_Destroy(void);

/* 읽기 전용 수명 고정 진단 포인터. 같은 task에서 읽거나 SWD halt 표본으로
 * 사용한다. 여러 word가 다른 task에 대해 원자적이라는 보장은 하지 않는다. */
const volatile GraphicsArcSweep_Diagnostics *GraphicsArcSweep_GetDiagnostics(void);

#ifdef __cplusplus
}
#endif
#endif
