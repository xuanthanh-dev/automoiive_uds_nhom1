/*
 * ECU diagnostic server.
 *
 * UART1:
 *   115200-8-N-1
 *   TX = PA9
 *   RX = PA10
 *
 * CAN:
 *   Request  = 0x7E0
 *   Response = 0x7E8
 *   Engine Status = 0x100
 */

#include "main.h"
#include "Can_if.h"
#include <stdio.h>
#include <stdint.h>

/* ============================================================
 * ECU CAN CONFIGURATION
 * ============================================================ */
#define ECU_CAN_ID_REQUEST          0x7E0U
#define ECU_CAN_ID_RESPONSE         0x7E8U
#define ECU_CAN_ID_ENGINE_STATUS    0x100U

#define ECU_ENGINE_PERIOD_MS        1000U

#define ECU_ISOTP_MAX_LENGTH        64U
#define ECU_ISOTP_CF_DELAY_MS       1U

/* ============================================================
 * HAL HANDLES
 * ============================================================ */
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

/* ============================================================
 * ECU APPLICATION DATA
 * ============================================================ */
static uint8_t ecuSession = 0x01U;

static uint16_t ecuVehicleSpeed = 20U;

static uint16_t ecuEngineRpm = 600U;

static uint32_t ecuNextEngineStatusMs;

/* ============================================================
 * ECU ISO-TP TRANSMISSION
 * ============================================================ */
static uint8_t ecuTxData[ECU_ISOTP_MAX_LENGTH];

static uint16_t ecuTxLength;

static uint16_t ecuTxSent;

static uint8_t ecuTxSequence;

static uint8_t ecuWaitingFlowControl;

static uint32_t ecuNextConsecutiveFrameMs;

/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */
static void SystemClock_Config(void);
static void MX_USART1_UART_Init(void);

static void Ecu_PrintHex(
    const uint8_t *data,
    uint16_t length
);

static void Ecu_ProcessCan(void);

static void Ecu_ProcessUdsRequest(
    const uint8_t *request,
    uint8_t length
);

static void Ecu_BuildNegativeResponse(
    uint8_t service,
    uint8_t nrc,
    uint8_t *response,
    uint16_t *length
);

static void Ecu_SendUdsResponse(
    const uint8_t *response,
    uint16_t length
);

static void Ecu_ProcessIsoTpTx(uint32_t now);

static void Ecu_SendNextConsecutiveFrame(void);

static void Ecu_SendEngineStatus(void);

static uint8_t Ecu_SendCan(
    uint16_t canId,
    const uint8_t *data,
    uint8_t dlc
);

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_USART1_UART_Init();

    /*
     * Initialize CAN interface.
     */
    if (CAN_IF_Init() != OK)
    {
        Error_Handler();
    }

    /*
     * Initial engine status transmission.
     */
    ecuNextEngineStatusMs =
        HAL_GetTick();

    printf("\r\n");
    printf("========================================\r\n");
    printf("       ECU DIAGNOSTIC SERVER READY\r\n");
    printf("========================================\r\n");

    printf(
        "CAN REQUEST  : 0x%03X\r\n",
        ECU_CAN_ID_REQUEST
    );

    printf(
        "CAN RESPONSE : 0x%03X\r\n",
        ECU_CAN_ID_RESPONSE
    );

    printf(
        "ENGINE STATUS: 0x%03X\r\n",
        ECU_CAN_ID_ENGINE_STATUS
    );

    printf(
        "UART: 115200 8-N-1\r\n"
    );

    printf(
        "Waiting for DIAG requests...\r\n"
    );

    while (1)
    {
        uint32_t now =
            HAL_GetTick();

        /* ====================================================
         * CAN RX
         * ==================================================== */
        Ecu_ProcessCan();

        /* ====================================================
         * ISO-TP TX
         * ==================================================== */
        Ecu_ProcessIsoTpTx(now);

        /* ====================================================
         * PERIODIC ENGINE STATUS
         * ==================================================== */
        if ((int32_t)(
                now -
                ecuNextEngineStatusMs) >= 0)
        {
            Ecu_SendEngineStatus();

            ecuNextEngineStatusMs =
                now + ECU_ENGINE_PERIOD_MS;

            /*
             * Simulate vehicle speed.
             */
            if (ecuVehicleSpeed >= 120U)
            {
                ecuVehicleSpeed = 20U;
            }
            else
            {
                ecuVehicleSpeed =
                    (uint16_t)(
                        ecuVehicleSpeed + 5U
                    );
            }

            /*
             * Simulate RPM.
             */
            ecuEngineRpm =
                (uint16_t)(
                    ecuVehicleSpeed * 30U
                );
        }
    }
}

