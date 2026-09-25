#include "BSP_Power.h"
#include "stm32f4xx_hal.h"
volatile BSP_Power_Diagnostics g_bsp_power;
static uint32_t candidate_since;

void BSP_Power_Init(uint32_t board_revision)
{
    GPIO_InitTypeDef pin={0};__HAL_RCC_GPIOG_CLK_ENABLE();
    /* Both edges wake owners; the task retains the11ms debounce. Preserve
     * other EXTI15_10 button mappings and their shared priority5 handler. */
    pin.Pin=GPIO_PIN_13;pin.Mode=GPIO_MODE_IT_RISING_FALLING;pin.Pull=GPIO_NOPULL;HAL_GPIO_Init(GPIOG,&pin);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn,5U,0U);HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    g_bsp_power.magic=0x50575231U;g_bsp_power.board_revision=board_revision;
    g_bsp_power.raw_ign_off=(GPIOG->IDR&GPIO_PIN_13)!=0U;
    candidate_since=HAL_GetTick();g_bsp_power.ready=1U;
}
void BSP_Power_Process(uint32_t now_ms)
{
    if(!g_bsp_power.ready)return;
    uint32_t raw=(GPIOG->IDR&GPIO_PIN_13)!=0U;
    if(raw!=g_bsp_power.raw_ign_off){g_bsp_power.raw_ign_off=raw;candidate_since=now_ms;}
    if((uint32_t)(now_ms-candidate_since)>=BSP_POWER_IGN_DEBOUNCE_MS &&
       (!g_bsp_power.ign_valid || g_bsp_power.ign_on==raw)){
        g_bsp_power.ign_valid=1U;g_bsp_power.ign_on=!raw;
        ++g_bsp_power.changes;g_bsp_power.last_change_ms=now_ms;
    }
}
uint32_t BSP_Power_RequestOff(void)
{
    if(!g_bsp_power.ready || !g_bsp_power.ign_valid || g_bsp_power.ign_on)return 1U;
    /* A fresh ON edge must cancel shutdown even during its debounce window. */
    if((GPIOG->IDR&GPIO_PIN_13)==0U)return 1U;
    __HAL_RCC_GPIOD_CLK_ENABLE();__HAL_RCC_GPIOG_CLK_ENABLE();
    /* Match stock fail/shutdown outputs; do not infer completed power loss
     * from these register writes. The external circuit remains authoritative. */
    HAL_GPIO_WritePin(GPIOD,GPIO_PIN_13,GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOG,GPIO_PIN_14,GPIO_PIN_SET);
    GPIO_InitTypeDef pin={0};pin.Mode=GPIO_MODE_OUTPUT_PP;pin.Pull=GPIO_NOPULL;pin.Speed=GPIO_SPEED_FREQ_LOW;
    pin.Pin=GPIO_PIN_13;HAL_GPIO_Init(GPIOD,&pin);pin.Pin=GPIO_PIN_14;HAL_GPIO_Init(GPIOG,&pin);
    if(g_bsp_power.board_revision>=3U && g_bsp_power.board_revision<256U){
        __HAL_RCC_GPIOI_CLK_ENABLE();HAL_GPIO_WritePin(GPIOI,GPIO_PIN_9,GPIO_PIN_SET);
        pin.Pin=GPIO_PIN_9;HAL_GPIO_Init(GPIOI,&pin);
    }
    g_bsp_power.shutdown_outputs=1U;return 0U;
}
