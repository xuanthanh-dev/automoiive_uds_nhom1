/*----------------------------------------------------------------------------
 * Diagnostic Tester application integration
 *
 * Transport responsibility:
 *   AppDiag -> ISO-TP -> CAN_IF
 *
 * ISO-TP is intentionally not reimplemented here.  IsoTp_OnCanFrame()
 * consumes CAN frames and IsoTp_MainFunction() handles CF timing and
 * transport timeouts.  When a complete UDS response is available,
 * AppDiag_IsoTpRxCallback() is called.
 *----------------------------------------------------------------------------*/

#include "app_diag.h"
#include "main.h"
#include "Can_if.h"
#include "did_manager.h"
#include "dtc_manager.h"
#include "uds.h"
#include "isotp.h"

#include <stdio.h>
#include <string.h>


#define APP_DIAG_SID_SESSION_CONTROL (0x10U)
#define APP_DIAG_SID_ECU_RESET       (0x11U)
#define APP_DIAG_SID_READ_DTC        (0x19U)
#define APP_DIAG_SID_READ_DID        (0x22U)
#define APP_DIAG_SID_TESTER_PRESENT  (0x3EU)
#define APP_DIAG_POSITIVE_OFFSET     (0x40U)
#define APP_DIAG_NEGATIVE_SID        (0x7FU)
#define APP_DIAG_SOFT_RESET          (0x03U)
#define APP_DIAG_DTC_BY_STATUS_MASK  (0x02U)
#define DIAG_CAN_ID_REQUEST       (0x7E0U)
#define DIAG_CAN_ID_RESPONSE      (0x7E8U)
#define DIAG_RESPONSE_TIMEOUT_MS  (10000U)

static const Diag_DidItemType diagDidItems[] =
{
    { DID_VIN,                "VIN" },
    { DID_SOFTWARE_VERSION,   "SW Info" },
    { DID_VEHICLE_SPEED,      "Vehicle Speed" },
    { DID_ENGINE_SPEED,       "Engine RPM" },
    { DID_ENGINE_TEMPERATURE, "Engine Temp" },
    { DID_BATTERY_VOLTAGE,    "Battery Voltage" }
};

#define DIAG_DID_ITEM_COUNT \
    ((uint8_t)(sizeof(diagDidItems) / sizeof(diagDidItems[0])))

extern UART_HandleTypeDef huart1;

static AppDiag_ContextType diagApp;
static Diag_MenuType diagMenu;
static Diag_ActionType diagAction;
static uint16_t diagCurrentDid;
static uint8_t diagEngineStep;
static uint8_t diagWaitingResponse;
static uint32_t diagRequestTimeMs;
static uint32_t diagResetAtMs;
static uint8_t diagResetPending;

static volatile uint8_t diagUartRxByte;
static volatile char diagUartCommand[8];
static volatile uint8_t diagUartCommandIndex;
static volatile uint8_t diagUartCommandReady;

static void AppDiag_CopyBytes(uint8_t *destination, const uint8_t *source, uint16_t length);
static AppDiag_ReturnType AppDiag_ValidatePrepare(const AppDiag_ContextType *context);
static void AppDiag_CommitRequest(AppDiag_ContextType *context, const uint8_t *request, uint16_t requestLength);

static void Diag_PrintSessionMenu(void);
static void Diag_PrintDidMenu(void);
static void Diag_StartAction(Diag_ActionType action, uint16_t did);
static uint8_t Diag_PrepareAndSend(void);
static void Diag_CheckTimeout(uint32_t now);
static uint8_t Diag_SendCanFrame(const uint8_t *frame, uint8_t dlc);
static void AppDiag_IsoTpRxCallback(const uint8_t *message, uint16_t length);
static void Diag_CompleteResponse(const uint8_t *response, uint16_t length);
static void Diag_PrintHex(const uint8_t *data, uint16_t length);
static void Diag_PrintResponse(const uint8_t *response, uint16_t length);
static void Diag_PrintDtcResponse(const uint8_t *response, uint16_t length);
static void Diag_PrintDidResponse(const uint8_t *response, uint16_t length);
static void Diag_PrintEngineStatusStep(const uint8_t *response, uint16_t length);
static void Diag_StartNextEngineStatusRead(void);
/**
 * @brief Copies one byte range.
 * @param destination Destination buffer.
 * @param source Source buffer.
 * @param length Number of bytes to copy.
 */
static void AppDiag_CopyBytes(uint8_t *destination,
                              const uint8_t *source,
                              uint16_t length)
{
    uint16_t index;

    for (index = 0U; index < length; index++)
    {
        destination[index] = source[index];
    }
}

