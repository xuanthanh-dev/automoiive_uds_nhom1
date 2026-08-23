/* Diagnostic tester. UART1: 115200-8-N-1, TX=PA9, RX=PA10. */
#include "main.h"
#include "Can_if.h"
#include <stdio.h>
#include <string.h>
#define DIAG_CAN_ID_REQUEST       0x7E0U
#define DIAG_CAN_ID_RESPONSE      0x7E8U
#define DIAG_CAN_ID_ENGINE_STATUS 0x100U
#define DIAG_RESPONSE_TIMEOUT_MS  3000U
#define DIAG_TEST_COUNT           9U

typedef struct
{
    const char *id;
    const char *name;
    uint8_t request[3];
    uint8_t requestLength;
} Diag_TestType;

static const Diag_TestType diagTests[DIAG_TEST_COUNT] =
{
    {"ST-002", "Read VIN",           {0x22U, 0xF1U, 0x90U}, 3U},
    {"ST-003", "Read SW version",    {0x22U, 0xF1U, 0x87U}, 3U},
    {"ST-004", "Read vehicle speed", {0x22U, 0x01U, 0x01U}, 3U},
    {"ST-005", "Read RPM",           {0x22U, 0x01U, 0x02U}, 3U},
    {"ST-006", "Unknown DID",        {0x22U, 0xFFU, 0xFFU}, 3U},
    {"ST-007", "Extended session",   {0x10U, 0x03U, 0x00U}, 2U},
    {"ST-008", "ECU soft reset",     {0x11U, 0x03U, 0x00U}, 2U},
    {"ST-009", "TesterPresent",      {0x3EU, 0x00U, 0x00U}, 2U},
    {"ST-010/011", "Read active DTC",{0x19U, 0x02U, 0xFFU}, 3U}
};

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

static uint8_t diagResponse[64];
static uint16_t diagResponseLength;
static uint16_t diagResponseExpectedLength;
static uint8_t diagExpectedSequence;
static uint8_t diagWaitingResponse;
static uint8_t diagReceivingConsecutiveFrames;
static uint8_t diagSelectedTest;
static uint8_t diagRunAll;
static uint32_t diagRequestTimeMs;
static uint32_t diagNextActionMs;
static volatile uint8_t diagUartKey;
static volatile uint8_t diagUartKeyReady = 0U;

void SystemClock_Config(void);
static void MX_USART1_UART_Init(void);
static void Diag_PrintMenu(void);
static void Diag_PrintHex(const uint8_t *data, uint16_t length);
static void Diag_HandleUart(void);
static void Diag_StartTest(uint8_t testIndex);
static void Diag_ProcessCan(uint32_t now);
static void Diag_HandleResponseFrame(const uint8_t *frame, uint8_t dlc);
static void Diag_CompleteResponse(void);
static void Diag_CheckTimeout(uint32_t now);
static uint8_t Diag_IsResponseExpected(uint8_t testIndex,
                                       const uint8_t *response,
                                       uint16_t length);
static void Diag_SendFlowControl(void);
static uint8_t Diag_SendCan(uint16_t canId, const uint8_t *data, uint8_t dlc);


