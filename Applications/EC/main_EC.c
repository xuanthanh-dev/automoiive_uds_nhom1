/**
 * @file    main_EC.c
 * @brief   ECU application entry point.
 *
 * main_EC.c only performs:
 *
 *   - MCU initialisation
 *   - Clock initialisation
 *   - UART initialisation
 *   - CAN initialisation
 *   - AppEngine initialisation
 *   - Cyclic scheduling
 *
 * Application processing is implemented in app_engine.c.
 */

#include "main.h"
#include "app_engine.h"
#include "Can_if.h"
#include <stdio.h>

/* ============================================================
 * GLOBAL PERIPHERAL HANDLES
 * ============================================================ */

CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;

/* ============================================================
 * LOCAL FUNCTIONS
 * ============================================================ */

static void SystemClock_Config(void);

static void MX_USART1_UART_Init(void);

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_USART1_UART_Init();

    /*
     * Initialise CAN interface.
     */
    CAN_IF_Init();

    /*
     * Initialise complete ECU application.
     *
     * This internally initialises:
     *
     *   Engine
     *   DTC manager
     *   UDS
     *   ISO-TP
     */
    AppEngine_EcuInit();

    printf("\r\n");
    printf("========================================\r\n");
    printf("          STM32 DIAGNOSTIC ECU\r\n");
    printf("========================================\r\n");

    printf(
        "[EC] CAN RX ID : 0x%03X\r\n",
        APP_ENGINE_CAN_ID_REQUEST
    );

    printf(
        "[EC] CAN TX ID : 0x%03X\r\n",
        APP_ENGINE_CAN_ID_RESPONSE
    );

    printf("========================================\r\n");

    /*
     * ECU cyclic task.
     */
    while (1)
    {
        AppEngine_MainFunction(
            HAL_GetTick()
        );
    }
}

/* ============================================================
 * SYSTEM CLOCK
 * ============================================================ */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};

    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    osc.HSEState =
        RCC_HSE_ON;

    osc.HSEPredivValue =
        RCC_HSE_PREDIV_DIV1;

    osc.HSIState =
        RCC_HSI_ON;

    osc.PLL.PLLState =
        RCC_PLL_ON;

    osc.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    osc.PLL.PLLMUL =
        RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    clk.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    clk.APB1CLKDivider =
        RCC_HCLK_DIV2;

    clk.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &clk,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * UART1
 * ============================================================ */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance =
        USART1;

    huart1.Init.BaudRate =
        115200U;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * STDOUT -> UART1
 * ============================================================ */

int __io_putchar(int ch)
{
    uint8_t character;

    character = (uint8_t)ch;

    (void)HAL_UART_Transmit(
        &huart1,
        &character,
        1U,
        HAL_MAX_DELAY
    );

    return ch;
}

/* ============================================================
 * ERROR HANDLER
 * ============================================================ */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
