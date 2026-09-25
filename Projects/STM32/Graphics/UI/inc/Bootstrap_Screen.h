#ifndef BOOTSTRAP_SCREEN_H
#define BOOTSTRAP_SCREEN_H
#include "Bootstrap_UI.h"
/* No LVGL, external font, image, SDRAM or heap dependency. */
/* 0=submitted, 1=previous frame still pending, 2=bus/not-ready failure. */
uint32_t BootstrapScreen_Draw(const BootstrapView *view);
#endif
