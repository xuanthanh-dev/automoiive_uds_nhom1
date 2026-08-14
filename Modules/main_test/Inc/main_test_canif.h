#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include <stdint.h>

/* Exported variables --------------------------------------------------------*/

/**
 * @brief CAN1 handle
 */
extern CAN_HandleTypeDef hcan;

/**
 * @brief USART1 handle
 */
extern UART_HandleTypeDef huart1;


/* Exported functions --------------------------------------------------------*/

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void);

/**
 * @brief UART log function
 *
 * @param message Pointer to message string
 */
void uartlog(char *message);

/**
 * @brief Blink LED on PA5
 *
 * Used to indicate MCU startup/reset.
 */
void blink_led(void);

/**
 * @brief Error Handler
 *
 * Called when a HAL initialization or system error occurs.
 */
void Error_Handler(void);

static void SystemTest_RunAll(void);
static void SystemTest_CANIF_Init(void);
static void SystemTest_TX_8Bytes(void);
static void SystemTest_TX_DLC0(void);
static void SystemTest_InvalidDLC(void);
static void SystemTest_IDBoundary(void);
static void SystemTest_RXFrame(void);
static void SystemTest_RXInterrupt(void);
static void SystemTest_TXRX_E2E(void);
static void SystemTest_DataPattern(void);
static void SystemTest_MultipleFrame(void);
static void SystemTest_RXTimeout(void);
static void SystemTest_CANError(void);
static uint8_t SystemTest_WaitForRxCallback(uint32_t timeout);
static void SystemTest_PrintData(uint8_t *data, uint8_t len);

/* Assert function -----------------------------------------------------------*/

#ifdef USE_FULL_ASSERT

/**
 * @brief Reports the source file and line number
 *        where assert_param error occurred.
 *
 * @param file Pointer to source file name
 * @param line Source line number
 */
void assert_failed(uint8_t *file, uint32_t line);

#endif /* USE_FULL_ASSERT */


#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
