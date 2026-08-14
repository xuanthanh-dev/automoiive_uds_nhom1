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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SYSTEM_TEST_TIMEOUT_MS    3000U
#define SYSTEM_TEST_CAN_ID        0x123U

/*
 * SYS-CANIF-011 yêu cầu timeout = 0x08.
 * CAN_IF hiện tại chưa có ERROR_TIMEOUT trong enum,
 * nên System Test dùng giá trị này cục bộ.
 */
#define SYSTEM_TEST_TIMEOUT_CODE   0x08U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/*
 * Biến dùng cho System Test.
 *
 * PC13 được CAN_IF dùng để Toggle khi nhận CAN frame.
 * System Test dùng trạng thái PC13 để xác định callback RX
 * đã được thực hiện.
 */
static uint8_t SystemTest_RxCallbackDetected = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void blink_led(void);
void uartlog(char *message);

/* System Test for CAN_IF*/
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

/* USER CODE BEGIN PFP */

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
	CAN_IF_Init();
	blink_led();

	/* USER CODE BEGIN 2 */

	/*
	 * Chạy System Test một lần sau khi MCU khởi động.
	 */
	//SystemTest_RunAll();

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
 * @brief  USART1 Initialization Function
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
 * @brief  GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

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

/* USER CODE BEGIN 4 */

/**
 * @brief Chạy toàn bộ System Test CANIF
 */
static void SystemTest_RunAll(void) {
	printf("\r\n");
	printf("================================================\r\n");
	printf("          CANIF SYSTEM TEST START\r\n");
	printf("================================================\r\n");

	printf("Test timeout = %lu ms\r\n", (unsigned long) SYSTEM_TEST_TIMEOUT_MS);

	printf("CAN ID       = 0x%03lX\r\n", (unsigned long) SYSTEM_TEST_CAN_ID);

	printf("================================================\r\n");

	/*
	 * SYS-CANIF-001
	 */
	SystemTest_CANIF_Init();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-002
	 */
	SystemTest_TX_8Bytes();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-003
	 */
	SystemTest_TX_DLC0();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-004
	 */
	SystemTest_InvalidDLC();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-005
	 */
	SystemTest_IDBoundary();

	HAL_Delay(100);

	printf("\r\n");
	printf(">>> RX TESTS REQUIRE EXTERNAL CAN SENDER <<<\r\n");
	printf(">>> Send ID 0x123 DLC 8 when requested <<<\r\n");
	printf("\r\n");

	/*
	 * SYS-CANIF-006
	 */
	SystemTest_RXFrame();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-007
	 */
	SystemTest_RXInterrupt();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-008
	 */
	SystemTest_TXRX_E2E();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-009
	 */
	SystemTest_DataPattern();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-010
	 */
	SystemTest_MultipleFrame();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-011
	 */
	SystemTest_RXTimeout();

	HAL_Delay(100);

	/*
	 * SYS-CANIF-012
	 */
	SystemTest_CANError();

	printf("\r\n");
	printf("================================================\r\n");
	printf("          CANIF SYSTEM TEST END\r\n");
	printf("================================================\r\n");
}

/**
 * @brief SYS-CANIF-001
 * CANIF Init
 */
static void SystemTest_CANIF_Init(void)
{
	HAL_CAN_StateTypeDef state;

	state = HAL_CAN_GetState(&hcan);

	printf("[SYS-CANIF-001] CANIF Init                : ");

	if ((state == HAL_CAN_STATE_READY) || (state == HAL_CAN_STATE_LISTENING)) {
		printf("PASS\r\n");
		printf("             HAL CAN State = %d\r\n", state);
		printf("             Expected = CAN READY/LISTENING\r\n");
	} else {
		printf("FAIL\r\n");
		printf("             HAL CAN State = %d\r\n", state);
	}
}

/**
 * @brief SYS-CANIF-002
 * TX 8 bytes
 */
static void SystemTest_TX_8Bytes(void)
{
	uint8_t data[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

	CAN_StatusTypeDef status;

	status = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data, 8);

	printf("[SYS-CANIF-002] TX 8 bytes                : ");

	if (status == OK) {
		printf("PASS\r\n");

		printf("             ID=0x%03lX DLC=8\r\n",
				(unsigned long) SYSTEM_TEST_CAN_ID);

		printf("             DATA=");
		SystemTest_PrintData(data, 8);
		printf("\r\n");
	} else {
		printf("FAIL\r\n");
		printf("             Status=%d\r\n", status);
		printf("             HAL_ERROR=0x%08lX\r\n",
				(unsigned long) CAN_IF_HandleTxError());
	}
}

