/* 통합 브링업에서 필요한 arc/label/EVE만 선택한다. 원래 lv_conf 설정과
 * 모든 시험 source는 Graphics profile(NOODOE_INTEGRATED=0)에 보존한다.
 * MCU/RTOS 메모리 여유를 실제 map/진단으로 확인하고 임의 framebuffer는 없다. */
#if defined(NOODOE_INTEGRATED) && NOODOE_INTEGRATED
#undef LV_MEM_SIZE
#define LV_MEM_SIZE (32 * 1024U)
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
#undef LV_MEM_SIZE
#define LV_MEM_SIZE (48 * 1024U)
#define LV_MEM_POOL_INCLUDE "Graphics_Memory.h"
#define LV_MEM_POOL_ALLOC GraphicsMemory_Pool
/* Product owns every style explicitly and clears the default theme. Do not
 * allocate or link the unused simple theme before immediately discarding it. */
#undef LV_USE_THEME_SIMPLE
#define LV_USE_THEME_SIMPLE 0
#endif
#undef LV_USE_ANIMIMG
#define LV_USE_ANIMIMG 0
#undef LV_USE_BAR
#define LV_USE_BAR 0
#undef LV_USE_BUTTON
#define LV_USE_BUTTON 0
#undef LV_USE_BUTTONMATRIX
#define LV_USE_BUTTONMATRIX 0
#undef LV_USE_CALENDAR
#define LV_USE_CALENDAR 0
#undef LV_USE_CHART
#define LV_USE_CHART 0
#undef LV_USE_CHECKBOX
#define LV_USE_CHECKBOX 0
#undef LV_USE_DROPDOWN
#define LV_USE_DROPDOWN 0
#undef LV_USE_IMAGE
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
#define LV_USE_IMAGE 1
#else
#define LV_USE_IMAGE 0
#endif
#undef LV_USE_IMAGEBUTTON
#define LV_USE_IMAGEBUTTON 0
#undef LV_USE_KEYBOARD
#define LV_USE_KEYBOARD 0
#undef LV_USE_LED
#define LV_USE_LED 0
#undef LV_USE_LINE
#if defined(NOODOE_PRODUCT) && NOODOE_PRODUCT
/* Product clock separator: EVE-supported three-segment line, no layer. */
#define LV_USE_LINE 1
#else
#define LV_USE_LINE 0
#endif
#undef LV_USE_LIST
#define LV_USE_LIST 0
#undef LV_USE_MENU
#define LV_USE_MENU 0
#undef LV_USE_MSGBOX
#define LV_USE_MSGBOX 0
#undef LV_USE_ROLLER
#define LV_USE_ROLLER 0
#undef LV_USE_SCALE
#define LV_USE_SCALE 0
#undef LV_USE_SLIDER
#define LV_USE_SLIDER 0
#undef LV_USE_SPAN
#define LV_USE_SPAN 0
#undef LV_USE_SPINBOX
#define LV_USE_SPINBOX 0
#undef LV_USE_SPINNER
#define LV_USE_SPINNER 0
#undef LV_USE_SWITCH
#define LV_USE_SWITCH 0
#undef LV_USE_TABLE
#define LV_USE_TABLE 0
#undef LV_USE_TABVIEW
#define LV_USE_TABVIEW 0
#undef LV_USE_TEXTAREA
#define LV_USE_TEXTAREA 0
#undef LV_USE_TILEVIEW
#define LV_USE_TILEVIEW 0
#undef LV_USE_WIN
#define LV_USE_WIN 0
#undef LV_USE_THEME_DEFAULT
#define LV_USE_THEME_DEFAULT 0
#undef LV_USE_FLEX
#define LV_USE_FLEX 0
#undef LV_USE_GRID
#define LV_USE_GRID 0
#undef LV_USE_OBSERVER
#define LV_USE_OBSERVER 0
#endif
