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
#include "isotp.h"

/* Private typedef -----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/


/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;
HAL_StatusTypeDef status;
UART_HandleTypeDef huart1;



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);
static void blink_led(void);
void uartlog(char *message);

static uint8_t isoTpTestFrame[8];
static uint8_t isoTpTestDlc;
static uint32_t isoTpTxFrameCount = 0;
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{


  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();


  /* Configure the system clock */
  SystemClock_Config();


  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  CAN_IF_Init();
  MX_USART1_UART_Init();
  blink_led();
  /* USER CODE BEGIN 2 */
//  uint8_t txData[8] = {0x03, 0x22, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
  /* USER CODE END 2 */
  uint8_t txData[8] =
  	      {
  	          0x01, 0x02, 0x03, 0x04,
  	          0x05, 0x06, 0x07, 0x08
  	      };

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // TEST CAN
	  printf("\r\n========== TEST CAN ==========\r\n");
	  CAN_IF_Transmit(0x123, txData, 8);

      HAL_Delay(2000);



	    //test cho isotp

	    uint32_t currentTimeMs;
	    /*========================================
	       * TEST 1: SINGLE FRAME
	       *========================================*/

	      Test_ISOTP_SingleFrame();

	      HAL_Delay(2000);


	      /*========================================
	       * TEST 2: FIRST FRAME
	       *========================================*/

	      Test_ISOTP_FirstFrame();

	      HAL_Delay(2000);


	      /*========================================
	       * TEST 3: CONSECUTIVE FRAME
	       *========================================*/

	      Test_ISOTP_ConsecutiveFrame();


	      HAL_Delay(5000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

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
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
	 GPIO_InitTypeDef GPIO_InitStruct = {0};
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
#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
void uartlog(char *message)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
}
void blink_led(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);   // Bật LED PA5 báo hiệu reset
	HAL_Delay(2000);                                      // Giữ sáng 2 giây để dễ quan sát
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // Tắt LED PA5
}
/* USER CODE END 4 */

/**
  * @brief  This function isx` executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

uint8_t IsoTp_Test_CanSend(const uint8_t *frame, uint8_t dlc)
{
    isoTpTestDlc = dlc;

    for (uint8_t i = 0; i < dlc; i++)
    {
        isoTpTestFrame[i] = frame[i];
    }

    isoTpTxFrameCount++;

    printf("\r\nISO-TP TX FRAME %lu: ",
           isoTpTxFrameCount);

    for (uint8_t i = 0; i < dlc; i++)
    {
        printf("%02X ", frame[i]);
    }

    printf("\r\n");

    /*
     * Nếu muốn frame thực sự đi ra CAN BUS:
     *
     * CAN_IF_Transmit(CAN_ID, (uint8_t *)frame, dlc);
     *
     * Ví dụ:
     */

    if (CAN_IF_Transmit(0x123,
                        (uint8_t *)frame,
                        dlc) == HAL_OK)
    {
        return 0;
    }

    return 1;
}


void Test_ISOTP_SingleFrame(void)
{
    uint8_t data[7] =
    {
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07
    };

    IsoTp_StatusType status;

    isoTpTxFrameCount = 0;

    printf("\r\n");
    printf("====================================\r\n");
    printf("TC_ISOTP_SF_001\r\n");
    printf("Single Frame Test\r\n");
    printf("====================================\r\n");

    status = IsoTp_Send(
        data,
        7,
        HAL_GetTick()
    );

    if (status != OK)
    {
        printf("TEST FAIL: IsoTp_Send error\r\n");
        return;
    }

    /*
     * Single Frame phải chỉ tạo 1 CAN frame
     */

    if (isoTpTxFrameCount != 1)
    {
        printf("TEST FAIL: Frame count = %lu\r\n",
               isoTpTxFrameCount);
        return;
    }

    /*
     * Với 7 byte:
     *
     * Byte 0 = 0x07
     *
     * 0 = Single Frame
     * 7 = Payload length
     */

    if (isoTpTestFrame[0] != 0x07)
    {
        printf("TEST FAIL: Wrong SF PCI = 0x%02X\r\n",
               isoTpTestFrame[0]);
        return;
    }

    /*
     * Kiểm tra payload
     */

    if (memcmp(&isoTpTestFrame[1],
               data,
               7) != 0)
    {
        printf("TEST FAIL: Payload mismatch\r\n");
        return;
    }

    printf("TC_ISOTP_SF_001: PASS\r\n");
}

