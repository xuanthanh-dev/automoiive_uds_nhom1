/* ECU diagnostic server. UART1: 115200-8-N-1, TX=PA9, RX=PA10. */
#include "main.h"
#include "Can_if.h"
#include <stdio.h>

#define ECU_CAN_ID_REQUEST       0x7E0U
#define ECU_CAN_ID_RESPONSE      0x7E8U
#define ECU_CAN_ID_ENGINE_STATUS 0x100U
#define ECU_ENGINE_PERIOD_MS     1000U

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

static uint8_t ecuSession = 0x01U;
static uint16_t ecuVehicleSpeed = 20U;
static uint16_t ecuEngineRpm = 600U;
static uint32_t ecuNextEngineStatusMs;

/* ISO-TP transmit context. ECU sends FF, then waits for DIAG flow control. */
static uint8_t ecuTxData[64];
static uint16_t ecuTxLength;
static uint16_t ecuTxSent;
static uint8_t ecuTxSequence;
static uint8_t ecuWaitingFlowControl;

static void SystemClock_Config(void);
static void MX_USART1_UART_Init(void);
static void Ecu_PrintHex(const uint8_t *data, uint16_t length);
static void Ecu_ProcessCan(void);
static void Ecu_ProcessUdsRequest(const uint8_t *request, uint8_t length);
static void Ecu_BuildNegativeResponse(uint8_t service, uint8_t nrc,
                                      uint8_t *response, uint16_t *length);
static void Ecu_SendUdsResponse(const uint8_t *response, uint16_t length);
static void Ecu_SendNextConsecutiveFrame(void);
static void Ecu_SendEngineStatus(void);
static uint8_t Ecu_SendCan(uint16_t canId, const uint8_t *data, uint8_t dlc);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();
    CAN_IF_Init();

    printf("\r\n========== ECU DIAGNOSTIC SERVER READY ==========\r\n");
    printf("CAN: request 0x%03X, response 0x%03X, EngineStatus 0x%03X\r\n",
           ECU_CAN_ID_REQUEST, ECU_CAN_ID_RESPONSE, ECU_CAN_ID_ENGINE_STATUS);
    printf("UART1: 115200 baud. Waiting for DIAG requests...\r\n");

    while (1)
    {
        uint32_t now = HAL_GetTick();
        Ecu_ProcessCan();
        if ((int32_t)(now - ecuNextEngineStatusMs) >= 0)
        {
            Ecu_SendEngineStatus();
            ecuNextEngineStatusMs = now + ECU_ENGINE_PERIOD_MS;
            ecuVehicleSpeed = (uint16_t)((ecuVehicleSpeed >= 120U) ? 20U : ecuVehicleSpeed + 5U);
            ecuEngineRpm = (uint16_t)(ecuVehicleSpeed * 30U);
        }
    }
}

static void Ecu_ProcessCan(void)
{
    uint32_t canId;
    uint8_t frame[8];
    uint8_t dlc;
    uint8_t payloadLength;

    if (CAN_IF_GetReceivedFrame(&canId, frame, &dlc) != OK) return;

    printf("[EC] CAN RX 0x%03lX: ", (unsigned long)canId);
    Ecu_PrintHex(frame, dlc);
    printf("\r\n");

    if ((canId != ECU_CAN_ID_REQUEST) || (dlc == 0U)) return;

    if ((frame[0] & 0xF0U) == 0x00U)
    {
        payloadLength = (uint8_t)(frame[0] & 0x0FU);
        if ((payloadLength == 0U) || (payloadLength > 7U) || (payloadLength > (uint8_t)(dlc - 1U)))
        {
            printf("[EC] Invalid ISO-TP single frame\r\n");
        }
        else
        {
            Ecu_ProcessUdsRequest(&frame[1], payloadLength);
        }
    }
    else if (((frame[0] & 0xF0U) == 0x30U) && (ecuWaitingFlowControl != 0U))
    {
        if ((frame[0] & 0x0FU) == 0x00U)
        {
            printf("[EC] FlowControl CTS received\r\n");
            ecuWaitingFlowControl = 0U;
            Ecu_SendNextConsecutiveFrame();
        }
        else
        {
            printf("[EC] FlowControl rejected by DIAG\r\n");
            ecuWaitingFlowControl = 0U;
        }
    }
}

