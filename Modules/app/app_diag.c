/**
 * @file app_diag.c
 * @brief Diagnostic Tool application implementation.
 */

#include "app_diag.h"

#define APP_DIAG_SID_SESSION_CONTROL (0x10U)
#define APP_DIAG_SID_ECU_RESET       (0x11U)
#define APP_DIAG_SID_READ_DTC        (0x19U)
#define APP_DIAG_SID_READ_DID        (0x22U)
#define APP_DIAG_SID_TESTER_PRESENT  (0x3EU)
#define APP_DIAG_POSITIVE_OFFSET     (0x40U)
#define APP_DIAG_NEGATIVE_SID        (0x7FU)
#define APP_DIAG_SOFT_RESET          (0x03U)
#define APP_DIAG_DTC_BY_STATUS_MASK  (0x02U)

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
