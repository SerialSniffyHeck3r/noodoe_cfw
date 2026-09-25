#include "SpeedHome_Layout.h"
/* X, baseline and box width. Align at measured font baseline, not nominal px.
 * Footer order is mode/icon, larger tabular number, then fixed unit.
 * Neither the mode nor km/mi changes any other field's origin. Lato units
 * end at ink y436 (baseline437), matching the D-DIN36 numeric ink envelope. */
const SpeedHomeLabelLayout speed_home_labels[SH_LABEL_COUNT]={
    [SH_CLOCK]        ={144,71,192,SH_NUMBER,48,SH_CENTER},
    [SH_CARD_TITLE]   ={92,164,296,SH_TEXT,32,SH_CENTER},
    [SH_CARD_LINE]    ={84,212,312,SH_TEXT,24,SH_CENTER},
    [SH_CARD_HINT]    ={92,255,296,SH_TEXT,20,SH_CENTER},
    [SH_CARD_NUMBER]  ={112,255,256,SH_NUMBER,32,SH_CENTER},
    [SH_UART]         ={78,336,92,SH_TEXT,20,SH_LEFT},
    [SH_FPS_TITLE]    ={181,336,37,SH_TEXT,20,SH_LEFT},
    [SH_FPS_VALUE]    ={221,336,60,SH_NUMBER,32,SH_RIGHT},
    [SH_CPU_TITLE]    ={290,336,44,SH_TEXT,20,SH_LEFT},
    [SH_CPU_VALUE]    ={336,336,74,SH_NUMBER,32,SH_RIGHT},
    [SH_PERCENT]     ={413,336,22,SH_TEXT,20,SH_LEFT},
    [SH_FOOTER_TITLE]={118,424,76,SH_TEXT,20,SH_LEFT},
    [SH_FOOTER_VALUE]={194,436,116,SH_NUMBER,36,SH_RIGHT},
    [SH_FOOTER_UNIT] ={322,437,40,SH_TEXT,20,SH_LEFT},
    [SH_FOOTER_AUX_TITLE]={145,461,58,SH_TEXT,16,SH_LEFT},
    [SH_FOOTER_AUX_VALUE]={210,464,100,SH_NUMBER,20,SH_RIGHT}
};
/* One source for both construction and animated endpoints. With the22px
 * ring its inner radius is218. The endpoint radius214.72 plus the1.5px
 * rounded cap leaves1.78px clearance, including antialiasing. */
const int16_t speed_home_separator[SPEED_HOME_TRAPEZOID_POINTS][2]=
    {{131,55},{172,87},{308,87},{349,55}};

/* Keep a trapezoid ABOVE the distance row; only its lower border is straight.
 * Preserve the expanded Y401..448 gap and every glyph's fixed position.
 * The upper arms stay outside mode/value/unit ink. The lower chord is shorter
 * and remains separate from the centered oil remaining bar below it. */
/* Continue the24:19 footer diagonals DOWN/outward to the physical circle.
 * The speed arc is absent here. Endpoints are rounded inward by less than
 * one pixel from the original rays; only the3px caps need circle clipping.
 * No horizontal appendage or changed distance row is introduced. */
const int16_t speed_home_footer_separator[SPEED_HOME_TRAPEZOID_POINTS][2]=
    {{87,424},{116,401},{364,401},{392,424}};
const int16_t speed_home_footer_bottom_separator[2][2]={{144,448},{336,448}};
