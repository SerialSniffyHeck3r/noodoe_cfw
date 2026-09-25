#ifndef RUNTIME_UPDATE_TEST_PORT_H
#define RUNTIME_UPDATE_TEST_PORT_H
#include "BSP_RAM.h"
#include "BSP_NOR.h"
#include "NoodoeBluetooth.h"
uint32_t RuntimeUpdateTest_ContextOkay(void);
void RuntimeUpdateTest_Reset(void);
#define RU_CONTEXT_OK() RuntimeUpdateTest_ContextOkay()
#define RU_RESET() RuntimeUpdateTest_Reset()
#endif
