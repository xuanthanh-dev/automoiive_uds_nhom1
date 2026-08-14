/**
 * @file    app_diag.c
 * @brief   Application Diagnostic - giao dien nguoi dung cho UDS.
 *
 * Architecture:
 *
 *   User Interface
 *        |
 *        v
 *     AppDiag
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
 *       CAN
 */

#include "app_diag.h"
#include "isotp_app.h"
#include "uart_log.h"

#include <stdio.h>

/*----------------------------------------------------------------------------
 * Cau hinh
 *--------------------------------------------------------------------------*/

#define APP_DIAG_MAX_REQUEST_LENGTH    (8U)

/*----------------------------------------------------------------------------
 * Trang thai module
 *--------------------------------------------------------------------------*/

static uint8_t appDiagInitialized = 0U;
static uint32_t appDiagTxCount = 0U;
static uint32_t appDiagErrorCount = 0U;

/*----------------------------------------------------------------------------
 * Ham phu tro
 *--------------------------------------------------------------------------*/

/**
 * @brief In loi Application Diagnostic ra UART.
 */
static void AppDiag_LogError(AppDiag_StatusType error)
{
    switch (error)
    {
        case APP_DIAG_ERROR_NULL:
            uartlog("[APP_DIAG] ERROR: NULL pointer\r\n");
            break;

        case APP_DIAG_ERROR_INVALID_CASE:
            uartlog("[APP_DIAG] ERROR: Invalid case\r\n");
            break;

        case APP_DIAG_ERROR_NOT_INITIALIZED:
            uartlog("[APP_DIAG] ERROR: Not initialized\r\n");
            break;

        case APP_DIAG_ERROR_TRANSMIT:
            uartlog("[APP_DIAG] ERROR: Transmit failed\r\n");
            break;

        default:
            uartlog("[APP_DIAG] ERROR: Unknown error\r\n");
            break;
    }

    appDiagErrorCount++;
}

/**
 * @brief Gui mot UDS request qua ISO-TP.
 *
 * @note Day la diem duy nhat AppDiag giao tiep voi tang ISO-TP.
 */