/* ============================================================
 * PROCESS CAN RX
 * ============================================================ */
static void Ecu_ProcessCan(void)
{
    uint32_t canId;

    uint8_t frame[8];

    uint8_t dlc;

    uint8_t payloadLength;

    if (CAN_IF_GetReceivedFrame(
            &canId,
            frame,
            &dlc) != OK)
    {
        return;
    }

    printf(
        "[EC] CAN RX 0x%03lX: ",
        (unsigned long)canId
    );

    Ecu_PrintHex(
        frame,
        dlc
    );

    printf("\r\n");

    /*
     * Only accept diagnostic requests
     * from 0x7E0.
     */
    if ((canId != ECU_CAN_ID_REQUEST) ||
        (dlc == 0U))
    {
        return;
    }

    /* ========================================================
     * SINGLE FRAME
     * ======================================================== */
    if ((frame[0] & 0xF0U) == 0x00U)
    {
        payloadLength =
            (uint8_t)(
                frame[0] & 0x0FU
            );

        if ((payloadLength == 0U) ||
            (payloadLength > 7U) ||
            (payloadLength >
             (uint8_t)(dlc - 1U)))
        {
            printf(
                "[EC] Invalid ISO-TP Single Frame\r\n"
            );

            return;
        }

        Ecu_ProcessUdsRequest(
            &frame[1],
            payloadLength
        );

        return;
    }

    /* ========================================================
     * FLOW CONTROL
     * ======================================================== */
    if (((frame[0] & 0xF0U) == 0x30U) &&
        (ecuWaitingFlowControl != 0U))
    {
        uint8_t flowStatus;

        flowStatus =
            (uint8_t)(
                frame[0] & 0x0FU
            );

        /*
         * 0 = Continue To Send
         */
        if (flowStatus == 0x00U)
        {
            printf(
                "[EC] FlowControl CTS received\r\n"
            );

            ecuWaitingFlowControl = 0U;

            /*
             * Send first CF on next main loop.
             */
            ecuNextConsecutiveFrameMs =
                HAL_GetTick();
        }
        /*
         * 1 = Wait
         */
        else if (flowStatus == 0x01U)
        {
            printf(
                "[EC] FlowControl WAIT received\r\n"
            );

            /*
             * Keep waiting.
             */
        }
        /*
         * 2 = Overflow / Abort
         */
        else
        {
            printf(
                "[EC] FlowControl OVERFLOW/ABORT\r\n"
            );

            ecuWaitingFlowControl = 0U;

            ecuTxLength = 0U;
            ecuTxSent = 0U;
        }
    }
}

/* ============================================================
 * PROCESS UDS REQUEST
 * ============================================================ */
