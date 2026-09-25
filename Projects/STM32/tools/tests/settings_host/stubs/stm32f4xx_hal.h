#ifndef SETTINGS_TEST_HAL_H
#define SETTINGS_TEST_HAL_H
#include <stdint.h>
typedef struct {uint32_t unused;} SPI_HandleTypeDef;
static inline uint32_t HAL_GetUIDw0(void){return 0x12345678U;}
static inline uint32_t HAL_GetUIDw1(void){return 0x90ABCDEFU;}
static inline uint32_t HAL_GetUIDw2(void){return 0x00001234U;}
static inline uint32_t __get_IPSR(void){return 0U;}
static inline uint32_t __get_PRIMASK(void){return 0U;}
static inline uint32_t __get_BASEPRI(void){return 0U;}
static inline void __disable_irq(void){}
static inline void __set_PRIMASK(uint32_t value){(void)value;}
static inline void __DMB(void){__asm volatile("" ::: "memory");}
#endif
