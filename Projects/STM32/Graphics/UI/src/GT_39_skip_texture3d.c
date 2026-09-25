#include "Graphics_Test_Internal.h"

/* 이 항목은 지원되지 않는 기능을 실행하지 않는다. 표준 UI 순서에 사유 화면을
 * 남겨 자동 순환 후에도 미지원 기능을 RENDERED/PASS로 오해하지 않게 한다. */
const GT_Case gt_skip_texture3d={{39U,"3D texture","3D rendering / texture target",GRAPHICS_TEST_CAP_LAYER,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"The 3D texture widget is disabled; no 3D backend exists in this build."},GT_Skip,GT_NoTick};

