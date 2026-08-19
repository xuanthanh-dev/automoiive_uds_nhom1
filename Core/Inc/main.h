#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* =========================================================
 * STM32 PERIPHERALS
 * ========================================================= */


extern UART_HandleTypeDef huart1;


/* =========================================================
 * FUNCTION PROTOTYPES
 * ========================================================= */

void SystemClock_Config(void);

void MX_GPIO_Init(void);

void MX_USART1_UART_Init(void);

void Error_Handler(void);


/* =========================================================
 * ISO-TP UNIT TEST
 * ========================================================= */


/* =========================================================
 * UART printf
 * ========================================================= */

int _write(int file, char *ptr, int len);


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
