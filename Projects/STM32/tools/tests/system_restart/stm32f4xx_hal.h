#ifndef RESTART_TEST_HAL_H
#define RESTART_TEST_HAL_H
void Test_Reset(void);
#define NVIC_SystemReset() Test_Reset()
#endif