int main(void)
{
    uint32_t now;

    /* =========================================================
     * 1. HAL initialization
     * ========================================================= */
    HAL_Init();

    /* =========================================================
     * 2. System clock
     * ========================================================= */
    SystemClock_Config();

    /* =========================================================
     * 3. Peripheral initialization
     * ========================================================= */
    MX_USART1_UART_Init();

    /*
     * Initialize CAN interface
     */
    CAN_IF_Init();

    /*
     * LED báo MCU đã khởi động
     */
    blink_led();

    /* =========================================================
     * 4. Start UART1 RX interrupt
     *
     * Nhận từng byte từ Serial Debug.
     * Ví dụ:
     *
     * PC gửi '1'
     *       ↓
     * USART1 RX
     *       ↓
     * HAL_UART_RxCpltCallback()
     * =========================================================
     */
    printf("[DEBUG] Start UART RX\r\n");

    HAL_UART_Receive_IT(&huart1,
                               (uint8_t *)&diagUartKey,
                               1U);
    printf("[DEBUG] RX: '%c' 0x%02X\r\n",
           diagUartKey,
           diagUartKey);
//    if (HAL_UART_Receive_IT(&huart1,
//                           (uint8_t *)&diagUartKey,
//                           1U) != HAL_OK)
//    {
//        Error_Handler();
//    }

    /* =========================================================
     * 5. Diagnostic Tester startup message
     * ========================================================= */
    printf("\r\n");
    printf("========================================\r\n");
    printf("        DIAGNOSTIC TESTER READY\r\n");
    printf("========================================\r\n");

    printf("UART1 : 115200 8-N-1\r\n");

    printf("CAN Request  : 0x%03X\r\n",
           DIAG_CAN_ID_REQUEST);

    printf("CAN Response : 0x%03X\r\n",
           DIAG_CAN_ID_RESPONSE);

    printf("========================================\r\n");

    /*
     * Hiển thị menu DIAG
     */
    Diag_PrintMenu();

    /* =========================================================
     * 6. Main loop
     * ========================================================= */
    while (1)
    {
//        now = HAL_GetTick();
//        /* Process CAN */
//        Diag_ProcessCan(now);
//
//        /* Check diagnostic timeout */
//        Diag_CheckTimeout(now);
//        /* =====================================================
//         * UART RX processing
//         * ===================================================== */
//        if (diagUartKeyReady != 0U)
//        {
//            uint8_t key;
//
//            /*
//             * Copy byte nhận được
//             */
//            key = diagUartKey;
//
//            /*
//             * Clear flag
//             */
//            diagUartKeyReady = 0U;
//
//            /*
//             * Debug:
//             * hiển thị ký tự và mã HEX nhận được
//             *
//             * Ví dụ gửi '1':
//             *
//             * UART RX = '1' 0x31
//             */
//            printf("\r\nUART RX = '%c' (0x%02X)\r\n",
//                   key,
//                   key);
//
//            /* =================================================
//             * Nhận phím 1 -> 9
//             * ================================================= */
//            if ((key >= '1') && (key <= '9'))
//            {
//                /*
//                 * Không cho chạy test mới nếu đang
//                 * chờ CAN response.
//                 */
//                if (diagWaitingResponse != 0U)
//                {
//                    printf("DIAG dang cho CAN response...\r\n");
//                }
//                else
//                {
//                    /*
//                     * '1' -> index 0
//                     * '2' -> index 1
//                     * ...
//                     * '9' -> index 8
//                     */
//                    diagRunAll = 0U;
//
//                    diagSelectedTest =
//                        (uint8_t)(key - '1');
//
//                    printf("Starting DIAG TEST %d...\r\n",
//                           diagSelectedTest + 1U);
//
//                    Diag_StartTest(diagSelectedTest);
//                }
//            }
//
//            /* =================================================
//             * Nhận A hoặc a -> chạy toàn bộ test
//             * ================================================= */
//            else if ((key == 'A') || (key == 'a'))
//            {
//                if (diagWaitingResponse == 0U)
//                {
//                    printf("\r\n");
//                    printf("DIAG: CHAY TOAN BO SYSTEM TEST\r\n");
//
//                    diagRunAll = 1U;
//
//                    diagSelectedTest = 0U;
//
//                    diagNextActionMs = now;
//                }
//                else
//                {
//                    printf("DIAG dang cho CAN response...\r\n");
//                }
//            }
//
//            /* =================================================
//             * Bỏ qua Enter / Newline
//             * ================================================= */
//            else if ((key == '\r') || (key == '\n'))
//            {
//                /*
//                 * Không làm gì.
//                 *
//                 * Serial terminal thường gửi:
//                 * '1' + '\r' + '\n'
//                 */
//            }
//
//            /* =================================================
//             * Ký tự không hợp lệ
//             * ================================================= */
//            else
//            {
//                printf("Lua chon khong hop le!\r\n");
//
//                Diag_PrintMenu();
//            }
//        }
//
//        /* =====================================================
//         * CAN processing
//         * ===================================================== */
//
//        Diag_ProcessCan(now);
//
//        /* =====================================================
//         * CAN timeout processing
//         * ===================================================== */
//
//        Diag_CheckTimeout(now);
//
//        /* =====================================================
//         * RUN ALL TEST
//         * ===================================================== */
//
//        if ((diagRunAll != 0U) &&
//            (diagWaitingResponse == 0U) &&
//            ((int32_t)(now - diagNextActionMs) >= 0))
//        {
//            /*
//             * Còn test để chạy
//             */
//            if (diagSelectedTest < DIAG_TEST_COUNT)
//            {
//                Diag_StartTest(diagSelectedTest);
//            }
//            else
//            {
//                /*
//                 * Đã chạy xong toàn bộ test
//                 */
//                diagRunAll = 0U;
//
//                printf("\r\n");
//                printf("========================================\r\n");
//                printf("       ALL TESTS FINISHED\r\n");
//                printf("========================================\r\n");
//
//                Diag_PrintMenu();
//            }
//        }

    	if (diagUartKeyReady != 0U)
    	{
    	    diagUartKeyReady = 0U;

    	    printf("[DEBUG] RX: '%c' 0x%02X\r\n",
    	           diagUartKey,
    	           diagUartKey);

    	    if (diagUartKey == '1')
    	    {
    	        printf("[DEBUG] KEY 1 DETECTED\r\n");
    	    }

    	    HAL_UART_Receive_IT(&huart1,
    	                       (uint8_t *)&diagUartKey,
    	                       1U);
    	}
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        diagUartKeyReady = 1U;
        blink_led();
        /* Re-arm UART RX interrupt */
//        HAL_UART_Receive_IT(&huart1,
//                           (uint8_t *)&diagUartKey,
//                           1U);
    }
}


