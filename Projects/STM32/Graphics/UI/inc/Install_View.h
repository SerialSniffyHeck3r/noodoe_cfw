#ifndef NOODOE_INSTALL_VIEW_H
#define NOODOE_INSTALL_VIEW_H
#include "lvgl.h"
#include "InstallSession.h"
uint32_t InstallView_Create(lv_obj_t *screen);
void InstallView_Render(uint32_t visible,const InstallSession *session,uint32_t now);
#endif