static void Ecu_ProcessUdsRequest(const uint8_t *request, uint8_t length)
{
    static const uint8_t vin[17] = {'W','0','L','0','0','0','0','0','0','0','0','0','0','0','0','0','1'};
    static const uint8_t swVersion[5] = {'1','.','0','.','0'};
    uint8_t response[64];
    uint16_t responseLength = 0U;
    uint16_t did;
    uint8_t index;

    printf("[EC] UDS RX: ");
    Ecu_PrintHex(request, length);
    printf("\r\n");

    if (length == 0U)
    {
        return;
    }

    if (request[0] == 0x22U)
    {
        if (length != 3U)
        {
            Ecu_BuildNegativeResponse(0x22U, 0x13U, response, &responseLength);
        }
        else
        {
            did = (uint16_t)(((uint16_t)request[1] << 8U) | request[2]);
            response[0] = 0x62U; response[1] = request[1]; response[2] = request[2];
            if (did == 0xF190U)
            {
                for (index = 0U; index < sizeof(vin); index++) response[3U + index] = vin[index];
                responseLength = 20U;
            }
            else if (did == 0xF187U)
            {
                for (index = 0U; index < sizeof(swVersion); index++) response[3U + index] = swVersion[index];
                responseLength = 8U;
            }
            else if (did == 0x0101U)
            {
                response[3] = (uint8_t)(ecuVehicleSpeed >> 8U);
                response[4] = (uint8_t)ecuVehicleSpeed;
                responseLength = 5U;
            }
            else if (did == 0x0102U)
            {
                response[3] = (uint8_t)(ecuEngineRpm >> 8U);
                response[4] = (uint8_t)ecuEngineRpm;
                responseLength = 5U;
            }
            else
            {
                Ecu_BuildNegativeResponse(0x22U, 0x31U, response, &responseLength);
            }
        }
    }
    else if (request[0] == 0x10U)
    {
        if ((length == 2U) && (request[1] == 0x03U))
        {
            ecuSession = 0x03U; response[0] = 0x50U; response[1] = 0x03U; responseLength = 2U;
        }
        else Ecu_BuildNegativeResponse(0x10U, 0x12U, response, &responseLength);
    }
    else if (request[0] == 0x11U)
    {
        if ((length == 2U) && (request[1] == 0x03U))
        {
            response[0] = 0x51U; response[1] = 0x03U; responseLength = 2U;
            ecuSession = 0x01U; /* software reset after positive response is queued */
        }
        else Ecu_BuildNegativeResponse(0x11U, 0x12U, response, &responseLength);
    }
    else if (request[0] == 0x3EU)
    {
        if ((length == 2U) && (request[1] == 0x00U))
        {
            response[0] = 0x7EU; response[1] = 0x00U; responseLength = 2U;
        }
        else Ecu_BuildNegativeResponse(0x3EU, 0x12U, response, &responseLength);
    }
    else if (request[0] == 0x19U)
    {
        if ((length == 3U) && (request[1] == 0x02U))
        {
            /* 59 02 09 + DTC 010001/09 + DTC 010002/09 */
            response[0] = 0x59U; response[1] = 0x02U; response[2] = 0x09U;
            response[3] = 0x01U; response[4] = 0x00U; response[5] = 0x01U; response[6] = 0x09U;
            response[7] = 0x01U; response[8] = 0x00U; response[9] = 0x02U; response[10] = 0x09U;
            responseLength = 11U;
        }
        else Ecu_BuildNegativeResponse(0x19U, 0x12U, response, &responseLength);
    }
    else
    {
        Ecu_BuildNegativeResponse(request[0], 0x11U, response, &responseLength);
    }

    printf("[EC] UDS TX: ");
    Ecu_PrintHex(response, responseLength);
    printf("\r\n");
    Ecu_SendUdsResponse(response, responseLength);
}

static void Ecu_BuildNegativeResponse(uint8_t service, uint8_t nrc,
                                      uint8_t *response, uint16_t *length)
{
    response[0] = 0x7FU; response[1] = service; response[2] = nrc; *length = 3U;
}

