/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define STOCK_PE3_Pin GPIO_PIN_3
#define STOCK_PE3_GPIO_Port GPIOE
#define LCD_FRAME_CTRL_Pin GPIO_PIN_4
#define LCD_FRAME_CTRL_GPIO_Port GPIOE
#define BACKLIGHT_POWER_CTRL_Pin GPIO_PIN_8
#define BACKLIGHT_POWER_CTRL_GPIO_Port GPIOI
#define LCD_RESET_CTRL_Pin GPIO_PIN_13
#define LCD_RESET_CTRL_GPIO_Port GPIOC
#define BOARD_REV_POWER_CTRL_Pin GPIO_PIN_9
#define BOARD_REV_POWER_CTRL_GPIO_Port GPIOI
#define STOCK_UNUSED_PI10_Pin GPIO_PIN_10
#define STOCK_UNUSED_PI10_GPIO_Port GPIOI
#define LCD_ENABLE_CTRL_Pin GPIO_PIN_11
#define LCD_ENABLE_CTRL_GPIO_Port GPIOI
#define NOR_CS_N_Pin GPIO_PIN_6
#define NOR_CS_N_GPIO_Port GPIOF
#define STOCK_UNUSED_PF10_Pin GPIO_PIN_10
#define STOCK_UNUSED_PF10_GPIO_Port GPIOF
#define STOCK_PC1_Pin GPIO_PIN_1
#define STOCK_PC1_GPIO_Port GPIOC
#define STOCK_UNUSED_PA0_Pin GPIO_PIN_0
#define STOCK_UNUSED_PA0_GPIO_Port GPIOA
#define STOCK_UNUSED_PA1_Pin GPIO_PIN_1
#define STOCK_UNUSED_PA1_GPIO_Port GPIOA
#define STOCK_UNUSED_PA2_Pin GPIO_PIN_2
#define STOCK_UNUSED_PA2_GPIO_Port GPIOA
#define STOCK_PH2_Pin GPIO_PIN_2
#define STOCK_PH2_GPIO_Port GPIOH
#define STOCK_PH3_Pin GPIO_PIN_3
#define STOCK_PH3_GPIO_Port GPIOH
#define STOCK_PA3_Pin GPIO_PIN_3
#define STOCK_PA3_GPIO_Port GPIOA
#define EVE_CS_N_Pin GPIO_PIN_4
#define EVE_CS_N_GPIO_Port GPIOA
#define STOCK_UNUSED_PA5_Pin GPIO_PIN_5
#define STOCK_UNUSED_PA5_GPIO_Port GPIOA
#define STOCK_UNUSED_PA7_Pin GPIO_PIN_7
#define STOCK_UNUSED_PA7_GPIO_Port GPIOA
#define EVE_PDN_N_Pin GPIO_PIN_1
#define EVE_PDN_N_GPIO_Port GPIOB
#define STOCK_PB10_Pin GPIO_PIN_10
#define STOCK_PB10_GPIO_Port GPIOB
#define STOCK_UNUSED_PB11_Pin GPIO_PIN_11
#define STOCK_UNUSED_PB11_GPIO_Port GPIOB
#define STOCK_UNUSED_PH6_Pin GPIO_PIN_6
#define STOCK_UNUSED_PH6_GPIO_Port GPIOH
#define STOCK_UNUSED_PH8_Pin GPIO_PIN_8
#define STOCK_UNUSED_PH8_GPIO_Port GPIOH
#define STOCK_PH9_Pin GPIO_PIN_9
#define STOCK_PH9_GPIO_Port GPIOH
#define STOCK_UNUSED_PH10_Pin GPIO_PIN_10
#define STOCK_UNUSED_PH10_GPIO_Port GPIOH
#define STOCK_UNUSED_PH11_Pin GPIO_PIN_11
#define STOCK_UNUSED_PH11_GPIO_Port GPIOH
#define STOCK_UNUSED_PH12_Pin GPIO_PIN_12
#define STOCK_UNUSED_PH12_GPIO_Port GPIOH
#define STOCK_UNUSED_PB12_Pin GPIO_PIN_12
#define STOCK_UNUSED_PB12_GPIO_Port GPIOB
#define STOCK_UNUSED_PB13_Pin GPIO_PIN_13
#define STOCK_UNUSED_PB13_GPIO_Port GPIOB
#define STOCK_PD11_Pin GPIO_PIN_11
#define STOCK_PD11_GPIO_Port GPIOD
#define STOCK_PD12_EXTI_Pin GPIO_PIN_12
#define STOCK_PD12_EXTI_GPIO_Port GPIOD
#define STOCK_PD12_EXTI_EXTI_IRQn EXTI15_10_IRQn
#define BOARD_POWER_HOLD_Pin GPIO_PIN_13
#define BOARD_POWER_HOLD_GPIO_Port GPIOD
#define STOCK_PG3_Pin GPIO_PIN_3
#define STOCK_PG3_GPIO_Port GPIOG
#define STOCK_UNUSED_PG6_Pin GPIO_PIN_6
#define STOCK_UNUSED_PG6_GPIO_Port GPIOG
#define STOCK_UNUSED_PG7_Pin GPIO_PIN_7
#define STOCK_UNUSED_PG7_GPIO_Port GPIOG
#define STOCK_UNUSED_PC6_Pin GPIO_PIN_6
#define STOCK_UNUSED_PC6_GPIO_Port GPIOC
#define STOCK_UNUSED_PC7_Pin GPIO_PIN_7
#define STOCK_UNUSED_PC7_GPIO_Port GPIOC
#define BACKLIGHT_DISABLE_CTRL_Pin GPIO_PIN_8
#define BACKLIGHT_DISABLE_CTRL_GPIO_Port GPIOC
#define STOCK_PA8_Pin GPIO_PIN_8
#define STOCK_PA8_GPIO_Port GPIOA
#define MFI_RESET_N_Pin GPIO_PIN_13
#define MFI_RESET_N_GPIO_Port GPIOH
#define STOCK_UNUSED_PH14_Pin GPIO_PIN_14
#define STOCK_UNUSED_PH14_GPIO_Port GPIOH
#define STOCK_UNUSED_PH15_Pin GPIO_PIN_15
#define STOCK_UNUSED_PH15_GPIO_Port GPIOH
#define STOCK_PI1_Pin GPIO_PIN_1
#define STOCK_PI1_GPIO_Port GPIOI
#define STOCK_PI2_Pin GPIO_PIN_2
#define STOCK_PI2_GPIO_Port GPIOI
#define STOCK_PI3_EXTI_Pin GPIO_PIN_3
#define STOCK_PI3_EXTI_GPIO_Port GPIOI
#define STOCK_PI3_EXTI_EXTI_IRQn EXTI3_IRQn
#define STOCK_PA15_EXTI_Pin GPIO_PIN_15
#define STOCK_PA15_EXTI_GPIO_Port GPIOA
#define STOCK_PA15_EXTI_EXTI_IRQn EXTI15_10_IRQn
#define STOCK_PC10_Pin GPIO_PIN_10
#define STOCK_PC10_GPIO_Port GPIOC
#define STOCK_PC11_Pin GPIO_PIN_11
#define STOCK_PC11_GPIO_Port GPIOC
#define STOCK_PD3_Pin GPIO_PIN_3
#define STOCK_PD3_GPIO_Port GPIOD
#define STOCK_PD4_Pin GPIO_PIN_4
#define STOCK_PD4_GPIO_Port GPIOD
#define STOCK_PD5_Pin GPIO_PIN_5
#define STOCK_PD5_GPIO_Port GPIOD
#define STOCK_UNUSED_PD6_Pin GPIO_PIN_6
#define STOCK_UNUSED_PD6_GPIO_Port GPIOD
#define STOCK_PD7_Pin GPIO_PIN_7
#define STOCK_PD7_GPIO_Port GPIOD
#define STOCK_PG9_Pin GPIO_PIN_9
#define STOCK_PG9_GPIO_Port GPIOG
#define STOCK_PG10_Pin GPIO_PIN_10
#define STOCK_PG10_GPIO_Port GPIOG
#define STOCK_UNUSED_PG11_Pin GPIO_PIN_11
#define STOCK_UNUSED_PG11_GPIO_Port GPIOG
#define STOCK_UNUSED_PG12_Pin GPIO_PIN_12
#define STOCK_UNUSED_PG12_GPIO_Port GPIOG
#define STOCK_PG13_EXTI_Pin GPIO_PIN_13
#define STOCK_PG13_EXTI_GPIO_Port GPIOG
#define STOCK_PG13_EXTI_EXTI_IRQn EXTI15_10_IRQn
#define BOARD_POWER_CTRL_Pin GPIO_PIN_14
#define BOARD_POWER_CTRL_GPIO_Port GPIOG
#define STOCK_UNUSED_PB4_Pin GPIO_PIN_4
#define STOCK_UNUSED_PB4_GPIO_Port GPIOB
#define STOCK_UNUSED_PB8_Pin GPIO_PIN_8
#define STOCK_UNUSED_PB8_GPIO_Port GPIOB
#define STOCK_UNUSED_PB9_Pin GPIO_PIN_9
#define STOCK_UNUSED_PB9_GPIO_Port GPIOB
#define STOCK_PI4_EXTI_Pin GPIO_PIN_4
#define STOCK_PI4_EXTI_GPIO_Port GPIOI
#define STOCK_PI4_EXTI_EXTI_IRQn EXTI4_IRQn
#define STOCK_PI5_EXTI_Pin GPIO_PIN_5
#define STOCK_PI5_EXTI_GPIO_Port GPIOI
#define STOCK_PI5_EXTI_EXTI_IRQn EXTI9_5_IRQn
#define STOCK_PI6_EXTI_Pin GPIO_PIN_6
#define STOCK_PI6_EXTI_GPIO_Port GPIOI
#define STOCK_PI6_EXTI_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