static void Diag_PrintMenu(void)
{
    printf("\r\nChon yeu cau DIAG (gui ky tu tren UART):\r\n");
    printf("  1: ST-002 VIN          2: ST-003 SW version\r\n");
    printf("  3: ST-004 VehicleSpeed 4: ST-005 RPM\r\n");
    printf("  5: ST-006 Unknown DID  6: ST-007 Extended session\r\n");
    printf("  7: ST-008 ECU reset    8: ST-009 TesterPresent\r\n");
    printf("  9: ST-010/ST-011 Read DTC   A: Chay tat ca\r\n> ");
}


static void Diag_StartTest(uint8_t testIndex)
{
    uint8_t frame[8] = {0};
    uint8_t index;

    if (testIndex >= DIAG_TEST_COUNT)
    {
        return;
    }

    frame[0] = diagTests[testIndex].requestLength;
    for (index = 0U; index < diagTests[testIndex].requestLength; index++)
    {
        frame[index + 1U] = diagTests[testIndex].request[index];
    }

    printf("\r\n[DIAG][%s] %s\r\n", diagTests[testIndex].id,
           diagTests[testIndex].name);
    printf("[DIAG] UDS TX: ");
    Diag_PrintHex(diagTests[testIndex].request, diagTests[testIndex].requestLength);
    printf("\r\n[DIAG] CAN TX 0x%03X: ", DIAG_CAN_ID_REQUEST);
    Diag_PrintHex(frame, 8U);
    printf("\r\n");

    if (Diag_SendCan(DIAG_CAN_ID_REQUEST, frame, 8U) != 0U)
    {
        diagSelectedTest = testIndex;
        diagWaitingResponse = 1U;
        diagReceivingConsecutiveFrames = 0U;
        diagResponseLength = 0U;
        diagRequestTimeMs = HAL_GetTick();
    }
    else
    {
        printf("[DIAG][%s] FAIL - CAN transmit error\r\n", diagTests[testIndex].id);
        if (diagRunAll != 0U)
        {
            diagSelectedTest++;
            diagNextActionMs = HAL_GetTick() + 150U;
        }
        else
        {
            Diag_PrintMenu();
        }
    }
}

