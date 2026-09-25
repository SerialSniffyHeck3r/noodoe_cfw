/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, STOCK_PE3_Pin|LCD_FRAME_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, BACKLIGHT_POWER_CTRL_Pin|STOCK_PI1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RESET_CTRL_GPIO_Port, LCD_RESET_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, BOARD_REV_POWER_CTRL_Pin|LCD_ENABLE_CTRL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(NOR_CS_N_GPIO_Port, NOR_CS_N_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, STOCK_PC1_Pin|BACKLIGHT_DISABLE_CTRL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EVE_CS_N_GPIO_Port, EVE_CS_N_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(EVE_PDN_N_GPIO_Port, EVE_PDN_N_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BOARD_POWER_HOLD_GPIO_Port, BOARD_POWER_HOLD_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STOCK_PA8_GPIO_Port, STOCK_PA8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MFI_RESET_N_GPIO_Port, MFI_RESET_N_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BOARD_POWER_CTRL_GPIO_Port, BOARD_POWER_CTRL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : STOCK_PE3_Pin */
  GPIO_InitStruct.Pin = STOCK_PE3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(STOCK_PE3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_FRAME_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_FRAME_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_FRAME_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BACKLIGHT_POWER_CTRL_Pin BOARD_REV_POWER_CTRL_Pin STOCK_PI1_Pin */
  GPIO_InitStruct.Pin = BACKLIGHT_POWER_CTRL_Pin|BOARD_REV_POWER_CTRL_Pin|STOCK_PI1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RESET_CTRL_Pin STOCK_PC1_Pin BACKLIGHT_DISABLE_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_RESET_CTRL_Pin|STOCK_PC1_Pin|BACKLIGHT_DISABLE_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_UNUSED_PI10_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PI10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_UNUSED_PI10_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_ENABLE_CTRL_Pin */
  GPIO_InitStruct.Pin = LCD_ENABLE_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_ENABLE_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NOR_CS_N_Pin */
  GPIO_InitStruct.Pin = NOR_CS_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(NOR_CS_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_UNUSED_PF10_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PF10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_UNUSED_PF10_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_UNUSED_PA0_Pin STOCK_UNUSED_PA1_Pin STOCK_UNUSED_PA2_Pin STOCK_UNUSED_PA5_Pin
                           STOCK_UNUSED_PA7_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PA0_Pin|STOCK_UNUSED_PA1_Pin|STOCK_UNUSED_PA2_Pin|STOCK_UNUSED_PA5_Pin
                          |STOCK_UNUSED_PA7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_PH2_Pin STOCK_PH3_Pin STOCK_PH9_Pin */
  GPIO_InitStruct.Pin = STOCK_PH2_Pin|STOCK_PH3_Pin|STOCK_PH9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PA3_Pin */
  GPIO_InitStruct.Pin = STOCK_PA3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PA3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EVE_CS_N_Pin */
  GPIO_InitStruct.Pin = EVE_CS_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EVE_CS_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EVE_PDN_N_Pin */
  GPIO_InitStruct.Pin = EVE_PDN_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(EVE_PDN_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PB10_Pin */
  GPIO_InitStruct.Pin = STOCK_PB10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PB10_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_UNUSED_PB11_Pin STOCK_UNUSED_PB12_Pin STOCK_UNUSED_PB13_Pin PB14
                           PB15 STOCK_UNUSED_PB4_Pin STOCK_UNUSED_PB8_Pin STOCK_UNUSED_PB9_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PB11_Pin|STOCK_UNUSED_PB12_Pin|STOCK_UNUSED_PB13_Pin|GPIO_PIN_14
                          |GPIO_PIN_15|STOCK_UNUSED_PB4_Pin|STOCK_UNUSED_PB8_Pin|STOCK_UNUSED_PB9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_UNUSED_PH6_Pin STOCK_UNUSED_PH8_Pin STOCK_UNUSED_PH10_Pin STOCK_UNUSED_PH11_Pin
                           STOCK_UNUSED_PH12_Pin STOCK_UNUSED_PH14_Pin STOCK_UNUSED_PH15_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PH6_Pin|STOCK_UNUSED_PH8_Pin|STOCK_UNUSED_PH10_Pin|STOCK_UNUSED_PH11_Pin
                          |STOCK_UNUSED_PH12_Pin|STOCK_UNUSED_PH14_Pin|STOCK_UNUSED_PH15_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_PD11_Pin STOCK_PD3_Pin STOCK_PD4_Pin STOCK_PD5_Pin
                           STOCK_PD7_Pin */
  GPIO_InitStruct.Pin = STOCK_PD11_Pin|STOCK_PD3_Pin|STOCK_PD4_Pin|STOCK_PD5_Pin
                          |STOCK_PD7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PD12_EXTI_Pin */
  GPIO_InitStruct.Pin = STOCK_PD12_EXTI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PD12_EXTI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOARD_POWER_HOLD_Pin */
  GPIO_InitStruct.Pin = BOARD_POWER_HOLD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BOARD_POWER_HOLD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_PG3_Pin STOCK_PG9_Pin STOCK_PG10_Pin */
  GPIO_InitStruct.Pin = STOCK_PG3_Pin|STOCK_PG9_Pin|STOCK_PG10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_UNUSED_PG6_Pin STOCK_UNUSED_PG7_Pin STOCK_UNUSED_PG11_Pin STOCK_UNUSED_PG12_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PG6_Pin|STOCK_UNUSED_PG7_Pin|STOCK_UNUSED_PG11_Pin|STOCK_UNUSED_PG12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_UNUSED_PC6_Pin STOCK_UNUSED_PC7_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PC6_Pin|STOCK_UNUSED_PC7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PA8_Pin */
  GPIO_InitStruct.Pin = STOCK_PA8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(STOCK_PA8_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MFI_RESET_N_Pin */
  GPIO_InitStruct.Pin = MFI_RESET_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MFI_RESET_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PI2_Pin */
  GPIO_InitStruct.Pin = STOCK_PI2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PI2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_PI3_EXTI_Pin STOCK_PI4_EXTI_Pin STOCK_PI5_EXTI_Pin */
  GPIO_InitStruct.Pin = STOCK_PI3_EXTI_Pin|STOCK_PI4_EXTI_Pin|STOCK_PI5_EXTI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PA15_EXTI_Pin */
  GPIO_InitStruct.Pin = STOCK_PA15_EXTI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PA15_EXTI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : STOCK_PC10_Pin STOCK_PC11_Pin */
  GPIO_InitStruct.Pin = STOCK_PC10_Pin|STOCK_PC11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_UNUSED_PD6_Pin */
  GPIO_InitStruct.Pin = STOCK_UNUSED_PD6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_UNUSED_PD6_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PG13_EXTI_Pin */
  GPIO_InitStruct.Pin = STOCK_PG13_EXTI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PG13_EXTI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOARD_POWER_CTRL_Pin */
  GPIO_InitStruct.Pin = BOARD_POWER_CTRL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BOARD_POWER_CTRL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : STOCK_PI6_EXTI_Pin */
  GPIO_InitStruct.Pin = STOCK_PI6_EXTI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(STOCK_PI6_EXTI_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
