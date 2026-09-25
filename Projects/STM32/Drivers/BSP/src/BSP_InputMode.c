#include "BSP_InputMode.h"
#include "stm32f4xx_hal.h"
void BSP_InputMode_Init(void)
{
    static uint32_t initialized;if(initialized)return;
    __HAL_RCC_GPIOH_CLK_ENABLE();
    GPIO_InitTypeDef pin={0};pin.Pin=GPIO_PIN_9;
    pin.Mode=GPIO_MODE_INPUT;pin.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOH,&pin);initialized=1;
}
uint32_t BSP_InputMode_Read(void)
{return !!(GPIOH->IDR&GPIO_PIN_9);}
