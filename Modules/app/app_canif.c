/**
 * @file    app_can.c
 * @brief   Application CAN - giao dien nguoi dung va xu ly cac case CAN.
 *
 * Architecture:
 *
 *   User Interface
 *        |
 *        v
 *     AppCan
 *        |
 *        v
 *   IsoTpApp_SendMessage()
 *        |
 *        v
 *      ISO-TP
 *        |
 *        v
 *      CAN_IF
 *        |
 *        v
 *     CAN Driver
 */

#include "app_can.h"
#include "isotp_app.h"
#include "uart_log.h"
#include "main.h"

#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------------------------
 * Cau hinh
 *--------------------------------------------------------------------------*/

/**
 * @brief CAN ID cua Engine Status.
 *
 * Theo requirement SYS-001.
 */
#define APP_CAN_ENGINE_STATUS_ID       (0x100U)

/**
 * @brief So byte cua Engine Status.
 */
#define APP_CAN_ENGINE_STATUS_LENGTH   (8U)

/**
 * @brief So byte toi da cua mot message.
 */
#define APP_CAN_MAX_DATA_LENGTH       (64U)

/*----------------------------------------------------------------------------
 * Trang thai module
 *--------------------------------------------------------------------------*/

static uint8_t appCanInitialized = 0U;

/**
 * @brief Dem so message da yeu cau gui.
 */
static uint32_t appCanTxCount = 0U;

/**
 * @brief Dem so loi.
 */
static uint32_t appCanErrorCount = 0U;

/*----------------------------------------------------------------------------
 * Ham phu tro
 *--------------------------------------------------------------------------*/

/**
 * @brief In loi Application CAN ra UART.
 */
static void AppCan_LogError(AppCan_StatusType error)
{
    switch (error)
    {
        case APP_CAN_ERROR_NULL:
            uartlog("[APP_CAN] ERROR: NULL pointer\r\n");
            break;

        case APP_CAN_ERROR_INVALID_CASE:
            uartlog("[APP_CAN] ERROR: Invalid case\r\n");
            break;

        case APP_CAN_ERROR_NOT_INITIALIZED:
            uartlog("[APP_CAN] ERROR: Not initialized\r\n");
            break;

        case APP_CAN_ERROR_TRANSMIT:
            uartlog("[APP_CAN] ERROR: Transmit failed\r\n");
            break;

        default:
            uartlog("[APP_CAN] ERROR: Unknown error\r\n");
            break;
    }

    appCanErrorCount++;
}

/**
 * @brief In menu User Interface ra UART.
 */
void AppCan_ShowMenu(void)
{
    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("          CAN APPLICATION MENU\r\n");
    uartlog("========================================\r\n");
    uartlog(" [0] Send Engine Status\r\n");
    uartlog(" [1] Send Test Message\r\n");
    uartlog(" [2] Read CAN Frame\r\n");
    uartlog(" [3] Show CAN Status\r\n");
    uartlog("========================================\r\n");
    uartlog(" Select case: ");
}

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief Khoi tao CAN Application.
 *
 * @note CAN hardware khong duoc khoi tao tai day.
 *       Viec khoi tao CAN thuoc CAN_IF.
 */
void AppCan_Init(void)
{
    appCanInitialized = 1U;
    appCanTxCount = 0U;
    appCanErrorCount = 0U;

    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("        CAN APPLICATION INITIALIZED\r\n");
    uartlog("========================================\r\n");

    uartlog("[APP_CAN] CAN ID Engine Status: 0x100\r\n");
    uartlog("[APP_CAN] Transport: ISO-TP\r\n");
    uartlog("[APP_CAN] UART User Interface: READY\r\n");

    AppCan_ShowMenu();
}

/**
 * @brief Xu ly case do User Interface lua chon.
 */
AppCan_StatusType AppCan_HandleCase(AppCan_CaseType caseId)
{
    AppCan_StatusType status;

    if (appCanInitialized == 0U)
    {
        status = APP_CAN_ERROR_NOT_INITIALIZED;
        AppCan_LogError(status);
    }
    else
    {
        switch (caseId)
        {
            case APP_CAN_CASE_SEND_ENGINE_STATUS:
                status = AppCan_SendEngineStatus();
                break;

            case APP_CAN_CASE_SEND_TEST_MESSAGE:
            {
                uint8_t testData[3];

                testData[0] = 0x11U;
                testData[1] = 0x22U;
                testData[2] = 0x33U;

                uartlog("\r\n[UI] Selected: TEST MESSAGE\r\n");

                status = AppCan_SendTestMessage(testData, 3U);
                break;
            }

            case APP_CAN_CASE_READ_CAN_FRAME:
                /*
                 * Frame nhan duoc duoc xu ly boi CAN_IF -> IsoTpApp.
                 * Application khong doc truc tiep CAN FIFO.
                 */
                uartlog("\r\n[UI] Selected: READ CAN FRAME\r\n");
                uartlog("[APP_CAN] RX frame is handled by CAN_IF/ISO-TP\r\n");

                status = APP_CAN_OK;
                break;

            case APP_CAN_CASE_SHOW_STATUS:
                AppCan_ShowStatus();
                status = APP_CAN_OK;
                break;

            default:
                status = APP_CAN_ERROR_INVALID_CASE;
                AppCan_LogError(status);
                break;
        }
    }

    return status;
}

