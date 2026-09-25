#ifndef PRODUCT_MODE_STRIP_H
#define PRODUCT_MODE_STRIP_H
#include "lvgl.h"
#define PRODUCT_MODE_STRIP_Y 97
#define PRODUCT_MODE_STRIP_HEIGHT 40
#define PRODUCT_MODE_ICON_SMALL 22U
#define PRODUCT_MODE_ICON_LARGE 36U
typedef struct {uint32_t magic,seq,active,current,target,elapsed_ms,weights[9];int32_t centers_x[9];} ProductModeStripDiagnostics;
extern volatile ProductModeStripDiagnostics g_mode_strip;
/* Graphics-owner only. Five visible circular slots, selected mode atX240.
 * Update alone advances the shared240ms slide/scale/white-gray animation.
 * PassingUINT32_MAX keeps the last request (e.g. an incoming page is busy).
 * Same-mode/subpage updates do not restart. No allocation or device I/O. */
/* shell is the480x480 screen parent: the raised strip is a sibling of the
 * clipped page body, so enlarging icons cannot clip them at the oldY105 top. */
uint32_t ProductModeStrip_Create(lv_obj_t *shell);
void ProductModeStrip_Update(uint32_t mode,uint32_t now_ms);
void ProductModeStrip_SetVisible(uint32_t visible);
void ProductModeStrip_SetOpacity(uint32_t visible,uint32_t now);
#endif