/**
 * @brief Validates that a new request may be prepared.
 * @param context Application context.
 * @return APP_DIAG_E_OK or a state/pointer error.
 */
static AppDiag_ReturnType AppDiag_ValidatePrepare(
    const AppDiag_ContextType *context)
{
    AppDiag_ReturnType result;

    if (context == 0)
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if ((context->state != APP_DIAG_STATE_IDLE) &&
             (context->state != APP_DIAG_STATE_ERROR))
    {
        result = APP_DIAG_E_BUSY;
    }
    else
    {
        result = APP_DIAG_E_OK;
    }

    return result;
}

/**
 * @brief Commits a validated request to the context.
 * @param context Application context.
 * @param request Request bytes.
 * @param requestLength Number of bytes.
 */
static void AppDiag_CommitRequest(AppDiag_ContextType *context,
                                  const uint8_t *request,
                                  uint16_t requestLength)
{
    AppDiag_CopyBytes(context->request, request, requestLength);
    context->requestLength = requestLength;
    context->responseLength = 0U;
    context->lastNegativeResponseCode = 0U;
    context->state = APP_DIAG_STATE_REQUEST_READY;
}

/** @brief Initializes one Diagnostic Tool context. */
AppDiag_ReturnType AppDiag_Init(AppDiag_ContextType *context)
{
    AppDiag_ReturnType result;
    uint16_t index;

    if (context == 0)
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else
    {
        for (index = 0U; index < APP_DIAG_MAX_MESSAGE_LENGTH; index++)
        {
            context->request[index] = 0U;
            context->response[index] = 0U;
        }

        context->requestLength = 0U;
        context->responseLength = 0U;
        context->lastNegativeResponseCode = 0U;
        context->requestCounter = 0UL;
        context->responseCounter = 0UL;
        context->state = APP_DIAG_STATE_IDLE;
        context->initialized = 1U;
        result = APP_DIAG_E_OK;
    }

    return result;
}

/** @brief Builds a DiagnosticSessionControl request. */
AppDiag_ReturnType AppDiag_PrepareSessionControl(AppDiag_ContextType *context,
                                                 uint8_t session)
{
    AppDiag_ReturnType result;
    uint8_t request[2];

    result = AppDiag_ValidatePrepare(context);
    if (result == APP_DIAG_E_OK)
    {
        if ((session == 0U) || (session > 0x7FU))
        {
            result = APP_DIAG_E_INVALID_PARAMETER;
        }
        else
        {
            request[0] = APP_DIAG_SID_SESSION_CONTROL;
            request[1] = session;
            AppDiag_CommitRequest(context, request, 2U);
        }
    }
    else
    {
        /* Validation error is returned unchanged. */
    }

    return result;
}

/** @brief Builds the supported soft-reset request. */
AppDiag_ReturnType AppDiag_PrepareSoftReset(AppDiag_ContextType *context)
{
    AppDiag_ReturnType result;
    uint8_t request[2];

    result = AppDiag_ValidatePrepare(context);
    if (result == APP_DIAG_E_OK)
    {
        request[0] = APP_DIAG_SID_ECU_RESET;
        request[1] = APP_DIAG_SOFT_RESET;
        AppDiag_CommitRequest(context, request, 2U);
    }
    else
    {
        /* Validation error is returned unchanged. */
    }

    return result;
}

/** @brief Builds a ReadDataByIdentifier request. */
AppDiag_ReturnType AppDiag_PrepareReadDid(AppDiag_ContextType *context,
                                          uint16_t dataIdentifier)
{
    AppDiag_ReturnType result;
    uint8_t request[3];

    result = AppDiag_ValidatePrepare(context);
    if (result == APP_DIAG_E_OK)
    {
        request[0] = APP_DIAG_SID_READ_DID;
        request[1] = (uint8_t)(dataIdentifier >> 8U);
        request[2] = (uint8_t)(dataIdentifier & 0xFFU);
        AppDiag_CommitRequest(context, request, 3U);
    }
    else
    {
        /* Validation error is returned unchanged. */
    }

    return result;
}

/** @brief Builds a ReportDTCByStatusMask request. */
AppDiag_ReturnType AppDiag_PrepareReadDtc(AppDiag_ContextType *context,
                                          uint8_t statusMask)
{
    AppDiag_ReturnType result;
    uint8_t request[3];

    result = AppDiag_ValidatePrepare(context);
    if (result == APP_DIAG_E_OK)
    {
        request[0] = APP_DIAG_SID_READ_DTC;
        request[1] = APP_DIAG_DTC_BY_STATUS_MASK;
        request[2] = statusMask;
        AppDiag_CommitRequest(context, request, 3U);
    }
    else
    {
        /* Validation error is returned unchanged. */
    }

    return result;
}