static void Ecu_ProcessUdsRequest(
    const uint8_t *request,
    uint8_t length)
{
    /*
     * Example VIN.
     */
    static const uint8_t vin[17] =
    {
        'W',
        '0',
        'L',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '0',
        '1'
    };

    /*
     * Example software version.
     */
    static const uint8_t swVersion[5] =
    {
        '1',
        '.',
        '0',
        '.',
        '0'
    };

    uint8_t response[ECU_ISOTP_MAX_LENGTH];

    uint16_t responseLength = 0U;

    uint16_t did;

    uint8_t index;

    printf("[EC] UDS RX: ");

    Ecu_PrintHex(
        request,
        length
    );

    printf("\r\n");

    if ((request == NULL) ||
        (length == 0U))
    {
        return;
    }

    /* ========================================================
     * SERVICE 0x22
     * ReadDataByIdentifier
     * ======================================================== */
    if (request[0] == 0x22U)
    {
        if (length != 3U)
        {
            Ecu_BuildNegativeResponse(
                0x22U,
                0x13U,
                response,
                &responseLength
            );
        }
        else
        {
            did =
                (uint16_t)(
                    ((uint16_t)request[1] << 8U) |
                    request[2]
                );

            response[0] = 0x62U;

            response[1] = request[1];

            response[2] = request[2];

            /* =================================================
             * VIN F190
             * ================================================= */
            if (did == 0xF190U)
            {
                for (index = 0U;
                     index < sizeof(vin);
                     index++)
                {
                    response[3U + index] =
                        vin[index];
                }

                responseLength = 20U;
            }

            /* =================================================
             * SW VERSION F187
             * ================================================= */
            else if (did == 0xF187U)
            {
                for (index = 0U;
                     index < sizeof(swVersion);
                     index++)
                {
                    response[3U + index] =
                        swVersion[index];
                }

                responseLength = 8U;
            }

            /* =================================================
             * VEHICLE SPEED 0101
             * ================================================= */
            else if (did == 0x0101U)
            {
                response[3] =
                    (uint8_t)(
                        ecuVehicleSpeed >> 8U
                    );

                response[4] =
                    (uint8_t)(
                        ecuVehicleSpeed
                    );

                responseLength = 5U;
            }

            /* =================================================
             * RPM 0102
             * ================================================= */
            else if (did == 0x0102U)
            {
                response[3] =
                    (uint8_t)(
                        ecuEngineRpm >> 8U
                    );

                response[4] =
                    (uint8_t)(
                        ecuEngineRpm
                    );

                responseLength = 5U;
            }

            /* =================================================
             * UNKNOWN DID
             * ================================================= */
            else
            {
                Ecu_BuildNegativeResponse(
                    0x22U,
                    0x31U,
                    response,
                    &responseLength
                );
            }
        }
    }

    /* ========================================================
     * SERVICE 0x10
     * DiagnosticSessionControl
     * ======================================================== */
    else if (request[0] == 0x10U)
    {
        if ((length == 2U) &&
            (request[1] == 0x03U))
        {
            ecuSession = 0x03U;

            response[0] = 0x50U;
            response[1] = 0x03U;

            responseLength = 2U;
        }
        else
        {
            Ecu_BuildNegativeResponse(
                0x10U,
                0x12U,
                response,
                &responseLength
            );
        }
    }

    /* ========================================================
     * SERVICE 0x11
     * ECUReset
     * ======================================================== */
    else if (request[0] == 0x11U)
    {
        if ((length == 2U) &&
            (request[1] == 0x03U))
        {
            /*
             * Send positive response first.
             */
            response[0] = 0x51U;
            response[1] = 0x03U;

            responseLength = 2U;

            /*
             * Return to default session.
             *
             * Actual hardware reset is NOT performed here
             * because we need to transmit the response first.
             */
            ecuSession = 0x01U;
        }
        else
        {
            Ecu_BuildNegativeResponse(
                0x11U,
                0x12U,
                response,
                &responseLength
            );
        }
    }

    /* ========================================================
     * SERVICE 0x3E
     * TesterPresent
     * ======================================================== */
    else if (request[0] == 0x3EU)
    {
        if ((length == 2U) &&
            (request[1] == 0x00U))
        {
            response[0] = 0x7EU;
            response[1] = 0x00U;

            responseLength = 2U;
        }
        else
        {
            Ecu_BuildNegativeResponse(
                0x3EU,
                0x12U,
                response,
                &responseLength
            );
        }
    }

    /* ========================================================
     * SERVICE 0x19
     * ReadDTCInformation
     * ======================================================== */
    else if (request[0] == 0x19U)
    {
        if ((length == 3U) &&
            (request[1] == 0x02U))
        {
            /*
             * Response:
             *
             * 59 02 09
             *
             * DTC 010001 status 09
             *
             * DTC 010002 status 09
             */
            response[0] = 0x59U;
            response[1] = 0x02U;
            response[2] = 0x09U;

            /* DTC 010001 */
            response[3] = 0x01U;
            response[4] = 0x00U;
            response[5] = 0x01U;
            response[6] = 0x09U;

            /* DTC 010002 */
            response[7] = 0x01U;
            response[8] = 0x00U;
            response[9] = 0x02U;
            response[10] = 0x09U;

            responseLength = 11U;
        }
        else
        {
            Ecu_BuildNegativeResponse(
                0x19U,
                0x12U,
                response,
                &responseLength
            );
        }
    }

    /* ========================================================
     * UNKNOWN SERVICE
     * ======================================================== */
    else
    {
        Ecu_BuildNegativeResponse(
            request[0],
            0x11U,
            response,
            &responseLength
        );
    }

    /* ========================================================
     * PRINT UDS RESPONSE
     * ======================================================== */
    printf("[EC] UDS TX: ");

    Ecu_PrintHex(
        response,
        responseLength
    );

    printf("\r\n");

    /* ========================================================
     * SEND ISO-TP RESPONSE
     * ======================================================== */
    Ecu_SendUdsResponse(
        response,
        responseLength
    );
}

