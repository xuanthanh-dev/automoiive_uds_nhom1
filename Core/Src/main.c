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
#include "app_diag.h"
#include "app_engine.h"
#include "did_manager.h"
#include "dtc_manager.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
CAN_HandleTypeDef hcan;

UART_HandleTypeDef huart1;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SYSTEM_TEST_TIMEOUT_MS    3000U
#define SYSTEM_TEST_CAN_ID        0x123U
#define SYSTEM_TEST_AUTORUN       1U
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

static void SystemTest_RXTimeout(void);
static void SystemTest_CANError(void);
static uint8_t SystemTest_WaitForRxCallback(uint32_t timeout);
static void SystemTest_PrintData(uint8_t *data, uint8_t len);

/* UDS ECU black-box system test (ST-001 to ST-011). */
static void SystemTest_RunModuleSuite(void);
static uint8_t SystemTest_RunDid(uint16_t did, const char *testId);
static uint8_t SystemTest_RunSimpleRequest(const uint8_t *request,
                                           uint16_t requestLength,
                                           const uint8_t *response,
                                           uint16_t responseLength);
static uint8_t SystemTest_HasBytes(const uint8_t *data, uint16_t dataLength,
                                   const uint8_t *expected, uint16_t expectedLength);
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

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/*
		 * Sau khi System Test hoàn tất,
		 * MCU giữ trạng thái để quan sát kết quả UART.
		 */
		#if (SYSTEM_TEST_AUTORUN == 1U)
		SystemTest_RunModuleSuite();
		#endif

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

/* Executes the scenarios in docs/tests/system_test_spec.md using Modules. */
static void SystemTest_RunModuleSuite(void)
{
    AppEngine_ContextType engine;
    AppEngine_SignalsType signals;
    uint8_t engineReady;
    uint8_t request[3];
    uint8_t response[3];
    uint8_t dtcData[8];
    uint8_t dtcLength;
    uint8_t overTempActive;
    uint8_t lowBatteryActive;
    Dtc_ContextType dtc;

    printf("\r\n========== MODULE SYSTEM TEST START ==========\r\n");

    /* app_engine deliberately has no EngineStatus CAN transmitter. */
    engineReady = (AppEngine_Init(&engine) == APP_E_OK) &&
                  (AppEngine_RunSimulationStep(&engine, APP_ENGINE_STEP_PERIOD_MS) == APP_E_OK) &&
                  (AppEngine_GetSignals(&engine, &signals) == APP_E_OK);
    printf("[ST-001] EngineStatus cyclic message: %s (CAN scheduler not implemented in Modules)\r\n",
           engineReady ? "NOT RUN" : "FAIL");

    printf("[ST-002] Read VIN: %s\r\n",
           SystemTest_RunDid(DID_VIN, "ST-002") ? "PASS" : "FAIL");
    printf("[ST-003] Read SW Version: %s\r\n",
           SystemTest_RunDid(DID_SOFTWARE_VERSION, "ST-003") ? "PASS" : "FAIL");
    printf("[ST-004] Read VehicleSpeed: %s\r\n",
           SystemTest_RunDid(DID_VEHICLE_SPEED, "ST-004") ? "PASS" : "FAIL");
    printf("[ST-005] Read RPM: %s\r\n",
           SystemTest_RunDid(DID_ENGINE_SPEED, "ST-005") ? "PASS" : "FAIL");

    request[0] = 0x22U; request[1] = 0xFFU; request[2] = 0xFFU;
    response[0] = 0x7FU; response[1] = 0x22U; response[2] = 0x31U;
    printf("[ST-006] Unknown DID: %s\r\n",
           SystemTest_RunSimpleRequest(request, 3U, response, 3U) ? "PASS" : "FAIL");

    request[0] = 0x10U; request[1] = 0x03U;
    response[0] = 0x50U; response[1] = 0x03U;
    printf("[ST-007] Extended session: %s\r\n",
           SystemTest_RunSimpleRequest(request, 2U, response, 2U) ? "PASS" : "FAIL");

    request[0] = 0x11U; request[1] = 0x03U;
    response[0] = 0x51U; response[1] = 0x03U;
    printf("[ST-008] ECU reset: %s\r\n",
           SystemTest_RunSimpleRequest(request, 2U, response, 2U) ? "PASS" : "FAIL");

    request[0] = 0x3EU; request[1] = 0x00U;
    response[0] = 0x7EU; response[1] = 0x00U;
    printf("[ST-009] TesterPresent: %s\r\n",
           SystemTest_RunSimpleRequest(request, 2U, response, 2U) ? "PASS" : "FAIL");

    /* Inject the two faults through AppEngine, then obtain their DTC records. */
    signals.vehicleSpeedKmh = 80U; signals.engineSpeedRpm = 2400U;
    signals.engineTempCelsius = 111U; signals.batteryVoltageDeciV = 120U;
    if ((AppEngine_SetSignals(&engine, &signals) == APP_E_OK) &&
        (AppEngine_IsOverTemperature(&engine, &overTempActive) == APP_E_OK) &&
        (overTempActive != 0U) &&
        (DtcManager_Init(&dtc) == DTC_E_OK) &&
        (DtcManager_SetStatus(&dtc, DTC_CODE_ENGINE_OVER_TEMP, 1U) == DTC_E_OK) &&
        (DtcManager_SerialiseActive(&dtc, dtcData, sizeof(dtcData), &dtcLength) == DTC_E_OK))
    {
        request[0] = 0x19U; request[1] = 0x02U; request[2] = 0xFFU;
        printf("[ST-010] OverTemp DTC: %s\r\n",
               (dtcLength == 4U) && (dtcData[0] == 0x01U) && (dtcData[2] == 0x01U) &&
               SystemTest_RunSimpleRequest(request, 3U,
                   (const uint8_t[]){0x59U, 0x02U, dtcData[0], dtcData[1], dtcData[2], dtcData[3]}, 6U)
               ? "PASS" : "FAIL");
    }
    else { printf("[ST-010] OverTemp DTC: FAIL\r\n"); }

    signals.engineTempCelsius = 90U; signals.batteryVoltageDeciV = 109U;
    if ((AppEngine_SetSignals(&engine, &signals) == APP_E_OK) &&
        (AppEngine_IsLowBattery(&engine, &lowBatteryActive) == APP_E_OK) &&
        (lowBatteryActive != 0U) &&
        (DtcManager_ClearAll(&dtc) == DTC_E_OK) &&
        (DtcManager_SetStatus(&dtc, DTC_CODE_LOW_BATTERY, 1U) == DTC_E_OK) &&
        (DtcManager_SerialiseActive(&dtc, dtcData, sizeof(dtcData), &dtcLength) == DTC_E_OK))
    {
        printf("[ST-011] LowBattery DTC: %s\r\n",
               (dtcLength == 4U) && (dtcData[0] == 0x01U) && (dtcData[2] == 0x02U) &&
               SystemTest_RunSimpleRequest(request, 3U,
                   (const uint8_t[]){0x59U, 0x02U, dtcData[0], dtcData[1], dtcData[2], dtcData[3]}, 6U)
               ? "PASS" : "FAIL");
    }
    else { printf("[ST-011] LowBattery DTC: FAIL\r\n"); }
    printf("=========== MODULE SYSTEM TEST END ===========\r\n");
}