/** @brief Builds a TesterPresent request. */
AppDiag_ReturnType AppDiag_PrepareTesterPresent(AppDiag_ContextType *context,
                                                uint8_t suppressResponse)
{
    AppDiag_ReturnType result;
    uint8_t request[2];

    result = AppDiag_ValidatePrepare(context);
    if (result == APP_DIAG_E_OK)
    {
        if (suppressResponse > 1U)
        {
            result = APP_DIAG_E_INVALID_PARAMETER;
        }
        else
        {
            request[0] = APP_DIAG_SID_TESTER_PRESENT;
            request[1] = (suppressResponse == 0U) ? 0x00U : 0x80U;
            AppDiag_CommitRequest(context, request, 2U);
        }
    }
    else
    {
        /* Validation error is returned unchanged. */
    }

    return result;
}

/** @brief Copies the prepared request. */
AppDiag_ReturnType AppDiag_GetPreparedRequest(
    const AppDiag_ContextType *context,
    uint8_t *buffer,
    uint16_t bufferCapacity,
    uint16_t *requestLength)
{
    AppDiag_ReturnType result;

    if ((context == 0) || (buffer == 0) || (requestLength == 0))
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if (context->state != APP_DIAG_STATE_REQUEST_READY)
    {
        result = APP_DIAG_E_NO_DATA;
    }
    else if (bufferCapacity < context->requestLength)
    {
        result = APP_DIAG_E_SMALL_BUFFER;
    }
    else
    {
        AppDiag_CopyBytes(buffer, context->request, context->requestLength);
        *requestLength = context->requestLength;
        result = APP_DIAG_E_OK;
    }

    return result;
}

/** @brief Marks a prepared request as transmitted. */
AppDiag_ReturnType AppDiag_NotifyRequestTransmitted(
    AppDiag_ContextType *context)
{
    AppDiag_ReturnType result;

    if (context == 0)
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if (context->state != APP_DIAG_STATE_REQUEST_READY)
    {
        result = APP_DIAG_E_NO_DATA;
    }
    else
    {
        context->requestCounter++;
        if ((context->requestLength == 2U) &&
            (context->request[0] == APP_DIAG_SID_TESTER_PRESENT) &&
            (context->request[1] == 0x80U))
        {
            context->requestLength = 0U;
            context->state = APP_DIAG_STATE_IDLE;
        }
        else
        {
            context->state = APP_DIAG_STATE_WAIT_RESPONSE;
        }
        result = APP_DIAG_E_OK;
    }

    return result;
}

/** @brief Validates and stores one UDS response. */
AppDiag_ReturnType AppDiag_RxIndication(AppDiag_ContextType *context,
                                        const uint8_t *response,
                                        uint16_t responseLength)
{
    AppDiag_ReturnType result;
    uint8_t expectedPositiveService;

    if ((context == 0) || (response == 0))
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if (context->state != APP_DIAG_STATE_WAIT_RESPONSE)
    {
        result = APP_DIAG_E_BUSY;
    }
    else if ((responseLength == 0U) ||
             (responseLength > APP_DIAG_MAX_MESSAGE_LENGTH))
    {
        context->state = APP_DIAG_STATE_ERROR;
        result = APP_DIAG_E_INVALID_RESPONSE;
    }
    else
    {
        expectedPositiveService = (uint8_t)(context->request[0] +
                                             APP_DIAG_POSITIVE_OFFSET);

        if ((response[0] == APP_DIAG_NEGATIVE_SID) &&
            (responseLength == 3U) &&
            (response[1] == context->request[0]))
        {
            AppDiag_CopyBytes(context->response, response, responseLength);
            context->responseLength = responseLength;
            context->lastNegativeResponseCode = response[2];
            context->responseCounter++;
            context->state = APP_DIAG_STATE_RESPONSE_READY;
            result = APP_DIAG_E_OK;
        }
        else if (response[0] == expectedPositiveService)
        {
            AppDiag_CopyBytes(context->response, response, responseLength);
            context->responseLength = responseLength;
            context->lastNegativeResponseCode = 0U;
            context->responseCounter++;
            context->state = APP_DIAG_STATE_RESPONSE_READY;
            result = APP_DIAG_E_OK;
        }
        else
        {
            context->state = APP_DIAG_STATE_ERROR;
            result = APP_DIAG_E_INVALID_RESPONSE;
        }
    }

    return result;
}

