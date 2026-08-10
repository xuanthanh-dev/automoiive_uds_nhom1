#include "main.h"
#include "Can_if.h"
#include "STM32_CAN.h"
#include <string.h>

/* ============================================================
 * PRIVATE VARIABLES
 * ============================================================ */

UART_HandleTypeDef huart1;

static uint32_t lastTxTime = 0U;

/* ============================================================
 * PRIVATE FUNCTION PROTOTYPES
 * ============================================================ */

void SystemClock_Config(void);

static void MX_GPIO_Init(void);

static void MX_USART1_UART_Init(void);

static void Blink_LED(int times);

static void UART_Log(const char *message);

static void SendMessage(void);

static void Log_CanError(CanIf_ErrorType error);

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void) {
	CanIf_ErrorType error;

	HAL_Init();


	SystemClock_Config();

	MX_GPIO_Init();


	MX_USART1_UART_Init();


	Blink_LED(3);

	UART_Log("\r\n=== CAN ECU START ===\r\n");


	error = CanIf_Init();

	if (error != CANIF_ERROR_NONE)
	{
		UART_Log("[CAN INIT FAILED]\r\n");

		Log_CanError(error);

		Error_Handler();
	}

	UART_Log("[CAN INIT OK]\r\n");


	while (1) {

		SendMessage();


		HAL_Delay(1000);
	}
}
/* ============================================================
 * SYSTEM CLOCK CONFIGURATION
 * ============================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


    /*
     * Configure HSE and PLL
     */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState =
        RCC_HSE_ON;

    RCC_OscInitStruct.HSEPredivValue =
        RCC_HSE_PREDIV_DIV1;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLMUL =
        RCC_PLL_MUL9;


    if (
        HAL_RCC_OscConfig(
            &RCC_OscInitStruct
        ) != HAL_OK
    )
    {
        Error_Handler();
    }


    /*
     * Configure clocks
     */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;


    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;


    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}
/* ============================================================
* UART1 INITIALIZATION
 * ============================================================ */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance =
        USART1;

    huart1.Init.BaudRate =
        115200;

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


    if (
        HAL_UART_Init(&huart1)
        != HAL_OK
    )
    {
        Error_Handler();
    }
}
/* ============================================================
 * GPIO INITIALIZATION
 * ============================================================ */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct =
        {0};


    /*
     * Enable GPIO clocks
     */

    __HAL_RCC_GPIOD_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();


    /*
     * Initial LED state
     */

    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_5,
        GPIO_PIN_RESET
    );


    /*
     * Configure PA5 as output
     */

    GPIO_InitStruct.Pin =
        GPIO_PIN_5;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_LOW;


    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );
}
/* ============================================================
 * PERIODIC TRANSMISSION
 * ============================================================ */

static void SendMessage(void) {
	uint32_t currentTime;

	CanIf_ErrorType error;

	/*
	 * Get current system time
	 */

	currentTime = HAL_GetTick();

	/*
	 * Check transmission period
	 */

	if ((currentTime - lastTxTime) >= CAN_ENGINE_STATUS_PERIOD_MS) {

		lastTxTime = currentTime;


		error = CanIf_SendEngineStatus();


		if (error != CANIF_ERROR_NONE)
		{
			Log_CanError(error);
		}
		else
		{
			UART_Log("[TX SUCCESS] Engine Status sent\r\n");
		}
	}
}

/* ============================================================
 * CAN ERROR LOGGER
 * ============================================================ */

static void Log_CanError(CanIf_ErrorType error) {
	switch (error) {
	case CANIF_ERROR_NONE:

		UART_Log("[CAN ERROR] NONE\r\n");

		break;

	case CANIF_ERROR_NULL_PDU:

		UART_Log("[CAN ERROR] NULL PDU\r\n");

		break;

	case CANIF_ERROR_INVALID_DLC:

		UART_Log("[CAN ERROR] INVALID DLC\r\n");

		break;

	case CANIF_ERROR_CAN_TX_FAILED:

		UART_Log("[CAN ERROR] CAN TX FAILED\r\n");

		break;

	case CANIF_ERROR_CAN_INIT_FAILED:

		UART_Log("[CAN ERROR] CAN INIT FAILED\r\n");

		break;

	case CANIF_ERROR_CAN_START_FAILED:

		UART_Log("[CAN ERROR] CAN START FAILED\r\n");

		break;

	case CANIF_ERROR_NULL_RX_DATA:

		UART_Log("[CAN ERROR] NULL RX DATA\r\n");

		break;

	case CANIF_ERROR_UNKNOWN_CAN_ID:

		UART_Log("[CAN ERROR] UNKNOWN CAN ID\r\n");

		break;

	case CANIF_ERROR_RX_FAILED:

		UART_Log("[CAN ERROR] RX FAILED\r\n");

		break;

	default:

		UART_Log("[CAN ERROR] UNKNOWN ERROR\r\n");

		break;
	}
}

/* ============================================================
 * UART LOG
 * ============================================================ */

static void UART_Log(const char *message)
{
	HAL_UART_Transmit(&huart1, (uint8_t*) message, strlen(message),HAL_MAX_DELAY);
}

/* ============================================================
 * LED BLINK
 * ============================================================ */

static void Blink_LED(int times)
{
	for (int i = 0; i < times; i++)
	{
		HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5, GPIO_PIN_SET);

		HAL_Delay(1000);

		HAL_GPIO_WritePin(GPIOA,GPIO_PIN_5, GPIO_PIN_RESET);

		HAL_Delay(1000);
	}
}
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {

    }
}
