#ifndef STOCK_PHOTO_TEST_HAL_H
#define STOCK_PHOTO_TEST_HAL_H
#include <stdint.h>
typedef void SPI_HandleTypeDef;
static inline uint32_t __get_PRIMASK(void){return 0;}
static inline void __disable_irq(void){}
static inline void __set_PRIMASK(uint32_t value){(void)value;}
static inline void __DMB(void){__asm volatile("" ::: "memory");}
#endif