static AppDiag_StatusType AppDiag_SendRequest(const uint8_t *request,
                                              uint16_t length)
{
    AppDiag_StatusType status;
    IsoTpApp_StatusType isoTpStatus;

    if (appDiagInitialized == 0U)
    {
        status = APP_DIAG_ERROR_NOT_INITIALIZED;
        AppDiag_LogError(status);
    }
    else if (request == NULL)
    {
        status = APP_DIAG_ERROR_NULL;
        AppDiag_LogError(status);
    }
    else if ((length == 0U) ||
             (length > APP_DIAG_MAX_REQUEST_LENGTH))
    {
        status = APP_DIAG_ERROR_TRANSMIT;
        AppDiag_LogError(status);
    }
    else
    {
        isoTpStatus = IsoTpApp_SendMessage(
            request,
            length,
            HAL_GetTick()
        );

        if (isoTpStatus == ISOTP_APP_OK)
        {
            appDiagTxCount++;
            status = APP_DIAG_OK;
        }
        else
        {
            status = APP_DIAG_ERROR_TRANSMIT;
            AppDiag_LogError(status);
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief Khoi tao Application Diagnostic.
 */
void AppDiag_Init(void)
{
    appDiagInitialized = 1U;
    appDiagTxCount = 0U;
    appDiagErrorCount = 0U;

    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("     DIAGNOSTIC APPLICATION READY\r\n");
    uartlog("========================================\r\n");

    AppDiag_ShowMenu();
}

/**
 * @brief Hien thi menu Diagnostic qua UART.
 */
void AppDiag_ShowMenu(void)
{
    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("       UDS DIAGNOSTIC MENU\r\n");
    uartlog("========================================\r\n");
    uartlog(" [0] Read VIN\r\n");
    uartlog(" [1] Read Software Version\r\n");
    uartlog(" [2] ECU Reset\r\n");
    uartlog(" [3] Tester Present\r\n");
    uartlog(" [4] Show Diagnostic Status\r\n");
    uartlog("========================================\r\n");
    uartlog(" Select case: ");
}

/**
 * @brief Xu ly case diagnostic tu User Interface.
 */
AppDiag_StatusType AppDiag_HandleCase(AppDiag_CaseType caseId)
{
    AppDiag_StatusType status;

    if (appDiagInitialized == 0U)
    {
        status = APP_DIAG_ERROR_NOT_INITIALIZED;
        AppDiag_LogError(status);
    }
    else
    {
        switch (caseId)
        {
            case APP_DIAG_CASE_READ_VIN:
                status = AppDiag_ReadVIN();
                break;

            case APP_DIAG_CASE_READ_SW_VERSION:
                status = AppDiag_ReadSWVersion();
                break;

            case APP_DIAG_CASE_ECU_RESET:
                status = AppDiag_ECUReset();
                break;

            case APP_DIAG_CASE_TESTER_PRESENT:
                status = AppDiag_TesterPresent();
                break;

            default:
                status = APP_DIAG_ERROR_INVALID_CASE;
                AppDiag_LogError(status);
                break;
        }
    }

    return status;
}

/**
 * @brief Read VIN - DID 0xF190.
 *
 * Request:
 *     22 F1 90
 *
 * Expected positive response:
 *     62 F1 90 <VIN>
 */
AppDiag_StatusType AppDiag_ReadVIN(void)
{
    AppDiag_StatusType status;

    const uint8_t request[] =
    {
        0x22U,
        0xF1U,
        0x90U
    };

    uartlog("\r\n");
    uartlog("[UI] Selected: READ VIN\r\n");
    uartlog("[UDS] SID: 0x22 | DID: 0xF190\r\n");
    uartlog("[UDS] TX: 22 F1 90\r\n");

    status = AppDiag_SendRequest(
        request,
        sizeof(request)
    );

    if (status == APP_DIAG_OK)
    {
        uartlog("[UDS] Request sent successfully\r\n");
    }
    else
    {
        uartlog("[UDS] Request failed\r\n");
    }

    return status;
}

/**
 * @brief Read Software Version - DID 0xF187.
 *
 * Request:
 *     22 F1 87
 *
 * Expected positive response:
 *     62 F1 87 <Software Version>
 */
AppDiag_StatusType AppDiag_ReadSWVersion(void)
{
    AppDiag_StatusType status;

    const uint8_t request[] =
    {
        0x22U,
        0xF1U,
        0x87U
    };

    uartlog("\r\n");
    uartlog("[UI] Selected: READ SOFTWARE VERSION\r\n");
    uartlog("[UDS] SID: 0x22 | DID: 0xF187\r\n");
    uartlog("[UDS] TX: 22 F1 87\r\n");

    status = AppDiag_SendRequest(
        request,
        sizeof(request)
    );

    if (status == APP_DIAG_OK)
    {
        uartlog("[UDS] Request sent successfully\r\n");
    }
    else
    {
        uartlog("[UDS] Request failed\r\n");
    }

    return status;
}

/**
 * @brief ECU Reset - SID 0x11, sub-function 0x01.
 *
 * Request:
 *     11 01
 */
AppDiag_StatusType AppDiag_ECUReset(void)
{
    AppDiag_StatusType status;

    const uint8_t request[] =
    {
        0x11U,
        0x01U
    };

    uartlog("\r\n");
    uartlog("[UI] Selected: ECU RESET\r\n");
    uartlog("[UDS] SID: 0x11 | Sub-function: 0x01\r\n");
    uartlog("[UDS] TX: 11 01\r\n");

    status = AppDiag_SendRequest(
        request,
        sizeof(request)
    );

    if (status == APP_DIAG_OK)
    {
        uartlog("[UDS] ECU Reset request sent\r\n");
    }
    else
    {
        uartlog("[UDS] ECU Reset request failed\r\n");
    }

    return status;
}

/**
 * @brief Tester Present - SID 0x3E, sub-function 0x00.
 *
 * Request:
 *     3E 00
 */
AppDiag_StatusType AppDiag_TesterPresent(void)
{
    AppDiag_StatusType status;

    const uint8_t request[] =
    {
        0x3EU,
        0x00U
    };

    uartlog("\r\n");
    uartlog("[UI] Selected: TESTER PRESENT\r\n");
    uartlog("[UDS] SID: 0x3E\r\n");
    uartlog("[UDS] TX: 3E 00\r\n");

    status = AppDiag_SendRequest(
        request,
        sizeof(request)
    );

    if (status == APP_DIAG_OK)
    {
        uartlog("[UDS] Tester Present request sent\r\n");
    }
    else
    {
        uartlog("[UDS] Tester Present request failed\r\n");
    }

    return status;
}

/**
 * @brief Hien thi trang thai Diagnostic Application.
 */
void AppDiag_ShowStatus(void)
{
    char buffer[128];

    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("       DIAGNOSTIC APPLICATION STATUS\r\n");
    uartlog("========================================\r\n");

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " Initialized : %s\r\n",
        (appDiagInitialized != 0U) ? "YES" : "NO"
    );
    uartlog(buffer);

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " TX Count    : %lu\r\n",
        (unsigned long)appDiagTxCount
    );
    uartlog(buffer);

    (void)snprintf(
        buffer,
        sizeof(buffer),
        " Error Count : %lu\r\n",
        (unsigned long)appDiagErrorCount
    );
    uartlog(buffer);

    uartlog(" Transport   : ISO-TP\r\n");
    uartlog(" VIN DID     : 0xF190\r\n");
    uartlog(" SW DID      : 0xF187\r\n");

    uartlog("========================================\r\n");
}