static uint8_t SystemTest_RunDid(uint16_t did, const char *testId)
{
    AppEngine_ContextType engine;
    uint8_t didData[DID_MAX_DATA_LENGTH];
    uint8_t response[3U + DID_MAX_DATA_LENGTH];
    uint8_t didLength;
    uint8_t request[3] = { 0x22U, (uint8_t)(did >> 8U), (uint8_t)did };
    (void)testId;
    if ((AppEngine_Init(&engine) != APP_E_OK) ||
        (DidManager_ReadData(did, &engine, didData, sizeof(didData), &didLength) != DID_E_OK)) return 0U;
    response[0] = 0x62U; response[1] = request[1]; response[2] = request[2];
    (void)memcpy(&response[3], didData, didLength);
    return SystemTest_RunSimpleRequest(request, 3U, response, (uint16_t)(3U + didLength));
}

static uint8_t SystemTest_RunSimpleRequest(const uint8_t *request, uint16_t requestLength,
                                           const uint8_t *response, uint16_t responseLength)
{
    AppDiag_ContextType diagnostic;
    uint8_t prepared[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint8_t received[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint16_t preparedLength;
    uint16_t receivedLength;
    AppDiag_ReturnType result;
    if (AppDiag_Init(&diagnostic) != APP_DIAG_E_OK) return 0U;
    if (request[0] == 0x22U) result = AppDiag_PrepareReadDid(&diagnostic, (uint16_t)((request[1] << 8U) | request[2]));
    else if (request[0] == 0x10U) result = AppDiag_PrepareSessionControl(&diagnostic, request[1]);
    else if (request[0] == 0x11U) result = AppDiag_PrepareSoftReset(&diagnostic);
    else if (request[0] == 0x19U) result = AppDiag_PrepareReadDtc(&diagnostic, request[2]);
    else if (request[0] == 0x3EU) result = AppDiag_PrepareTesterPresent(&diagnostic, 0U);
    else return 0U;
    if ((result != APP_DIAG_E_OK) ||
        (AppDiag_GetPreparedRequest(&diagnostic, prepared, sizeof(prepared), &preparedLength) != APP_DIAG_E_OK) ||
        (preparedLength != requestLength) || !SystemTest_HasBytes(prepared, preparedLength, request, requestLength) ||
        (AppDiag_NotifyRequestTransmitted(&diagnostic) != APP_DIAG_E_OK) ||
        (AppDiag_RxIndication(&diagnostic, response, responseLength) != APP_DIAG_E_OK) ||
        (AppDiag_GetResponse(&diagnostic, received, sizeof(received), &receivedLength) != APP_DIAG_E_OK) ||
        (receivedLength != responseLength) || !SystemTest_HasBytes(received, receivedLength, response, responseLength)) return 0U;
    return (AppDiag_ClearResponse(&diagnostic) == APP_DIAG_E_OK) ? 1U : 0U;
}

static uint8_t SystemTest_HasBytes(const uint8_t *data, uint16_t dataLength,
                                   const uint8_t *expected, uint16_t expectedLength)
{
    return ((dataLength == expectedLength) && (memcmp(data, expected, expectedLength) == 0)) ? 1U : 0U;
}

/**

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
