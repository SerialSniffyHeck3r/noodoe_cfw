#include "Graphics_Test_Internal.h"

/* 이 항목은 지원되지 않는 기능을 실행하지 않는다. 표준 UI 순서에 사유 화면을
 * 남겨 자동 순환 후에도 미지원 기능을 RENDERED/PASS로 오해하지 않게 한다. */
const GT_Case gt_skip_canvas={{30U,"Canvas","Canvas / framebuffer",GRAPHICS_TEST_CAP_IMAGE,GRAPHICS_TEST_SUPPORT_CONFIG_DISABLED,"Canvas needs a framebuffer/render path not enabled in this EVE-only build."},GT_Skip,GT_NoTick};