/* ============================================================
 * BUILD NEGATIVE RESPONSE
 *
 * 7F <Service> <NRC>
 * ============================================================ */
static void Ecu_BuildNegativeResponse(
    uint8_t service,
    uint8_t nrc,
    uint8_t *response,
    uint16_t *length)
{
    if ((response == NULL) ||
        (length == NULL))
    {
        return;
    }

    response[0] = 0x7FU;

    response[1] = service;

    response[2] = nrc;

    *length = 3U;
}

/* ============================================================
 * SEND UDS RESPONSE
 *
 * <= 7 bytes:
 *
 *   Single Frame
 *
 * > 7 bytes:
 *
 *   First Frame
 *   wait Flow Control
 *   Consecutive Frames
 * ============================================================ */
static void Ecu_SendUdsResponse(
    const uint8_t *response,
    uint16_t length)
{
    uint8_t frame[8] = {0};

    uint16_t index;

    if ((response == NULL) ||
        (length == 0U) ||
        (length > ECU_ISOTP_MAX_LENGTH))
    {
        return;
    }

    /* ========================================================
     * SINGLE FRAME
     * ======================================================== */
    if (length <= 7U)
    {
        frame[0] =
            (uint8_t)length;

        for (index = 0U;
             index < length;
             index++)
        {
            frame[index + 1U] =
                response[index];
        }

        printf(
            "[EC] CAN TX 0x%03X: ",
            ECU_CAN_ID_RESPONSE
        );

        Ecu_PrintHex(
            frame,
            8U
        );

        printf("\r\n");

        (void)Ecu_SendCan(
            ECU_CAN_ID_RESPONSE,
            frame,
            8U
        );

        return;
    }

    /* ========================================================
     * FIRST FRAME
     * ======================================================== */

    /*
     * Store complete response.
     */
    for (index = 0U;
         index < length;
         index++)
    {
        ecuTxData[index] =
            response[index];
    }

    ecuTxLength = length;

    /*
     * FF already contains 6 payload bytes.
     */
    ecuTxSent = 6U;

    /*
     * Next CF sequence number.
     */
    ecuTxSequence = 1U;

    /*
     * Wait for Flow Control.
     */
    ecuWaitingFlowControl = 1U;

    /*
     * Build First Frame.
     */
    frame[0] =
        (uint8_t)(
            0x10U |
            ((length >> 8U) & 0x0FU)
        );

    frame[1] =
        (uint8_t)length;

    for (index = 0U;
         index < 6U;
         index++)
    {
        frame[index + 2U] =
            response[index];
    }

    printf(
        "[EC] CAN TX FirstFrame 0x%03X: ",
        ECU_CAN_ID_RESPONSE
    );

    Ecu_PrintHex(
        frame,
        8U
    );

    printf("\r\n");

    if (Ecu_SendCan(
            ECU_CAN_ID_RESPONSE,
            frame,
            8U) == 0U)
    {
        printf(
            "[EC] First Frame transmit error\r\n"
        );

        ecuWaitingFlowControl = 0U;
        ecuTxLength = 0U;
        ecuTxSent = 0U;
    }
}

/* ============================================================
 * PROCESS ISO-TP TX
 * ============================================================ */
static void Ecu_ProcessIsoTpTx(
    uint32_t now)
{
    /*
     * No response currently being transmitted.
     */
    if (ecuTxLength == 0U)
    {
        return;
    }

    /*
     * Still waiting for Flow Control.
     */
    if (ecuWaitingFlowControl != 0U)
    {
        return;
    }

    /*
     * All data already sent.
     */
    if (ecuTxSent >= ecuTxLength)
    {
        ecuTxLength = 0U;
        ecuTxSent = 0U;
        ecuTxSequence = 0U;

        return;
    }

    /*
     * Respect minimum delay between CF.
     */
    if ((int32_t)(
            now -
            ecuNextConsecutiveFrameMs) < 0)
    {
        return;
    }

    Ecu_SendNextConsecutiveFrame();

    /*
     * Schedule next CF.
     */
    ecuNextConsecutiveFrameMs =
        now + ECU_ISOTP_CF_DELAY_MS;
}

