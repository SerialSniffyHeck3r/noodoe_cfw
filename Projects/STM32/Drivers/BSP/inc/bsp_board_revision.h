#ifndef BSP_BOARD_REVISION_H
#define BSP_BOARD_REVISION_H
#include "stm32f4xx.h"
/* Stock strap order PA3,PH3,PH2,PB10. Factory PCBA and panel selector are
 * independent fields. Only these four input pins are configured here. */
static inline uint32_t BSP_BoardRevision(void)
{
 RCC->AHB1ENR|=RCC_AHB1ENR_GPIOAEN|RCC_AHB1ENR_GPIOBEN|RCC_AHB1ENR_GPIOHEN;
 (void)RCC->AHB1ENR;
 static const struct {GPIO_TypeDef *port;uint32_t mask;} inputs[]={
  {GPIOA,3U<<6},{GPIOH,(3U<<6)|(3U<<4)},{GPIOB,3U<<20}};
 for(uint32_t i=0;i<3;i++){
  inputs[i].port->MODER&=~inputs[i].mask;
  inputs[i].port->PUPDR&=~inputs[i].mask;
 }
 uint32_t a=GPIOA->IDR,h=GPIOH->IDR,b=GPIOB->IDR;
 return ((a>>3)&1U)|(((h>>3)&1U)<<1)|(((h>>2)&1U)<<2)|(((b>>10)&1U)<<3);
}
#endif