/**
 * @brief Gui Engine Status.
 *
 * Demo data:
 *   Byte 0-1 : Vehicle Speed
 *   Byte 2-3 : Engine RPM
 *   Byte 4-5 : Engine Temperature
 *   Byte 6-7 : Battery Voltage
 *
 * Luu y:
 * Ham nay gui message qua ISO-TP.
 * Khong goi CAN_IF_Transmit truc tiep.
 */
AppCan_StatusType AppCan_SendEngineStatus(void)
{
    AppCan_StatusType status;
    IsoTpApp_StatusType isoTpStatus;

    uint8_t engineStatus[APP_CAN_ENGINE_STATUS_LENGTH];

    /*
     * Demo values
     *
     * VehicleSpeed   = 60
     * EngineRPM      = 2000
     * EngineTemp     = 90
     * BatteryVoltage = 120
     *
     * Moi gia tri dang duoc minh hoa bang 2 byte.
     */
    engineStatus[0] = 0x00U;
    engineStatus[1] = 60U;

    engineStatus[2] = 0x07U;
    engineStatus[3] = 0xD0U;

    engineStatus[4] = 0x00U;
    engineStatus[5] = 90U;

    engineStatus[6] = 0x00U;
    engineStatus[7] = 120U;

    uartlog("\r\n");
    uartlog("[UI] Selected: SEND ENGINE STATUS\r\n");
    uartlog("[APP_CAN] ID: 0x100\r\n");

    isoTpStatus = IsoTpApp_SendMessage(
        engineStatus,
        APP_CAN_ENGINE_STATUS_LENGTH,
        HAL_GetTick()
    );

    if (isoTpStatus == ISOTP_APP_OK)
    {
        appCanTxCount++;
        uartlog("[APP_CAN] Engine Status sent successfully\r\n");
        status = APP_CAN_OK;
    }
    else
    {
        uartlog("[APP_CAN] Engine Status transmit failed\r\n");
        status = APP_CAN_ERROR_TRANSMIT;
        AppCan_LogError(status);
    }

    return status;
}

/**
 * @brief Gui test message.
 */
AppCan_StatusType AppCan_SendTestMessage(const uint8_t *data,
                                         uint16_t length)
{
    AppCan_StatusType status;
    IsoTpApp_StatusType isoTpStatus;

    if (appCanInitialized == 0U)
    {
        status = APP_CAN_ERROR_NOT_INITIALIZED;
        AppCan_LogError(status);
    }
    else if (data == NULL)
    {
        status = APP_CAN_ERROR_NULL;
        AppCan_LogError(status);
    }
    else if ((length == 0U) || (length > APP_CAN_MAX_DATA_LENGTH))
    {
        status = APP_CAN_ERROR_TRANSMIT;
        AppCan_LogError(status);
    }
    else
    {
        uartlog("\r\n[UI] Selected: SEND TEST MESSAGE\r\n");

        isoTpStatus = IsoTpApp_SendMessage(
            data,
            length,
            HAL_GetTick()
        );

        if (isoTpStatus == ISOTP_APP_OK)
        {
            appCanTxCount++;

            uartlog("[APP_CAN] Test message sent\r\n");

            status = APP_CAN_OK;
        }
        else
        {
            uartlog("[APP_CAN] Test message failed\r\n");

            status = APP_CAN_ERROR_TRANSMIT;
            AppCan_LogError(status);
        }
    }

    return status;
}

/**
 * @brief Hien thi trang thai CAN Application.
 */
void AppCan_ShowStatus(void)
{
    char buffer[128];

    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("            CAN STATUS\r\n");
    uartlog("========================================\r\n");

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " Initialized : %s\r\n",
        (appCanInitialized != 0U) ? "YES" : "NO"
    );
    uartlog(buffer);

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " TX Count    : %lu\r\n",
        (unsigned long)appCanTxCount
    );
    uartlog(buffer);

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " Error Count : %lu\r\n",
        (unsigned long)appCanErrorCount
    );
    uartlog(buffer);

    uartlog(" Transport   : ISO-TP\r\n");
    uartlog(" CAN ID      : 0x100\r\n");

    uartlog("========================================\r\n");
}