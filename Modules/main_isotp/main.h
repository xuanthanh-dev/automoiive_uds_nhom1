/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include "stm32f1xx_hal.h"


/* Exported functions prototypes --------------------------------------------*/

void SystemClock_Config(void);

void MX_GPIO_Init(void);

void MX_CAN_Init(void);

void MX_USART1_UART_Init(void);

void Error_Handler(void);


/* USER CODE BEGIN EFP */

/* USER CODE END EFP */


/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */