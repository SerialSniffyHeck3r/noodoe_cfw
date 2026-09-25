#ifndef SYSTEM_ERROR_VIEW_H
#define SYSTEM_ERROR_VIEW_H
#include <stdint.h>
/* Raw ROM-font frame; no LVGL, no SDRAM, no application assets. Owner only. */
uint32_t SystemErrorView_Draw(uint32_t code,uint32_t detail,uint32_t stage,uint32_t recovery);
#endif
