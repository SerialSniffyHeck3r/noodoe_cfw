#ifndef SPEED_HOME_LAYOUT_H
#define SPEED_HOME_LAYOUT_H
#include <stdint.h>
/* One geometry catalog: UI boxes stay fixed while glyph advances are natural.
 * Arc path radius is240-18/2=231. Its two ends at135/405degrees have center
 * Y=240+231/sqrt(2)=403.34. Footer starts BELOW that line, in the round cap.
 * Every footer rectangle must also fit the exact circular viewport. */
#define SPEED_HOME_FOOTER_TOP 404
#define SPEED_HOME_FOOTER_BOTTOM 464
#define SPEED_HOME_FOOTER_INK_TOP 410
#define SPEED_HOME_ICON_X 118
#define SPEED_HOME_RING_STROKE 22
/* One thickness for clock, upper footer trapezoid and lower straight border. */
#define SPEED_HOME_SEPARATOR_STROKE 3
/* Fixed central riding-UI viewport in absolute screen pixels. Its complete
 * rectangle fits inside the speed ring and clears both separator strokes.
 * New menu/notification/remote-control renderers use local coordinates here;
 * clock, speed ring, distance footer and development HUD remain shell-owned. */
#define SPEED_HOME_CONTENT_X 72
#define SPEED_HOME_CONTENT_Y 105
#define SPEED_HOME_CONTENT_WIDTH 336
/* Bottom384 reclaims9px while keeping even all four viewport corners inside
 * the speed ring. The distance divider stays at401 and page glyphs at376. */
#define SPEED_HOME_CONTENT_HEIGHT 280
typedef enum {SH_CLOCK,SH_CARD_TITLE,SH_CARD_LINE,SH_CARD_HINT,SH_CARD_NUMBER,
    SH_UART,SH_FPS_TITLE,SH_FPS_VALUE,SH_CPU_TITLE,SH_CPU_VALUE,SH_PERCENT,
    SH_FOOTER_TITLE,SH_FOOTER_VALUE,SH_FOOTER_UNIT,SH_FOOTER_AUX_TITLE,SH_FOOTER_AUX_VALUE,SH_LABEL_COUNT} SpeedHomeLabel;
typedef enum {SH_TEXT,SH_NUMBER} SpeedHomeFontRole;
typedef enum {SH_LEFT,SH_CENTER,SH_RIGHT} SpeedHomeAlignment;
typedef struct {int16_t x,baseline,width;uint8_t role,pixels,align;} SpeedHomeLabelLayout;
extern const SpeedHomeLabelLayout speed_home_labels[SH_LABEL_COUNT];
/* Extend the original inclined arms; do not add horizontal outer segments.
 * Clock ends stop at the speed ring's inner edge, including half-stroke.
 * Footer ends continue downward through the open arc sector to the circle;
 * the existing stencil trims their outside half-caps. The ring is preserved. */
#define SPEED_HOME_TRAPEZOID_POINTS 4
extern const int16_t speed_home_separator[SPEED_HOME_TRAPEZOID_POINTS][2];
/* The distance row's upper trapezoid extends outward; its glyphs stay fixed. */
extern const int16_t speed_home_footer_separator[SPEED_HOME_TRAPEZOID_POINTS][2];
/* Shorter lower chord, above the alternative oil-line / DAYS region. */
extern const int16_t speed_home_footer_bottom_separator[2][2];
#endif
