#ifndef CONTROL_TEST_HAL_H
#define CONTROL_TEST_HAL_H
#include <stdint.h>
static inline uint32_t HAL_GetUIDw0(void){return 0x12345678U;}
static inline uint32_t HAL_GetUIDw1(void){return 0x90ABCDEFU;}
static inline uint32_t HAL_GetUIDw2(void){return 0x00001234U;}
#endif