static void Diag_ProcessCan(uint32_t now)
{
    uint32_t canId;
    uint8_t frame[8];
    uint8_t dlc;

    (void)now;
    if (CAN_IF_GetReceivedFrame(&canId, frame, &dlc) == OK)
    {
        printf("[DIAG] CAN RX 0x%03lX: ", (unsigned long)canId);
        Diag_PrintHex(frame, dlc);
        printf("\r\n");

        if (canId == DIAG_CAN_ID_RESPONSE)
        {
            Diag_HandleResponseFrame(frame, dlc);
        }
        else if ((canId == DIAG_CAN_ID_ENGINE_STATUS) && (dlc == 8U))
        {
            uint16_t speed = (uint16_t)(((uint16_t)frame[0] << 8U) | frame[1]);
            uint16_t rpm = (uint16_t)(((uint16_t)frame[2] << 8U) | frame[3]);
            printf("[DIAG][ST-001] EngineStatus: speed=%u km/h, rpm=%u, temp=%u C, batt=%u.%u V\r\n",
                   speed, rpm, frame[4], frame[5] / 10U, frame[5] % 10U);
        }
    }
}

static void Diag_HandleResponseFrame(const uint8_t *frame, uint8_t dlc)
{
    uint8_t frameType;
    uint16_t copyLength;
    uint16_t remaining;

    if ((diagWaitingResponse == 0U) || (dlc == 0U))
    {
        return;
    }

    frameType = (uint8_t)(frame[0] & 0xF0U);
    if (frameType == 0x00U)
    {
        copyLength = (uint16_t)(frame[0] & 0x0FU);
        if ((copyLength == 0U) || (copyLength > 7U) || (copyLength > (uint16_t)(dlc - 1U)))
        {
            printf("[DIAG] Invalid ISO-TP single frame\r\n");
            return;
        }
        for (remaining = 0U; remaining < copyLength; remaining++)
        {
            diagResponse[remaining] = frame[remaining + 1U];
        }
        diagResponseLength = copyLength;
        Diag_CompleteResponse();
    }
    else if ((frameType == 0x10U) && (dlc == 8U))
    {
        diagResponseExpectedLength = (uint16_t)(((uint16_t)(frame[0] & 0x0FU) << 8U) | frame[1]);
        if ((diagResponseExpectedLength <= 7U) || (diagResponseExpectedLength > sizeof(diagResponse)))
        {
            printf("[DIAG] Invalid ISO-TP first frame length\r\n");
            return;
        }
        for (remaining = 0U; remaining < 6U; remaining++)
        {
            diagResponse[remaining] = frame[remaining + 2U];
        }
        diagResponseLength = 6U;
        diagExpectedSequence = 1U;
        diagReceivingConsecutiveFrames = 1U;
        Diag_SendFlowControl();
    }
    else if ((frameType == 0x20U) && (diagReceivingConsecutiveFrames != 0U))
    {
        if ((frame[0] & 0x0FU) != diagExpectedSequence)
        {
            printf("[DIAG] ISO-TP sequence error\r\n");
            diagReceivingConsecutiveFrames = 0U;
            return;
        }
        remaining = (uint16_t)(diagResponseExpectedLength - diagResponseLength);
        copyLength = (remaining > 7U) ? 7U : remaining;
        if (copyLength > (uint16_t)(dlc - 1U))
        {
            printf("[DIAG] ISO-TP consecutive frame too short\r\n");
            return;
        }
        for (remaining = 0U; remaining < copyLength; remaining++)
        {
            diagResponse[diagResponseLength + remaining] = frame[remaining + 1U];
        }
        diagResponseLength = (uint16_t)(diagResponseLength + copyLength);
        diagExpectedSequence = (uint8_t)((diagExpectedSequence + 1U) & 0x0FU);
        if (diagResponseLength == diagResponseExpectedLength)
        {
            diagReceivingConsecutiveFrames = 0U;
            Diag_CompleteResponse();
        }
    }
}