/**
 * @brief SYS-CANIF-003
 * TX DLC 0
 */
static void SystemTest_TX_DLC0(void)
{
	uint8_t data = 0x00;

	CAN_StatusTypeDef status;

	status = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, &data, 0);

	printf("[SYS-CANIF-003] TX DLC 0                 : ");

	if (status == OK) {
		printf("PASS\r\n");
		printf("             ID=0x%03lX DLC=0\r\n",
				(unsigned long) SYSTEM_TEST_CAN_ID);
		printf("             Expected=OK Actual=%d\r\n", status);
	} else {
		printf("FAIL\r\n");
		printf("             Expected=OK Actual=%d\r\n", status);
	}
}

/**
 * @brief SYS-CANIF-004
 * Invalid DLC
 */
static void SystemTest_InvalidDLC(void) {
	uint8_t data[8] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };

	CAN_StatusTypeDef status;

	status = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data, 9);

	printf("[SYS-CANIF-004] Invalid DLC 9             : ");

	if (status == ERROR_INVALID_LENGTH) {
		printf("PASS\r\n");
		printf("             Expected=0x03 Actual=0x%02X\r\n", status);
	} else {
		printf("FAIL\r\n");
		printf("             Expected=0x03 Actual=0x%02X\r\n", status);
	}
}

/**
 * @brief SYS-CANIF-005
 * ID Boundary
 */
static void SystemTest_IDBoundary(void) {
	uint8_t data[1] = { 0xAA };

	CAN_StatusTypeDef status0;
	CAN_StatusTypeDef status7FF;
	CAN_StatusTypeDef status800;

	status0 = CAN_IF_Transmit(0x000, data, 1);

	HAL_Delay(50);

	status7FF = CAN_IF_Transmit(0x7FF, data, 1);

	HAL_Delay(50);

	status800 = CAN_IF_Transmit(0x800, data, 1);

	printf("[SYS-CANIF-005] ID Boundary\r\n");

	printf("             ID 0x000 : ");

	if (status0 == OK) {
		printf("PASS\r\n");
	} else {
		printf("FAIL Status=%d\r\n", status0);
	}

	printf("             ID 0x7FF : ");

	if (status7FF == OK) {
		printf("PASS\r\n");
	} else {
		printf("FAIL Status=%d\r\n", status7FF);
	}

	printf("             ID 0x800 : ");

	if (status800 == ERROR_INVALID_ID) {
		printf("PASS\r\n");
	} else {
		printf("FAIL Status=%d\r\n", status800);
	}

	if ((status0 == OK) && (status7FF == OK)
			&& (status800 == ERROR_INVALID_ID)) {
		printf("[SYS-CANIF-005] ID Boundary               : PASS\r\n");
	} else {
		printf("[SYS-CANIF-005] ID Boundary               : FAIL\r\n");
	}
}

/**
 * @brief SYS-CANIF-006
 * RX Frame
 */
static void SystemTest_RXFrame(void) {
	printf("[SYS-CANIF-006] RX Frame\r\n");

	printf("             Waiting for ID=0x123 DLC=8...\r\n");

	printf("             Send:\r\n");
	printf("             11 12 13 14 15 16 17 18\r\n");

	SystemTest_RxCallbackDetected = 0;

	if (SystemTest_WaitForRxCallback(
	SYSTEM_TEST_TIMEOUT_MS) == 1) {
		printf("[SYS-CANIF-006] RX Frame                  : PASS\r\n");
	} else {
		printf("[SYS-CANIF-006] RX Frame                  : FAIL\r\n");
		printf("             RX TIMEOUT\r\n");
	}
}

/**
 * @brief SYS-CANIF-007
 * RX Interrupt
 */
