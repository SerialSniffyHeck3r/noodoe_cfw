#ifndef GRAPHICS_VIEWPORT_H
#define GRAPHICS_VIEWPORT_H

/* 고정 활성 영역의 유일한 기준. USB album/1/8f3c6ec2bb08c71586b00c691a8192a7.jpg
 * (480x480, SHA256 ccc245855ac9df2532b168bbbc0fca5c93b9e4cc8bf7e32bb56c73da0225503b)
 * 외곽 fit=(239.4974,239.4943), r=239.5149px는 최초 기준의 근거다.
 * 후속 사용자 지시에 따라 활성 지름은480px: 중심239.5px, 반경240px다.
 * 중앙 두 행/열239,240이0..479 전체를 포함하며 모서리는 원 밖이다.
 * JPEG나 물리 베젤에서 새 반경을 실측했다는 의미가 아니다. VIEWPORT.md 참조.
 * 반 pixel 중심을 잃지 않도록 좌표/반경은 모두2배 정수다. pixel은0..479. */
#define GRAPHICS_WIDTH 480U
#define GRAPHICS_HEIGHT 480U
#define GRAPHICS_ACTIVE_CENTER_X2 479
#define GRAPHICS_ACTIVE_CENTER_Y2 479
#define GRAPHICS_ACTIVE_RADIUS_X2 480

/* Home-bench boundary visualization. Set0 for the production black surround.
 * This changes only the final outside fill, never geometry or input clipping. */
#ifndef GRAPHICS_DEV_VIEWPORT
#define GRAPHICS_DEV_VIEWPORT 1
#endif
#define GRAPHICS_DEV_OUTSIDE_RGB 0x4A5952U
#if GRAPHICS_DEV_VIEWPORT
#define GRAPHICS_OUTSIDE_RGB GRAPHICS_DEV_OUTSIDE_RGB
#else
#define GRAPHICS_OUTSIDE_RGB 0x000000U
#endif

#endif
