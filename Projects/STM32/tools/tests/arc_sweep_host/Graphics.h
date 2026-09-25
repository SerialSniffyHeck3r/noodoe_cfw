#ifndef ARC_SWEEP_TEST_GRAPHICS_H
#define ARC_SWEEP_TEST_GRAPHICS_H
#include "lvgl.h"
#include "Graphics_Viewport.h"
/* 측정 포트 입력만 대체한다. 고정 영역 상수와 기하 판정은 production 파일을
 * 직접 include/링크하므로 UI와 다른 가짜 원을 통과시키는 시험이 아니다. */
typedef struct {
    uint32_t magic,version,valid,fps_tenths,cpu_tenths,target_fps_milli;
    uint32_t continuous,window_ms,window_frames,frame_slots_missed;
    uint32_t cpu_window_cycles,idle_window_cycles;
} Graphics_Performance;
const volatile Graphics_Performance *Graphics_GetPerformance(void);
uint32_t Graphics_IsPointVisible(int32_t,int32_t);
uint32_t Graphics_IsAreaVisible(const lv_area_t *);
uint32_t Graphics_AreaIntersectsVisible(const lv_area_t *);
uint32_t Graphics_IsCircleVisible(int32_t,int32_t,int32_t);
uint32_t Graphics_GetSafeArea(int32_t,int32_t,lv_area_t *);
#endif