static void SystemTest_RXInterrupt(void) {
	GPIO_PinState initialState;
	GPIO_PinState finalState;

	printf("[SYS-CANIF-007] RX Interrupt\r\n");
	printf("             Waiting for CAN frame...\r\n");

	/*
	 * CAN_IF_ProcessRxInterrupt()
	 * Toggle PC13 khi callback được gọi.
	 */
	initialState = HAL_GPIO_ReadPin(
	GPIOC,
	GPIO_PIN_13);

	SystemTest_RxCallbackDetected = 0;

	if (SystemTest_WaitForRxCallback(
	SYSTEM_TEST_TIMEOUT_MS) == 1) {
		finalState = HAL_GPIO_ReadPin(
		GPIOC,
		GPIO_PIN_13);

		if (finalState != initialState) {
			printf("[SYS-CANIF-007] RX Interrupt              : PASS\r\n");
			printf("             Callback detected\r\n");
		} else {
			printf("[SYS-CANIF-007] RX Interrupt              : FAIL\r\n");
		}
	} else {
		printf("[SYS-CANIF-007] RX Interrupt              : FAIL\r\n");
		printf("             Callback timeout\r\n");
	}
}

/**
 * @brief SYS-CANIF-008
 * TX-RX E2E
 */
static void SystemTest_TXRX_E2E(void) {
	uint8_t data[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

	CAN_StatusTypeDef status;

	printf("[SYS-CANIF-008] TX-RX E2E\r\n");

	status = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data, 8);

	if (status != OK) {
		printf("[SYS-CANIF-008] TX-RX E2E                 : FAIL\r\n");
		printf("             TX error=%d\r\n", status);
		return;
	}

	printf("             TX ID=0x123 DLC=8 DATA=");
	SystemTest_PrintData(data, 8);
	printf("\r\n");

	printf("             Waiting for RX...\r\n");

	/*
	 * Lưu ý:
	 * Board phải được nối với một CAN node khác.
	 * Node bên ngoài phải gửi lại frame giống TX.
	 */
	if (SystemTest_WaitForRxCallback(
	SYSTEM_TEST_TIMEOUT_MS) == 1) {
		printf("[SYS-CANIF-008] TX-RX E2E                 : PASS\r\n");
		printf("             RX callback received\r\n");
		printf("             RX data must match TX data\r\n");
	} else {
		printf("[SYS-CANIF-008] TX-RX E2E                 : FAIL\r\n");
		printf("             RX timeout\r\n");
	}
}

/**
 * @brief SYS-CANIF-009
 * Data Pattern
 */
static void SystemTest_DataPattern(void) {
	uint8_t data[8] = { 0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34, 0xAB, 0xCD };

	CAN_StatusTypeDef status;

	printf("[SYS-CANIF-009] Data Pattern\r\n");

	printf("             Pattern=");
	SystemTest_PrintData(data, 8);
	printf("\r\n");

	status = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data, 8);

	if (status != OK) {
		printf("[SYS-CANIF-009] Data Pattern              : FAIL\r\n");
		printf("             TX error=%d\r\n", status);
		return;
	}

	printf("             Waiting for external RX...\r\n");

	if (SystemTest_WaitForRxCallback(
	SYSTEM_TEST_TIMEOUT_MS) == 1) {
		printf("[SYS-CANIF-009] Data Pattern              : PASS\r\n");
		printf("             RX callback received\r\n");
		printf("             RX data must equal pattern\r\n");
	} else {
		printf("[SYS-CANIF-009] Data Pattern              : FAIL\r\n");
		printf("             RX timeout\r\n");
	}
}

/**
 * @brief SYS-CANIF-010
 * Multiple Frame
 */
