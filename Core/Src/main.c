/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "Can_if.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define Unit_TEST_TIMEOUT_MS    3000U
#define Unit_TEST_CAN_ID        0x123U
#define Unit_TEST_TIMEOUT_CODE   0x08U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/*
 * Biến dùng cho Timeout.

 * Timeout_Counter:
 *     Bộ đếm thời gian hiện tại, đơn vị ms.
 *
 * Timeout_Target:
 *     Giá trị timeout cần đạt, đơn vị ms.
 */
static volatile uint8_t Timeout_Flag = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
void blink_led(void);
/* USER CODE BEGIN PFP */

/*
 * Timeout API
 */
static void Timeout_Set(uint32_t timeoutMs);

/*
 * Unit Test CAN_IF
 */
static void UnitTest_RunAll(void);
static void UnitTest_CANIF_Init(void);
static void UnitTest_CANIF_Transmit(void);
static void UnitTest_PrintData(uint8_t *data, uint8_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	MX_TIM1_Init();

	/* USER CODE BEGIN 2 */

	blink_led();

	UnitTest_RunAll();

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/*
		 * Sau khi System Test hoàn tất,
		 * MCU giữ trạng thái để quan sát kết quả UART.
		 */
		HAL_Delay(1000);

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;

	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}


/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */

	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 71;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 999;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

	if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}

	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}

	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */

	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;

	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}

	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/*
	 * Không khai báo lại GPIO_InitStruct ở đây.
	 * GPIO_InitStruct đã được CubeMX khai báo ở phía trên.
	 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

	/*Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // LED OFF

	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // Khởi tạo mức 0 cho PA5

	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;         // Output Push-Pull
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* USER CODE END MX_GPIO_Init_2 */
}

/**
 * @brief Thiết lập Timeout bằng TIM1
 *
 * Mỗi lần gọi hàm sẽ reset TIM1 và bắt đầu
 * một khoảng Timeout mới.
 *
 * @param timeoutMs Thời gian Timeout, đơn vị ms
 * @retval None
 */
static void Timeout_Set(uint32_t timeoutMs) {
	/*
	 * Dừng TIM1 trước khi thiết lập Timeout mới.
	 */
	HAL_TIM_Base_Stop_IT(&htim1);

	/*
	 * Reset cờ Timeout.
	 */
	Timeout_Flag = 0U;

	/*
	 * Reset counter của TIM1.
	 */
	__HAL_TIM_SET_COUNTER(&htim1, 0U);

	/*
	 * Với TIM1:
	 *
	 * Clock = 72 MHz
	 * Prescaler = 71
	 *
	 * => Timer clock = 1 MHz
	 * => 1 tick = 1 us
	 *
	 * timeoutMs * 1000 = số tick cần đếm.
	 */
	__HAL_TIM_SET_AUTORELOAD(&htim1, (timeoutMs * 1000U) - 1U);

	/*
	 * Bắt đầu TIM1 và cho phép interrupt.
	 */
	HAL_TIM_Base_Start_IT(&htim1);
}
/**
 * @brief Timer Period Elapsed Callback
 *
 * Được gọi khi TIM1 đạt thời gian Timeout.
 *
 * @param htim Timer handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
		/*
		 * Báo Timeout đã xảy ra.
		 */
		Timeout_Flag = 1U;

		/*
		 * Dừng TIM1 để Timeout chỉ tạo
		 * một interrupt cho mỗi lần Set.
		 */
		HAL_TIM_Base_Stop_IT(&htim1);
	}
}

/**
 * @brief Chạy toàn bộ Unit Test CAN_IF
 */
static void UnitTest_RunAll(void) {
	printf("\r\n");
	printf("================================================\r\n");
	printf("            CANIF UNIT TEST START\r\n");
	printf("================================================\r\n");

	/*
	 * SYS-CANIF-001
	 */
	UnitTest_CANIF_Init();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-002
	 */
	UnitTest_CANIF_Transmit();

	printf("\r\n");
	printf("================================================\r\n");
	printf("             CANIF UNIT TEST END\r\n");
	printf("================================================\r\n");
}

