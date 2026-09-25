/* LVGL 9.5 EVE caches opposite corners separately, although SCISSOR_SIZE
 * depends on BOTH corners. RESTORE_CONTEXT also changes the hardware clip
 * without updating that cache. Interpose the complete clip operation instead
 * of emitting a dummy offscreen clip before every draw task. Two commands
 * describe one inclusive LVGL rectangle, even after raw SAVE/RESTORE or moving
 * only its top/left edge. No one-pixel expansion into the adjacent page/half. */
#include <stdint.h>
#include "src/libs/FT800-FT813/EVE_commands.h"
void __wrap_lv_eve_scissor(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
    EVE_cmd_dl_burst(SCISSOR_XY(x1,y1));
    EVE_cmd_dl_burst(SCISSOR_SIZE(x2>=x1?x2-x1+1U:0U,y2>=y1?y2-y1+1U:0U));
}
