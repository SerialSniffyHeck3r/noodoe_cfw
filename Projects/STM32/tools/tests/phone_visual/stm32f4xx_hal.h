#include <stdint.h>
static inline uint32_t __get_PRIMASK(void){uint32_t v;__asm volatile("mrs %0, primask":"=r"(v));return v;}
static inline void __disable_irq(void){__asm volatile("cpsid i":::"memory");}
static inline void __set_PRIMASK(uint32_t v){__asm volatile("msr primask, %0"::"r"(v):"memory");}
