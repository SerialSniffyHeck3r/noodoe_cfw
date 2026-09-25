#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#define __get_IPSR() 0U
#define __get_PRIMASK() 0U
#define __get_BASEPRI() 0U
#define __disable_irq() ((void)0)
#define __set_PRIMASK(x) ((void)(x))
#define __DMB() __asm volatile("dmb" ::: "memory")
typedef struct {volatile uint32_t KR;} TestIwdg;
extern TestIwdg test_iwdg;
#define IWDG (&test_iwdg)
uint32_t HAL_GetTick(void);
#endif
