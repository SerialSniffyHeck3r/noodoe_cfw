#include "Graphics_Test_Internal.h"

/* 이 항목은 지원되지 않는 기능을 실행하지 않는다. 표준 UI 순서에 사유 화면을
 * 남겨 자동 순환 후에도 미지원 기능을 RENDERED/PASS로 오해하지 않게 한다. */
const GT_Case gt_skip_svg={{35U,"SVG and vectors","SVG / vector paths",GRAPHICS_TEST_CAP_VECTOR,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"SVG/vector decoders and vector drawing backend are disabled."},GT_Skip,GT_NoTick};