void Test_ISOTP_FirstFrame(void)
{
    uint8_t data[16] =
    {
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10
    };

    IsoTp_StatusType status;

    isoTpTxFrameCount = 0;

    printf("\r\n");
    printf("====================================\r\n");
    printf("TC_ISOTP_FF_001\r\n");
    printf("First Frame Test\r\n");
    printf("====================================\r\n");

    status = IsoTp_Send(
        data,
        16,
        HAL_GetTick()
    );

    if (status != OK)
    {
        printf("TEST FAIL: IsoTp_Send error\r\n");
        return;
    }

    /*
     * Byte đầu phải có:
     *
     * 0x10
     *
     * High nibble = 1 -> First Frame
     */

    if ((isoTpTestFrame[0] & 0xF0) != 0x10)
    {
        printf("TEST FAIL: Not First Frame\r\n");
        return;
    }

    printf("First Frame detected\r\n");

    printf("FF DATA: ");

    for (uint8_t i = 0; i < isoTpTestDlc; i++)
    {
        printf("%02X ",
               isoTpTestFrame[i]);
    }

    printf("\r\n");

    printf("TC_ISOTP_FF_001: PASS\r\n");
}

void Test_ISOTP_ConsecutiveFrame(void)
{
    uint8_t data[16] =
    {
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10
    };

    uint8_t flowControl[8] =
    {
        0x30,   /* FC - Continue To Send */
        0x00,   /* Block Size */
        0x00,   /* STmin */
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    };

    IsoTp_StatusType status;

    isoTpTxFrameCount = 0;

    printf("\r\n");
    printf("====================================\r\n");
    printf("TC_ISOTP_CF_001\r\n");
    printf("Consecutive Frame Test\r\n");
    printf("====================================\r\n");

    /*
     * Bước 1:
     * Gửi message 16 byte
     */

    status = IsoTp_Send(
        data,
        16,
        HAL_GetTick()
    );

    if (status != OK)
    {
        printf("TEST FAIL: IsoTp_Send error\r\n");
        return;
    }

    printf("FF transmitted\r\n");

    /*
     * Bước 2:
     * Giả lập FC từ Receiver
     */

    status = IsoTp_OnCanFrame(
        flowControl,
        3,
        HAL_GetTick()
    );

    if (status != ISOTP_OK)
    {
        printf("TEST FAIL: FC processing\r\n");
        return;
    }

    printf("FC received\r\n");

    /*
     * Bước 3:
     * Cho ISO-TP tiếp tục chạy
     */

    for (uint8_t i = 0; i < 20; i++)
    {
        IsoTp_MainFunction(
            HAL_GetTick()
        );

        HAL_Delay(1);
    }

    /*
     * Với 16 byte:
     *
     * FF  = 6 byte
     * CF1 = 7 byte
     * CF2 = 3 byte
     *
     * Tổng:
     *
     * 1 FF + 2 CF = 3 TX frames
     */

    if (isoTpTxFrameCount == 3)
    {
        printf("TC_ISOTP_CF_001: PASS\r\n");
    }
    else
    {
        printf(
            "TC_ISOTP_CF_001: FAIL\r\n"
            "Expected = 3 frames\r\n"
            "Actual   = %lu frames\r\n",
            isoTpTxFrameCount
        );
    }
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
