#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* External peripheral handles */
extern CAN_HandleTypeDef hcan;
extern HAL_StatusTypeDef status;
extern UART_HandleTypeDef huart1;

/* System / peripheral initialization */
void SystemClock_Config(void);
void MX_CAN_Init(void);

/* Application helpers */
void uartlog(char *message);
void blink_led(void);
void Error_Handler(void);

/* ISO-TP test functions */
void Test_ISOTP_SingleFrame(void);
void Test_ISOTP_FirstFrame(void);
void Test_ISOTP_ConsecutiveFrame(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
