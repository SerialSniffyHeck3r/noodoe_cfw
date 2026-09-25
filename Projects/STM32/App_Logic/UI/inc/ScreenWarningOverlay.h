#ifndef SCREEN_WARNING_OVERLAY_H
#define SCREEN_WARNING_OVERLAY_H
#include <stdint.h>
typedef struct {uint32_t active,icon,color,started,blink_ms,text_ms,phase,opacity,size,y;char message[48];} ScreenWarningState;
/* UI-owner only, bounded storage/no allocation. Same icon+message is coalesced;
 * priority is supplied separately by the domain caller. */
void ScreenWarningOverlay(uint32_t icon,uint32_t color,uint32_t seconds1,uint32_t seconds2,const char *message);
void ScreenWarningOverlay_Process(uint32_t now,uint32_t allowed);
uint32_t ScreenWarningOverlay_Button(uint32_t button,uint32_t event);
void ScreenWarningOverlay_Get(ScreenWarningState *out);
void ScreenWarningOverlay_Close(void);
#endif