static void Diag_CompleteResponse(void)
{
    uint8_t passed;

    printf("[DIAG] UDS RX: ");
    Diag_PrintHex(diagResponse, diagResponseLength);
    printf("\r\n");
    passed = Diag_IsResponseExpected(diagSelectedTest, diagResponse, diagResponseLength);
    printf("[DIAG][%s] %s\r\n", diagTests[diagSelectedTest].id,
           (passed != 0U) ? "PASS" : "FAIL");
    if (diagSelectedTest == 8U)
    {
        printf("[DIAG][ST-010] OverTemp DTC 0x010001: %s\r\n",
               (passed != 0U) ? "PASS" : "FAIL");
        printf("[DIAG][ST-011] LowBattery DTC 0x010002: %s\r\n",
               (passed != 0U) ? "PASS" : "FAIL");
    }

    diagWaitingResponse = 0U;
    if (diagRunAll != 0U)
    {
        diagSelectedTest++;
        diagNextActionMs = HAL_GetTick() + 150U;
    }
    else
    {
        Diag_PrintMenu();
    }
}

static uint8_t Diag_IsResponseExpected(uint8_t testIndex,
                                       const uint8_t *response,
                                       uint16_t length)
{
    if ((response == 0) || (length == 0U)) return 0U;
    if (testIndex < 4U) return (length >= 3U) && (response[0] == 0x62U);
    if (testIndex == 4U) return (length == 3U) && (response[0] == 0x7FU) &&
                               (response[1] == 0x22U) && (response[2] == 0x31U);
    if (testIndex == 5U) return (length == 2U) && (response[0] == 0x50U) && (response[1] == 0x03U);
    if (testIndex == 6U) return (length == 2U) && (response[0] == 0x51U) && (response[1] == 0x03U);
    if (testIndex == 7U) return (length == 2U) && (response[0] == 0x7EU) && (response[1] == 0x00U);
    return (length == 11U) && (response[0] == 0x59U) && (response[1] == 0x02U) &&
           (response[3] == 0x01U) && (response[5] == 0x01U) &&
           (response[7] == 0x01U) && (response[9] == 0x02U);
}

static void Diag_SendFlowControl(void)
{
    uint8_t frame[8] = {0x30U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    printf("[DIAG] CAN TX FlowControl 0x%03X\r\n", DIAG_CAN_ID_REQUEST);
    (void)Diag_SendCan(DIAG_CAN_ID_REQUEST, frame, 8U);
}

static void Diag_CheckTimeout(uint32_t now)
{
    if ((diagWaitingResponse != 0U) && ((now - diagRequestTimeMs) > DIAG_RESPONSE_TIMEOUT_MS))
    {
        printf("[DIAG][%s] FAIL - timeout waiting ECU response\r\n", diagTests[diagSelectedTest].id);
        diagWaitingResponse = 0U;
        diagReceivingConsecutiveFrames = 0U;
        if (diagRunAll != 0U)
        {
            diagSelectedTest++;
            diagNextActionMs = now + 150U;
        }
        else Diag_PrintMenu();
    }
}

static uint8_t Diag_SendCan(uint16_t canId, const uint8_t *data, uint8_t dlc)
{
    return (CAN_IF_Transmit(canId, (uint8_t *)data, dlc) == OK) ? 1U : 0U;
}

static void Diag_PrintHex(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    for (index = 0U; index < length; index++) printf("%02X ", data[index]);
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
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

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

void uartlog(char *message)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)message,
                      strlen(message),
                      HAL_MAX_DELAY);
}
void blink_led(void)
{
    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_5,
                      GPIO_PIN_SET);

    HAL_Delay(2000);

    HAL_GPIO_WritePin(GPIOA,
                      GPIO_PIN_5,
                      GPIO_PIN_RESET);
}


void Error_Handler(void) { __disable_irq(); while (1) {} }