/** @brief Copies the stored response. */
AppDiag_ReturnType AppDiag_GetResponse(const AppDiag_ContextType *context,
                                       uint8_t *buffer,
                                       uint16_t bufferCapacity,
                                       uint16_t *responseLength)
{
    AppDiag_ReturnType result;

    if ((context == 0) || (buffer == 0) || (responseLength == 0))
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if (context->state != APP_DIAG_STATE_RESPONSE_READY)
    {
        result = APP_DIAG_E_NO_DATA;
    }
    else if (bufferCapacity < context->responseLength)
    {
        result = APP_DIAG_E_SMALL_BUFFER;
    }
    else
    {
        AppDiag_CopyBytes(buffer, context->response,
                          context->responseLength);
        *responseLength = context->responseLength;
        result = APP_DIAG_E_OK;
    }

    return result;
}

/** @brief Clears a consumed response. */
AppDiag_ReturnType AppDiag_ClearResponse(AppDiag_ContextType *context)
{
    AppDiag_ReturnType result;

    if (context == 0)
    {
        result = APP_DIAG_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = APP_DIAG_E_NOT_INITIALIZED;
    }
    else if ((context->state != APP_DIAG_STATE_RESPONSE_READY) &&
             (context->state != APP_DIAG_STATE_ERROR))
    {
        result = APP_DIAG_E_NO_DATA;
    }
    else
    {
        context->responseLength = 0U;
        context->requestLength = 0U;
        context->state = APP_DIAG_STATE_IDLE;
        result = APP_DIAG_E_OK;
    }

    return result;
}



void AppDiag_TesterInit(void)
{
    if (AppDiag_Init(&diagApp) != APP_DIAG_E_OK)
    {
        Error_Handler();
    }

    diagMenu = DIAG_MENU_MAIN;
    diagAction = DIAG_ACTION_NONE;
    diagCurrentDid = 0U;
    diagEngineStep = 0U;
    diagWaitingResponse = 0U;
    diagResetPending = 0U;
    diagResetAtMs = 0U;
    diagUartCommandIndex = 0U;
    diagUartCommandReady = 0U;

    if (IsoTp_Init(Diag_SendCanFrame, AppDiag_IsoTpRxCallback) != ISOTP_OK)
    {
        Error_Handler();
    }

}