/**
 * @brief SYS-CANIF-001
 * CAN_IF Initialization
 */
static void UnitTest_CANIF_Init(void) {
	HAL_CAN_StateTypeDef state;

	printf("\r\n");
	printf("[SYS-CANIF-001] CAN_IF_Init\r\n");
	printf("             Initializing CAN...\r\n");

	/*
	 * CAN_IF tự thực hiện CAN initialization.
	 */
	CAN_IF_Init();

	HAL_Delay(10);

	state = HAL_CAN_GetState(&hcan);

	printf("             HAL CAN State = %d\r\n", state);

	if ((state == HAL_CAN_STATE_READY) || (state == HAL_CAN_STATE_LISTENING)) {
		printf("[SYS-CANIF-001] CAN_IF_Init               : PASS\r\n");
		printf("             Expected = READY/LISTENING\r\n");
	} else {
		printf("[SYS-CANIF-001] CAN_IF_Init               : FAIL\r\n");
		printf("             Expected = READY/LISTENING\r\n");
	}

	/*
	 * DeInit sau khi test hoàn thành.
	 *
	 * Test tiếp theo sẽ Init lại CAN từ trạng thái ban đầu.
	 */
	CAN_IF_DeInit();
}

/**
 * @brief SYS-CANIF-002
 * CAN_IF Transmit 8 bytes
 */
static void UnitTest_CANIF_Transmit(void) {
	uint8_t data[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

	CAN_StatusTypeDef status;

	printf("\r\n");
	printf("[SYS-CANIF-002] CAN_IF_Transmit\r\n");

	/*
	 * Mỗi test tự Init CAN.
	 */
	CAN_IF_Init();

	HAL_Delay(10);

	printf("             ID   = 0x%03lX\r\n", (unsigned long) Unit_TEST_CAN_ID);

	printf("             DLC  = 8\r\n");

	printf("             DATA = ");

	UnitTest_PrintData(data, 8);

	printf("\r\n");

	/*
	 * Thực hiện truyền CAN.
	 */
	status = CAN_IF_Transmit(
	Unit_TEST_CAN_ID, data, 8);

	if (status == OK) {
		printf("[SYS-CANIF-002] CAN_IF_Transmit           : PASS\r\n");
		printf("             Expected = OK\r\n");
		printf("             Actual   = OK\r\n");
	} else {
		printf("[SYS-CANIF-002] CAN_IF_Transmit           : FAIL\r\n");
		printf("             Expected = OK\r\n");
		printf("             Actual   = %d\r\n", status);

		printf("             HAL Error = 0x%08lX\r\n",
				(unsigned long) CAN_IF_HandleTxError());
	}

	/*
	 * DeInit sau khi test hoàn thành.
	 */
	CAN_IF_DeInit();
}

/**
 * @brief In CAN data ra UART
 */
static void UnitTest_PrintData(uint8_t *data, uint8_t len) {
	for (uint8_t i = 0U; i < len; i++) {
		printf("%02X ", data[i]);
	}
}

#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
	HAL_UART_Transmit(&huart1, (uint8_t*) &ch, 1,
	HAL_MAX_DELAY);

	return ch;
}

void uartlog(char *message) {
	HAL_UART_Transmit(&huart1, (uint8_t*) message, strlen(message),
	HAL_MAX_DELAY);
}

void blink_led(void) {
	HAL_GPIO_WritePin(
	GPIOA,
	GPIO_PIN_5, GPIO_PIN_SET);   // Bật LED PA5 báo hiệu reset

	HAL_Delay(2000);                           // Giữ sáng 2 giây để dễ quan sát

	HAL_GPIO_WritePin(
	GPIOA,
	GPIO_PIN_5, GPIO_PIN_RESET); // Tắt LED PA5
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();

	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and the source line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