static void Ecu_SendUdsResponse(const uint8_t *response, uint16_t length)
{
    uint8_t frame[8] = {0};
    uint16_t index;

    if ((response == 0) || (length == 0U) || (length > sizeof(ecuTxData))) return;

    if (length <= 7U)
    {
        frame[0] = (uint8_t)length;
        for (index = 0U; index < length; index++) frame[index + 1U] = response[index];
        printf("[EC] CAN TX 0x%03X: ", ECU_CAN_ID_RESPONSE); Ecu_PrintHex(frame, 8U); printf("\r\n");
        (void)Ecu_SendCan(ECU_CAN_ID_RESPONSE, frame, 8U);
    }
    else
    {
        for (index = 0U; index < length; index++) ecuTxData[index] = response[index];
        ecuTxLength = length; ecuTxSent = 6U; ecuTxSequence = 1U; ecuWaitingFlowControl = 1U;
        frame[0] = (uint8_t)(0x10U | ((length >> 8U) & 0x0FU));
        frame[1] = (uint8_t)length;
        for (index = 0U; index < 6U; index++) frame[index + 2U] = response[index];
        printf("[EC] CAN TX FirstFrame 0x%03X: ", ECU_CAN_ID_RESPONSE); Ecu_PrintHex(frame, 8U); printf("\r\n");
        (void)Ecu_SendCan(ECU_CAN_ID_RESPONSE, frame, 8U);
    }
}

static void Ecu_SendNextConsecutiveFrame(void)
{
    uint8_t frame[8] = {0};
    uint16_t remaining;
    uint16_t bytesToCopy;
    uint16_t index;

    if (ecuTxSent >= ecuTxLength) return;
    remaining = (uint16_t)(ecuTxLength - ecuTxSent);
    bytesToCopy = (remaining > 7U) ? 7U : remaining;
    frame[0] = (uint8_t)(0x20U | (ecuTxSequence & 0x0FU));
    for (index = 0U; index < bytesToCopy; index++) frame[index + 1U] = ecuTxData[ecuTxSent + index];
    printf("[EC] CAN TX ConsecutiveFrame 0x%03X: ", ECU_CAN_ID_RESPONSE); Ecu_PrintHex(frame, 8U); printf("\r\n");
    if (Ecu_SendCan(ECU_CAN_ID_RESPONSE, frame, 8U) != 0U)
    {
        ecuTxSent = (uint16_t)(ecuTxSent + bytesToCopy);
        ecuTxSequence = (uint8_t)((ecuTxSequence + 1U) & 0x0FU);
        if (ecuTxSent < ecuTxLength) Ecu_SendNextConsecutiveFrame();
    }
}

static void Ecu_SendEngineStatus(void)
{
    uint8_t frame[8] = {0};
    frame[0] = (uint8_t)(ecuVehicleSpeed >> 8U); frame[1] = (uint8_t)ecuVehicleSpeed;
    frame[2] = (uint8_t)(ecuEngineRpm >> 8U); frame[3] = (uint8_t)ecuEngineRpm;
    frame[4] = 90U; frame[5] = 138U;
    printf("[EC][ST-001] CAN TX EngineStatus 0x100: "); Ecu_PrintHex(frame, 8U); printf("\r\n");
    (void)Ecu_SendCan(ECU_CAN_ID_ENGINE_STATUS, frame, 8U);
}

static uint8_t Ecu_SendCan(uint16_t canId, const uint8_t *data, uint8_t dlc)
{
    return (CAN_IF_Transmit(canId, (uint8_t *)data, dlc) == OK) ? 1U : 0U;
}

static void Ecu_PrintHex(const uint8_t *data, uint16_t length)
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

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0}; RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON; osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON; osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE; osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2; clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

int __io_putchar(int ch)
{
    uint8_t character = (uint8_t)ch;
    (void)HAL_UART_Transmit(&huart1, &character, 1U, HAL_MAX_DELAY);
    return ch;
}

void Error_Handler(void) { __disable_irq(); while (1) {} }