void AppDiag_MainFunction(void)
{
    uint32_t now;

    now = HAL_GetTick();

    /*
     * Process CAN -> ISO-TP
     */
    Diag_ProcessCan();

    /*
     * ISO-TP timing/state machine
     */
    (void)IsoTp_MainFunction(now);

    /*
     * Diagnostic response timeout
     */
    Diag_CheckTimeout(now);

    /*
     * ECU Reset
     */
    if ((diagResetPending != 0U) &&
        ((int32_t)(now - diagResetAtMs) >= 0))
    {
        HAL_NVIC_SystemReset();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((huart != 0) && (huart->Instance == USART1))
    {
        uint8_t received;

        received = diagUartRxByte;

        if ((received == '\r') || (received == '\n'))
        {
            if (diagUartCommandIndex > 0U)
            {
                diagUartCommand[diagUartCommandIndex] = '\0';
                diagUartCommandReady = 1U;
                diagUartCommandIndex = 0U;
            }
        }
        else if (received == '\b')
        {
            if (diagUartCommandIndex > 0U)
            {
                diagUartCommandIndex--;
            }
        }
        else if (diagUartCommandIndex <
                 (sizeof(diagUartCommand) - 1U))
        {
            diagUartCommand[diagUartCommandIndex++] = (char)received;
        }
        else
        {
            /* Command buffer is full; ignore the extra character. */
        }

        (void)HAL_UART_Receive_IT(&huart1,
                                  (uint8_t *)&diagUartRxByte,
                                  1U);
    }
}

void Diag_PrintMainMenu(void)
{
    printf("\r\n========== DIAGNOSTIC MENU ==========\r\n");
    printf("1. Diagnostic Session Control\r\n");
    printf("2. ECU Reset\r\n");
    printf("3. Read DTC Information\r\n");
    printf("4. Read Data by Identifier\r\n");
    printf("5. Tester Present\r\n");
    printf("6. Quit\r\n");
    printf("Select: ");
}

static void Diag_PrintSessionMenu(void)
{
    printf("\r\n===== DIAGNOSTIC SESSION CONTROL =====\r\n");
    printf("1. Default Session\r\n");
    printf("2. Extended Session\r\n");
    printf("3. Back\r\n");
    printf("Select: ");
}

static void Diag_PrintDidMenu(void)
{
    printf("\r\n======= READ DATA BY IDENTIFIER =======\r\n");
    printf("1. Read EngineStatus\r\n");
    printf("2. Read VIN\r\n");
    printf("3. Read SW Info\r\n");
    printf("4. Read Vehicle Speed\r\n");
    printf("5. Read Engine RPM\r\n");
    printf("6. Read Engine Temp\r\n");
    printf("7. Read Battery Voltage\r\n");
    printf("8. Back\r\n");
    printf("Select: ");
}

void Diag_HandleCommand(uint8_t command)
{
    uint8_t option;

    option = command;

    if (diagWaitingResponse != 0U)
    {
        printf("\r\nDiagnostic request is still running. Please wait.\r\n");
        return;
    }

    if (diagMenu == DIAG_MENU_MAIN)
    {
        switch (option)
        {
            case 1U:
                diagMenu = DIAG_MENU_SESSION;
                Diag_PrintSessionMenu();
                break;

            case 2U:
                Diag_StartAction(
                    DIAG_ACTION_RESET,
                    0U);
                break;

            case 3U:
                Diag_StartAction(
                    DIAG_ACTION_DTC,
                    0U);
                break;

            case 4U:
                diagMenu = DIAG_MENU_DID;
                Diag_PrintDidMenu();
                break;

            case 5U:
                if (AppDiag_PrepareTesterPresent(
                        &diagApp,
                        0U) == APP_DIAG_E_OK)
                {
                    (void)Diag_PrepareAndSend();
                }
                break;

            case 6U:
                printf("\r\nDiagnostic tester stopped.\r\n");
                break;

            default:
                printf("\r\nInvalid option: %u\r\n", option);
                Diag_PrintMainMenu();
                break;
        }
    }
    else if (diagMenu == DIAG_MENU_SESSION)
    {
        switch (option)
        {
            case 1U:
                Diag_StartAction(
                    DIAG_ACTION_SESSION_DEFAULT,
                    0U);
                break;

            case 2U:
                Diag_StartAction(
                    DIAG_ACTION_SESSION_EXTENDED,
                    0U);
                break;

            case 3U:
                diagMenu = DIAG_MENU_MAIN;
                Diag_PrintMainMenu();
                break;

            default:
                printf("\r\nInvalid option: %u\r\n", option);
                Diag_PrintSessionMenu();
                break;
        }
    }
    else if (diagMenu == DIAG_MENU_DID)
    {
        switch (option)
        {
            case 1U:
                diagAction = DIAG_ACTION_ENGINE_STATUS;
                diagEngineStep = 0U;
                Diag_StartNextEngineStatusRead();
                break;

            case 2U:
            case 3U:
            case 4U:
            case 5U:
            case 6U:
            case 7U:
                diagAction = DIAG_ACTION_DID_SINGLE;

                diagCurrentDid =
                    diagDidItems[option - 2U].did;

                Diag_StartAction(
                    DIAG_ACTION_DID_SINGLE,
                    diagCurrentDid);
                break;

            case 8U:
                diagMenu = DIAG_MENU_MAIN;
                Diag_PrintMainMenu();
                break;

            default:
                printf("\r\nInvalid option: %u\r\n", option);
                Diag_PrintDidMenu();
                break;
        }
    }
}

static void Diag_StartAction(Diag_ActionType action, uint16_t did)
{
    AppDiag_ReturnType result;

    diagAction = action;
    diagCurrentDid = did;

    switch (action)
    {
        case DIAG_ACTION_SESSION_DEFAULT:
            result = AppDiag_PrepareSessionControl(
                &diagApp,
                UDS_SESSION_DEFAULT);
            break;

        case DIAG_ACTION_SESSION_EXTENDED:
            result = AppDiag_PrepareSessionControl(
                &diagApp,
                UDS_SESSION_EXTENDED);
            break;

        case DIAG_ACTION_RESET:
            result = AppDiag_PrepareSoftReset(&diagApp);
            break;

        case DIAG_ACTION_DTC:
            result = AppDiag_PrepareReadDtc(
                &diagApp,
                DTC_STATUS_ACTIVE);
            break;

        case DIAG_ACTION_DID_SINGLE:
            result = AppDiag_PrepareReadDid(&diagApp, did);
            break;

        default:
            result = APP_DIAG_E_INVALID_PARAMETER;
            break;
    }

    if (result != APP_DIAG_E_OK)
    {
        printf("\r\nUnable to prepare diagnostic request (error %d).\r\n",
               (int)result);
    }
    else
    {
        (void)Diag_PrepareAndSend();
    }
}

static void Diag_StartNextEngineStatusRead(void)
{
    static const uint16_t engineDids[] =
    {
        DID_VEHICLE_SPEED,
        DID_ENGINE_SPEED,
        DID_ENGINE_TEMPERATURE,
        DID_BATTERY_VOLTAGE
    };

    AppDiag_ReturnType result;

    /* --------------------------------------------------------
     * Check all EngineStatus DIDs completed
     * -------------------------------------------------------- */
    if (diagEngineStep >= 4U)
    {
        diagAction = DIAG_ACTION_NONE;
        diagMenu = DIAG_MENU_DID;

        printf("\r\nEngineStatus read complete.\r\n");

        Diag_PrintDidMenu();
        return;
    }

    /* --------------------------------------------------------
     * Select current DID
     * -------------------------------------------------------- */
    diagCurrentDid = engineDids[diagEngineStep];

    printf(
        "\r\n[ENGINE] Step %u, DID = 0x%04X\r\n",
        diagEngineStep,
        diagCurrentDid);

    /* --------------------------------------------------------
     * Prepare UDS ReadDataByIdentifier request
     *
     * Example:
     *   22 F1 90
     * -------------------------------------------------------- */
    result = AppDiag_PrepareReadDid(
        &diagApp,
        diagCurrentDid);

    if (result != APP_DIAG_E_OK)
    {
        printf(
            "Unable to prepare EngineStatus DID "
            "(error %d).\r\n",
            (int)result);

        diagAction = DIAG_ACTION_NONE;
        diagMenu = DIAG_MENU_DID;

        Diag_PrintDidMenu();
        return;
    }

    /* --------------------------------------------------------
     * Request prepared successfully.
     *
     * AppDiag state is now:
     *
     *     REQUEST_READY
     *
     * Diag_PrepareAndSend() will transmit it through ISO-TP.
     * -------------------------------------------------------- */
    printf(
        "[ENGINE] Request prepared, state = %d\r\n",
        (int)diagApp.state);

    if (Diag_PrepareAndSend() != 0)
    {
        printf(
            "[ENGINE] Failed to send DID 0x%04X.\r\n",
            diagCurrentDid);

        diagAction = DIAG_ACTION_NONE;
        diagMenu = DIAG_MENU_DID;

        Diag_PrintDidMenu();
        return;
    }

    printf(
        "[ENGINE] DID 0x%04X request sent.\r\n",
        diagCurrentDid);
}

static uint8_t Diag_PrepareAndSend(void)
{
    uint8_t request[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint16_t requestLength;
    IsoTp_StatusType isoStatus;
    uint8_t result;

    result = 0U;
    requestLength = 0U;

    if (AppDiag_GetPreparedRequest(
            &diagApp,
            request,
            sizeof(request),
            &requestLength) != APP_DIAG_E_OK)
    {
        printf("Unable to get prepared diagnostic request.\r\n");
    }
    else
    {
        printf("\r\n[DIAG] TX: ");
        Diag_PrintHex(request, requestLength);
        printf("\r\n");

        isoStatus = IsoTp_Send(
            request,
            requestLength,
            HAL_GetTick());

        if (isoStatus != ISOTP_OK)
        {
            printf("[DIAG] ISO-TP transmit error: %d\r\n",
                   (int)isoStatus);
        }
        else if (AppDiag_NotifyRequestTransmitted(&diagApp) !=
                 APP_DIAG_E_OK)
        {
            printf("[DIAG] AppDiag transport notification failed.\r\n");
        }
        else
        {
            /*
             * Silent TesterPresent (0x3E 0x80) has no response.
             * AppDiag_NotifyRequestTransmitted() already returned
             * the context to IDLE for this case.
             */
            if (diagApp.state == APP_DIAG_STATE_WAIT_RESPONSE)
            {
                diagWaitingResponse = 1U;
                diagRequestTimeMs = HAL_GetTick();
            }
            else
            {
                diagWaitingResponse = 0U;
                diagAction = DIAG_ACTION_NONE;
                Diag_PrintMainMenu();
            }

            result = 1U;
        }
    }

    return result;
}

static uint8_t Diag_SendCanFrame(
    const uint8_t *frame,
    uint8_t dlc)
{
    CAN_StatusTypeDef status;

    if ((frame == 0U) || (dlc != ISOTP_CAN_FRAME_SIZE))
    {
        return 1U;
    }

    printf("[DIAG CAN TX] ID=0x%03X DLC=%u DATA=",
           DIAG_CAN_ID_REQUEST,
           dlc);
    Diag_PrintHex(frame, dlc);
    printf("\r\n");

    status = CAN_IF_Transmit(
        DIAG_CAN_ID_REQUEST,
        (uint8_t *)frame,
        dlc);

    if (status == OK)
    {
        return 0U;
    }

    printf("[DIAG CAN TX] ERROR=%d\r\n", (int)status);
    return 1U;
}

void Diag_ProcessCan(void)
{
    uint32_t canId;
    uint8_t frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t dlc;
    uint32_t now;
    IsoTp_StatusType isoStatus;

    now = HAL_GetTick();

    while (CAN_IF_GetReceivedFrame(
               &canId,
               frame,
               &dlc) == OK)
    {
    	printf("[DIAG CAN RX] ID=0x%03lX DLC=%u "
    	           "DATA=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
    	           canId,
    	           dlc,
    	           frame[0],
    	           frame[1],
    	           frame[2],
    	           frame[3],
    	           frame[4],
    	           frame[5],
    	           frame[6],
    	           frame[7]);

        if (canId == DIAG_CAN_ID_RESPONSE)
        {
        	printf("[DIAG CAN RX] -> ISO-TP\r\n");
            isoStatus = IsoTp_OnCanFrame(frame, dlc, now);

            if ((isoStatus != ISOTP_OK) && (isoStatus != ISOTP_BUSY))
            {
                printf("[DIAG ISO-TP] RX error=%d, detail=%d\r\n",
                       (int)isoStatus,
                       (int)IsoTp_GetLastError());
            }
        }
        else
        {
            printf("[DIAG CAN RX] -> ignored ID\r\n");
        }
    }
}

static void AppDiag_IsoTpRxCallback(
    const uint8_t *message,
    uint16_t length)
{
    if ((message == 0U) || (length == 0U))
    {
        return;
    }

    Diag_CompleteResponse(message, length);
}

static void Diag_CompleteResponse(
    const uint8_t *response,
    uint16_t length)
{
    uint8_t responseCopy[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint16_t responseLength;

    diagWaitingResponse = 0U;

    printf("[DIAG] RX: ");
    Diag_PrintHex(response, length);
    printf("\r\n");

    if (AppDiag_RxIndication(
            &diagApp,
            response,
            length) != APP_DIAG_E_OK)
    {
        printf("[DIAG] Invalid diagnostic response.\r\n");
        (void)AppDiag_Init(&diagApp);
        diagAction = DIAG_ACTION_NONE;
        Diag_PrintMainMenu();
    }
    else if (AppDiag_GetResponse(
                 &diagApp,
                 responseCopy,
                 sizeof(responseCopy),
                 &responseLength) != APP_DIAG_E_OK)
    {
        printf("[DIAG] Cannot read stored response.\r\n");
        (void)AppDiag_Init(&diagApp);
        diagAction = DIAG_ACTION_NONE;
        Diag_PrintMainMenu();
    }
    else
    {
        if ((responseLength >= 3U) &&
            (responseCopy[0] == 0x7FU))
        {
            printf("Diagnostic request rejected. NRC = 0x%02X\r\n",
                   responseCopy[2]);
        }
        else if (diagAction == DIAG_ACTION_DTC)
        {
            Diag_PrintDtcResponse(
                responseCopy,
                responseLength);
        }
        else if (diagAction == DIAG_ACTION_DID_SINGLE)
        {
            Diag_PrintDidResponse(
                responseCopy,
                responseLength);
        }
        else if (diagAction == DIAG_ACTION_ENGINE_STATUS)
        {
            Diag_PrintEngineStatusStep(
                responseCopy,
                responseLength);
        }
        else
        {
            Diag_PrintResponse(
                responseCopy,
                responseLength);

            if (diagAction == DIAG_ACTION_RESET)
            {
                diagResetPending = 1U;
                diagResetAtMs = HAL_GetTick() + 100U;
            }
        }

        (void)AppDiag_ClearResponse(&diagApp);

        if ((diagAction == DIAG_ACTION_ENGINE_STATUS) &&
            (responseCopy[0] != 0x7FU))
        {
            diagEngineStep++;
            Diag_StartNextEngineStatusRead();
        }
        else if (diagAction != DIAG_ACTION_RESET)
        {
            diagAction = DIAG_ACTION_NONE;

            if (diagMenu == DIAG_MENU_SESSION)
            {
                Diag_PrintSessionMenu();
            }
            else if (diagMenu == DIAG_MENU_DID)
            {
                Diag_PrintDidMenu();
            }
            else
            {
                Diag_PrintMainMenu();
            }
        }
    }
}

static void Diag_CheckTimeout(uint32_t now)
{
    if ((diagWaitingResponse != 0U) &&
        ((uint32_t)(now - diagRequestTimeMs) >=
         DIAG_RESPONSE_TIMEOUT_MS))
    {
        printf("Diagnostic request timeout.\r\n");

        diagWaitingResponse = 0U;
        diagAction = DIAG_ACTION_NONE;

        IsoTp_Reset();
        (void)AppDiag_Init(&diagApp);

        if (diagMenu == DIAG_MENU_SESSION)
        {
            Diag_PrintSessionMenu();
        }
        else if (diagMenu == DIAG_MENU_DID)
        {
            Diag_PrintDidMenu();
        }
        else
        {
            Diag_PrintMainMenu();
        }
    }
}

static void Diag_PrintResponse(
    const uint8_t *response,
    uint16_t length)
{
    printf("Diagnostic response: ");
    Diag_PrintHex(response, length);
    printf("\r\n");
}

static void Diag_PrintDtcResponse(
    const uint8_t *response,
    uint16_t length)
{
    uint16_t position;
    uint8_t count;

    if ((length < 3U) ||
        (response[0] != 0x59U) ||
        (response[1] != UDS_DTC_REPORT_BY_STATUS_MASK))
    {
        printf("Invalid DTC response.\r\n");
        return;
    }

    if (length == 3U)
    {
        printf("No DTC detected.\r\n");
        return;
    }

    count = (uint8_t)((length - 3U) / 4U);
    printf("DTC Count: %u\r\n", count);

    for (position = 3U;
         (position + 3U) < length;
         position += 4U)
    {
        uint32_t code;

        code = ((uint32_t)response[position] << 16U) |
               ((uint32_t)response[position + 1U] << 8U) |
               response[position + 2U];

        printf("DTC: 0x%06lX  Status: 0x%02X\r\n",
               (unsigned long)code,
               response[position + 3U]);
    }
}

static void Diag_PrintDidResponse(
    const uint8_t *response,
    uint16_t length)
{
    if ((length < 3U) || (response[0] != 0x62U))
    {
        printf("Invalid DID response.\r\n");
        return;
    }

    if (diagCurrentDid == DID_VIN)
    {
        uint16_t index;

        printf("VIN: ");

        for (index = 3U; index < length; index++)
        {
            printf("%c", response[index]);
        }

        printf("\r\n");
    }
    else if (diagCurrentDid == DID_SOFTWARE_VERSION)
    {
        uint16_t index;

        printf("SW Info: ");

        for (index = 3U; index < length; index++)
        {
            printf("%c", response[index]);
        }

        printf("\r\n");
    }
    else if (diagCurrentDid == DID_VEHICLE_SPEED)
    {
        uint16_t speed;

        if (length >= 5U)
        {
            speed = (uint16_t)(((uint16_t)response[3] << 8U) |
                               response[4]);

            printf("Vehicle Speed: %u km/h\r\n",
                   speed);
        }
    }
    else if (diagCurrentDid == DID_ENGINE_SPEED)
    {
        uint16_t rpm;

        if (length >= 5U)
        {
            rpm = (uint16_t)(((uint16_t)response[3] << 8U) |
                             response[4]);

            printf("Engine RPM: %u rpm\r\n",
                   rpm);
        }
    }
    else if (diagCurrentDid == DID_ENGINE_TEMPERATURE)
    {
        if (length >= 4U)
        {
            printf("Engine Temp: %u C\r\n",
                   response[3]);
        }
    }
    else if (diagCurrentDid == DID_BATTERY_VOLTAGE)
    {
        if (length >= 4U)
        {
            printf("Battery Voltage: %u.%u V\r\n",
                   (unsigned int)(response[3] / 10U),
                   (unsigned int)(response[3] % 10U));
        }
    }
}

static void Diag_PrintEngineStatusStep(
    const uint8_t *response,
    uint16_t length)
{
    if ((length < 4U) || (response[0] != 0x62U))
    {
        printf("Invalid EngineStatus response.\r\n");
    }
    else if (diagCurrentDid == DID_VEHICLE_SPEED)
    {
        if (length >= 5U)
        {
            uint16_t value;

            value = (uint16_t)(((uint16_t)response[3] << 8U) |
                               response[4]);

            printf("  Vehicle Speed : %u km/h\r\n",
                   value);
        }
    }
    else if (diagCurrentDid == DID_ENGINE_SPEED)
    {
        if (length >= 5U)
        {
            uint16_t value;

            value = (uint16_t)(((uint16_t)response[3] << 8U) |
                               response[4]);

            printf("  Engine RPM    : %u rpm\r\n",
                   value);
        }
    }
    else if (diagCurrentDid == DID_ENGINE_TEMPERATURE)
    {
        printf("  Engine Temp   : %u C\r\n",
               response[3]);
    }
    else if (diagCurrentDid == DID_BATTERY_VOLTAGE)
    {
        printf("  Battery       : %u.%u V\r\n",
               (unsigned int)(response[3] / 10U),
               (unsigned int)(response[3] % 10U));
    }
}

static void Diag_PrintHex(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t index;

    if (data != 0U)
    {
        for (index = 0U; index < length; index++)
        {
            printf("%02X ", data[index]);
        }
    }
}