/* ============================================================
 * SEND NEXT CONSECUTIVE FRAME
 * ============================================================ */
static void Ecu_SendNextConsecutiveFrame(void)
{
    uint8_t frame[8] = {0};

    uint16_t remaining;

    uint16_t bytesToCopy;

    uint16_t index;

    if ((ecuTxLength == 0U) ||
        (ecuTxSent >= ecuTxLength))
    {
        return;
    }

    remaining =
        (uint16_t)(
            ecuTxLength -
            ecuTxSent
        );

    bytesToCopy =
        (remaining > 7U) ?
        7U :
        remaining;

    /*
     * Consecutive Frame.
     */
    frame[0] =
        (uint8_t)(
            0x20U |
            (ecuTxSequence & 0x0FU)
        );

    for (index = 0U;
         index < bytesToCopy;
         index++)
    {
        frame[index + 1U] =
            ecuTxData[
                ecuTxSent + index
            ];
    }

    printf(
        "[EC] CAN TX ConsecutiveFrame "
        "0x%03X: ",
        ECU_CAN_ID_RESPONSE
    );

    Ecu_PrintHex(
        frame,
        8U
    );

    printf("\r\n");

    if (Ecu_SendCan(
            ECU_CAN_ID_RESPONSE,
            frame,
            8U) != 0U)
    {
        /*
         * Successfully transmitted.
         */
        ecuTxSent =
            (uint16_t)(
                ecuTxSent +
                bytesToCopy
            );

        ecuTxSequence =
            (uint8_t)(
                (ecuTxSequence + 1U) &
                0x0FU
            );

        /*
         * Transmission completed.
         */
        if (ecuTxSent >= ecuTxLength)
        {
            ecuTxLength = 0U;

            ecuTxSent = 0U;

            ecuTxSequence = 0U;

            ecuWaitingFlowControl = 0U;

            printf(
                "[EC] ISO-TP TX complete\r\n"
            );
        }
    }
    else
    {
        printf(
            "[EC] Consecutive Frame "
            "transmit error\r\n"
        );
    }
}

/* ============================================================
 * SEND ENGINE STATUS
 *
 * CAN ID = 0x100
 *
 * Byte 0-1 = Speed
 * Byte 2-3 = RPM
 * Byte 4   = Temperature
 * Byte 5   = Battery x10
 * Byte 6-7 = Reserved
 * ============================================================ */
static void Ecu_SendEngineStatus(void)
{
    uint8_t frame[8] = {0};

    frame[0] =
        (uint8_t)(
            ecuVehicleSpeed >> 8U
        );

    frame[1] =
        (uint8_t)(
            ecuVehicleSpeed
        );

    frame[2] =
        (uint8_t)(
            ecuEngineRpm >> 8U
        );

    frame[3] =
        (uint8_t)(
            ecuEngineRpm
        );

    /*
     * Temperature = 90 C
     */
    frame[4] = 90U;

    /*
     * Battery = 13.8 V
     * encoded as 138
     */
    frame[5] = 138U;

    printf(
        "[EC][ST-001] CAN TX EngineStatus "
        "0x100: "
    );

    Ecu_PrintHex(
        frame,
        8U
    );

    printf("\r\n");

    (void)Ecu_SendCan(
        ECU_CAN_ID_ENGINE_STATUS,
        frame,
        8U
    );
}

/* ============================================================
 * CAN SEND WRAPPER
 * ============================================================ */
static uint8_t Ecu_SendCan(
    uint16_t canId,
    const uint8_t *data,
    uint8_t dlc)
{
    if ((data == NULL) ||
        (dlc > 8U))
    {
        return 0U;
    }

    if (CAN_IF_Transmit(
            canId,
            (uint8_t *)data,
            dlc) == OK)
    {
        return 1U;
    }

    return 0U;
}

/* ============================================================
 * PRINT HEX
 * ============================================================ */
static void Ecu_PrintHex(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t index;

    if (data == NULL)
    {
        return;
    }

    for (index = 0U;
         index < length;
         index++)
    {
        printf("%02X ", data[index]);
    }
}

/* ============================================================
 * UART INIT
 * ============================================================ */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200U;

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
 * PRINTF -> UART
 * ============================================================ */
int __io_putchar(int ch)
{
    uint8_t character =
        (uint8_t)ch;

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