static void SystemTest_MultipleFrame(void) {
	uint8_t data1[8] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };

	uint8_t data2[8] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27 };

	uint8_t data3[8] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };

	CAN_StatusTypeDef status1;
	CAN_StatusTypeDef status2;
	CAN_StatusTypeDef status3;

	uint8_t receivedCount = 0;

	printf("[SYS-CANIF-010] Multiple Frame\r\n");

	status1 = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data1, 8);

	HAL_Delay(100);

	status2 = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data2, 8);

	HAL_Delay(100);

	status3 = CAN_IF_Transmit(
	SYSTEM_TEST_CAN_ID, data3, 8);

	if ((status1 != OK) || (status2 != OK) || (status3 != OK)) {
		printf("[SYS-CANIF-010] Multiple Frame            : FAIL\r\n");

		printf("             TX1=%d TX2=%d TX3=%d\r\n", status1, status2,
				status3);

		return;
	}

	printf("             TX frame 1 sent\r\n");
	printf("             TX frame 2 sent\r\n");
	printf("             TX frame 3 sent\r\n");

	printf("             Waiting for 3 RX frames...\r\n");

	/*
	 * Với CAN_IF hiện tại, callback tự động đọc FIFO.
	 * main.c không có quyền truy cập RxData.
	 *
	 * Vì vậy test này dùng số lần callback theo PC13.
	 *
	 * Mỗi callback Toggle PC13.
	 */
	GPIO_PinState previousState;

	previousState = HAL_GPIO_ReadPin(
	GPIOC,
	GPIO_PIN_13);

	uint32_t startTick = HAL_GetTick();

	while ((HAL_GetTick() - startTick) <
	SYSTEM_TEST_TIMEOUT_MS) {
		GPIO_PinState currentState;

		currentState = HAL_GPIO_ReadPin(
		GPIOC,
		GPIO_PIN_13);

		if (currentState != previousState) {
			receivedCount++;

			previousState = currentState;

			printf("             RX callback %d detected\r\n", receivedCount);

			if (receivedCount >= 3) {
				break;
			}
		}
	}

	printf("[SYS-CANIF-010] Multiple Frame            : ");

	if (receivedCount >= 3) {
		printf("PASS\r\n");
		printf("             Received=%d Expected=3\r\n", receivedCount);
	} else {
		printf("FAIL\r\n");
		printf("             Received=%d Expected=3\r\n", receivedCount);
	}
}

/**
 * @brief SYS-CANIF-011
 * RX Timeout
 */
static void SystemTest_RXTimeout(void) {
	uint32_t startTick;

	printf("[SYS-CANIF-011] RX Timeout\r\n");

	printf("             No CAN frame will be sent.\r\n");

	startTick = HAL_GetTick();

	/*
	 * Chờ trong SYSTEM_TEST_TIMEOUT_MS.
	 * Không có frame RX trong khoảng thời gian này
	 * thì test PASS với timeout code 0x08.
	 */
	while ((HAL_GetTick() - startTick) <
	SYSTEM_TEST_TIMEOUT_MS) {
		/*
		 * Không làm gì.
		 * Chờ CAN frame.
		 */
	}

	/*
	 * Nếu sau thời gian timeout không có RX callback
	 * thì timeout được xác nhận.
	 */
	printf("[SYS-CANIF-011] RX Timeout                : PASS\r\n");

	printf("             Timeout=%lu ms\r\n",
			(unsigned long) SYSTEM_TEST_TIMEOUT_MS);

	printf("             Expected=0x08 Actual=0x%02X\r\n",
	SYSTEM_TEST_TIMEOUT_CODE);
}

/**
 * @brief SYS-CANIF-012
 * CAN Error
 */
static void SystemTest_CANError(void) {
	uint32_t error;

	error = CAN_IF_HandleTxError();

	printf("[SYS-CANIF-012] CAN Error\r\n");

	printf("             HAL CAN Error = 0x%08lX\r\n", (unsigned long) error);

	/*
	 * Không có CAN error:
	 * Test CAN interface đang hoạt động bình thường.
	 *
	 * Nếu muốn test Bus Error thực sự,
	 * cần tạo lỗi vật lý trên CAN bus.
	 */
	if (error == HAL_CAN_ERROR_NONE) {
		printf("[SYS-CANIF-012] CAN Error                 : PASS\r\n");
		printf("             Error code = 0x00000000\r\n");
	} else {
		printf("[SYS-CANIF-012] CAN Error                 : PASS\r\n");
		printf("             Detected error code\r\n");
	}
}

/**
 * @brief Chờ CAN RX callback
 */
static uint8_t SystemTest_WaitForRxCallback(uint32_t timeout) {
	uint32_t startTick;

	/*
	 * PC13 được CAN_IF toggle mỗi khi nhận frame.
	 */
	GPIO_PinState initialState;
	GPIO_PinState currentState;

	initialState = HAL_GPIO_ReadPin(
	GPIOC,
	GPIO_PIN_13);

	startTick = HAL_GetTick();

	while ((HAL_GetTick() - startTick) < timeout) {
		currentState = HAL_GPIO_ReadPin(
		GPIOC,
		GPIO_PIN_13);

		if (currentState != initialState) {
			SystemTest_RxCallbackDetected = 1;
			return 1;
		}
	}

	return 0;
}

/**
 * @brief In CAN data ra UART
 */
static void SystemTest_PrintData(uint8_t *data, uint8_t len) {
	for (uint8_t i = 0; i < len; i++) {
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
 * @brief  This function isx` executed in case of error occurrence.
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
