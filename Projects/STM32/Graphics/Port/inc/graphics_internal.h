#ifndef GRAPHICS_INTERNAL_H
#define GRAPHICS_INTERNAL_H
#include "Graphics.h"

/* Graphics포팅내부에서만공유하는수명/전송경계. 제품UI는Graphics.h만사용한다. */
lv_display_t *Graphics_EveCreateDisplay(void);
Graphics_Status Graphics_EvePoll(void);
Graphics_Status Graphics_EveResetCache(void);
Graphics_Status Graphics_EvePreloadImage(const lv_image_dsc_t *image);
Graphics_Status Graphics_EvePreloadText(const lv_font_t *font, const char *text);
void Graphics_EveReleaseCache(void);
void Graphics_InputInit(lv_display_t *display, lv_group_t *group);
void Graphics_InputDeinit(void);
void Graphics_InputProcess(void);
lv_indev_t *Graphics_InputGetDevice(void);
void Graphics_InputSetEnabled(uint32_t enabled);
void Graphics_InputSetCallback(Graphics_ButtonCallback callback, void *context);
Graphics_Status Graphics_RecordError(Graphics_Status error);
#endif
