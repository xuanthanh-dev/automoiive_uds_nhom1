/**
 * @file    main_diag.c
 * @brief   Diagnostic Tester ECU application entry point.
 *
 * main_diag.c deliberately contains only MCU/peripheral initialisation and
 * the cyclic call to AppDiag_MainFunction().
 */
#include "main.h"
#include "Can_if.h"
#include "app_diag.h"

#include <stdio.h>

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_USART1_UART_Init(void);

int main(void)
{
    uint8_t rxData;
    uint8_t command;
    HAL_Init();

    SystemClock_Config();

    MX_USART1_UART_Init();

    CAN_IF_Init();

    AppDiag_TesterInit();

    printf("\r\n");
    printf("========================================\r\n");
    printf("        STM32 DIAGNOSTIC TESTER\r\n");
    printf("========================================\r\n");
    Diag_PrintMainMenu();

    while (1)
    {

        if (HAL_UART_Receive(
                &huart1,
                &rxData,
                1,
                10) == HAL_OK)
        {

            if ((rxData == '\r') ||
                (rxData == '\n'))
            {
                continue;
            }
            printf("\r\n");
            printf("Received: %c\r\n", rxData);
            printf("Received HEX: 0x%02X\r\n", rxData);
            command = rxData - '0';

            if ((rxData >= '0') &&
                (rxData <= '9'))
                {
                   command = (uint8_t)(rxData - '0');

                   Diag_HandleCommand(command);
                 }
           else
                 {
                   printf("Invalid command.\r\n");
                 }

        }
        AppDiag_MainFunction();

    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200U;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

int __io_putchar(int ch)
{
    uint8_t character;

    character = (uint8_t)ch;

    (void)HAL_UART_Transmit(
        &huart1,
        &character,
        1U,
        HAL_MAX_DELAY);

    return ch;
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
