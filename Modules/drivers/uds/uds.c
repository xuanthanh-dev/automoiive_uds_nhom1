/**
 * @file    uds.c
 * @brief   Implementation of the diagnostic server logic. See uds.h.
 */

#include "uds.h"
#include "did_manager.h"

/*----------------------------------------------------------------------------
 * Expected request lengths
 *--------------------------------------------------------------------------*/

/** @brief DiagnosticSessionControl carries one sub-function byte. */
#define UDS_LENGTH_SESSION_CONTROL      (2u)

/** @brief ECUReset carries one sub-function byte. */
#define UDS_LENGTH_ECU_RESET            (2u)

/** @brief ReadDTCInformation carries a sub-function and a status mask. */
#define UDS_LENGTH_READ_DTC             (3u)

/** @brief ReadDataByIdentifier carries a two byte identifier. */
#define UDS_LENGTH_READ_DATA_BY_ID      (3u)

/** @brief TesterPresent carries one sub-function byte. */
#define UDS_LENGTH_TESTER_PRESENT       (2u)

/** @brief Header length of a ReadDataByIdentifier positive response. */
#define UDS_HEADER_READ_DATA_BY_ID      (3u)

/** @brief Header length of a ReadDTCInformation positive response. */
#define UDS_HEADER_READ_DTC             (3u)

/** @brief Length of every negative response. */
#define UDS_LENGTH_NEGATIVE_RESPONSE    (3u)

/*----------------------------------------------------------------------------
 * Local helpers
 *--------------------------------------------------------------------------*/

/**
 * @brief   Build a negative response and count it.
 *
 * @param[in,out] context           Session context, for the error counter.
 * @param[in]     serviceId         Service identifier that was rejected.
 * @param[in]     responseCode      Negative response code to report.
 * @param[out]    response          Buffer receiving the response.
 * @param[in]     responseCapacity  Size of the response buffer.
 * @param[out]    responseLength    Number of response bytes produced.
 *
 * @return  UDS_E_NEGATIVE on success, UDS_E_SMALL_BUFFER if there is no room.
 */
static Uds_ReturnType Uds_BuildNegativeResponse(Uds_ContextType *context,
                                                uint8_t          serviceId,
                                                uint8_t          responseCode,
                                                uint8_t         *response,
                                                uint16_t         responseCapacity,
                                                uint16_t        *responseLength)
{
    Uds_ReturnType result;

    if (responseCapacity < (uint16_t)UDS_LENGTH_NEGATIVE_RESPONSE)
    {
        result = UDS_E_SMALL_BUFFER;
    }
    else
    {
        response[0] = UDS_NEGATIVE_RESPONSE_SID;
        response[1] = serviceId;
        response[2] = responseCode;

        *responseLength = (uint16_t)UDS_LENGTH_NEGATIVE_RESPONSE;

        context->negativeCounter++;

        result = UDS_E_NEGATIVE;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Service 0x10 DiagnosticSessionControl
 *--------------------------------------------------------------------------*/

/**
 * @brief   Handle a DiagnosticSessionControl request.
 *
 * @details Switches the active session and echoes the sub-function back, as
 *          required by SWR-UDS-002. Only the default and extended sessions
 *          are supported by this project.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK, UDS_E_NEGATIVE or UDS_E_SMALL_BUFFER.
 */
static Uds_ReturnType Uds_HandleSessionControl(
                                    const Uds_EnvironmentType *environment,
                                    const uint8_t             *request,
                                    uint16_t                   requestLength,
                                    uint8_t                   *response,
                                    uint16_t                   responseCapacity,
                                    uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    uint8_t        subFunction;

    if (requestLength != (uint16_t)UDS_LENGTH_SESSION_CONTROL)
    {
        result = Uds_BuildNegativeResponse(environment->udsContext,
                                           UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                           UDS_NRC_INCORRECT_LENGTH,
                                           response, responseCapacity,
                                           responseLength);
    }
    else
    {
        subFunction = request[1];

        if ((subFunction != UDS_SESSION_DEFAULT) &&
            (subFunction != UDS_SESSION_EXTENDED))
        {
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                        UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED,
                                        response, responseCapacity,
                                        responseLength);
        }
        else if (responseCapacity < 2u)
        {
            result = UDS_E_SMALL_BUFFER;
        }
        else
        {
            environment->udsContext->currentSession = subFunction;

            response[0] = (uint8_t)(UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
                                    UDS_POSITIVE_RESPONSE_OFFSET);
            response[1] = subFunction;

            *responseLength = 2u;

            result = UDS_E_OK;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Service 0x11 ECUReset
 *--------------------------------------------------------------------------*/

/**
 * @brief   Handle an ECUReset request.
 *
 * @details Only the soft reset sub-function is supported by this project.
 *          The handler does not reset anything. It raises a flag that the
 *          caller reads once the response has really been transmitted,
 *          because resetting here would destroy the response before it
 *          reaches the tester.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK, UDS_E_NEGATIVE or UDS_E_SMALL_BUFFER.
 */
static Uds_ReturnType Uds_HandleEcuReset(
                                    const Uds_EnvironmentType *environment,
                                    const uint8_t             *request,
                                    uint16_t                   requestLength,
                                    uint8_t                   *response,
                                    uint16_t                   responseCapacity,
                                    uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    uint8_t        subFunction;

    if (requestLength != (uint16_t)UDS_LENGTH_ECU_RESET)
    {
        result = Uds_BuildNegativeResponse(environment->udsContext,
                                           UDS_SID_ECU_RESET,
                                           UDS_NRC_INCORRECT_LENGTH,
                                           response, responseCapacity,
                                           responseLength);
    }
    else
    {
        subFunction = request[1];

        if (subFunction != UDS_RESET_SOFT)
        {
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_ECU_RESET,
                                        UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED,
                                        response, responseCapacity,
                                        responseLength);
        }
        else if (responseCapacity < 2u)
        {
            result = UDS_E_SMALL_BUFFER;
        }
        else
        {
            /* Raise the flag only. The caller performs the reset later. */
            environment->udsContext->resetPending = 1u;

            response[0] = (uint8_t)(UDS_SID_ECU_RESET +
                                    UDS_POSITIVE_RESPONSE_OFFSET);
            response[1] = subFunction;

            *responseLength = 2u;

            result = UDS_E_OK;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Service 0x19 ReadDTCInformation
 *--------------------------------------------------------------------------*/

/**
 * @brief   Handle a ReadDTCInformation request.
 *
 * @details Only the report by status mask sub-function is supported. The
 *          trouble codes themselves are produced by the trouble code manager,
 *          which this handler simply asks for a serialised block.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK, UDS_E_NEGATIVE or UDS_E_SMALL_BUFFER.
 */
static Uds_ReturnType Uds_HandleReadDtcInformation(
                                    const Uds_EnvironmentType *environment,
                                    const uint8_t             *request,
                                    uint16_t                   requestLength,
                                    uint8_t                   *response,
                                    uint16_t                   responseCapacity,
                                    uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    Dtc_ReturnType dtcResult;
    uint8_t        subFunction;
    uint8_t        statusMask;
    uint8_t        dtcLength;

    dtcLength = 0u;

    if (requestLength != (uint16_t)UDS_LENGTH_READ_DTC)
    {
        result = Uds_BuildNegativeResponse(environment->udsContext,
                                           UDS_SID_READ_DTC_INFORMATION,
                                           UDS_NRC_INCORRECT_LENGTH,
                                           response, responseCapacity,
                                           responseLength);
    }
    else
    {
        subFunction = request[1];
        statusMask  = request[2];

        if (subFunction != UDS_DTC_REPORT_BY_STATUS_MASK)
        {
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_READ_DTC_INFORMATION,
                                        UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED,
                                        response, responseCapacity,
                                        responseLength);
        }
        else if (responseCapacity < (uint16_t)UDS_HEADER_READ_DTC)
        {
            result = UDS_E_SMALL_BUFFER;
        }
        else
        {
            /*
             * The header is written first so that the trouble code manager
             * can serialise straight into the remaining space, avoiding an
             * intermediate copy.
             */
            response[0] = (uint8_t)(UDS_SID_READ_DTC_INFORMATION +
                                    UDS_POSITIVE_RESPONSE_OFFSET);
            response[1] = subFunction;
            response[2] = DTC_STATUS_ACTIVE;

            if ((statusMask & DTC_STATUS_ACTIVE) != 0U)
            {
                dtcResult = DtcManager_SerialiseActive(
                                environment->dtcContext,
                                &response[UDS_HEADER_READ_DTC],
                                (uint8_t)(responseCapacity -
                                          UDS_HEADER_READ_DTC),
                                &dtcLength);
            }
            else
            {
                dtcResult = DTC_E_OK;
                dtcLength = 0U;
            }

            if (dtcResult == DTC_E_OK)
            {
                *responseLength = (uint16_t)(UDS_HEADER_READ_DTC + dtcLength);
                result = UDS_E_OK;
            }
            else
            {
                /*
                 * The codes exist but cannot be delivered right now, which is
                 * exactly what conditions not correct describes.
                 */
                result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_READ_DTC_INFORMATION,
                                        UDS_NRC_CONDITIONS_NOT_CORRECT,
                                        response, responseCapacity,
                                        responseLength);
            }
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Service 0x22 ReadDataByIdentifier
 *--------------------------------------------------------------------------*/

/**
 * @brief   Handle a ReadDataByIdentifier request.
 *
 * @details The two identifier bytes are combined most significant byte first,
 *          then handed to the data identifier manager. An unknown identifier
 *          produces request out of range, as required by SYS-007.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK, UDS_E_NEGATIVE or UDS_E_SMALL_BUFFER.
 */
static Uds_ReturnType Uds_HandleReadDataByIdentifier(
                                    const Uds_EnvironmentType *environment,
                                    const uint8_t             *request,
                                    uint16_t                   requestLength,
                                    uint8_t                   *response,
                                    uint16_t                   responseCapacity,
                                    uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    Did_ReturnType didResult;
    uint16_t       dataIdentifier;
    uint8_t        dataLength;

    dataLength = 0u;

    if (requestLength != (uint16_t)UDS_LENGTH_READ_DATA_BY_ID)
    {
        result = Uds_BuildNegativeResponse(environment->udsContext,
                                           UDS_SID_READ_DATA_BY_IDENTIFIER,
                                           UDS_NRC_INCORRECT_LENGTH,
                                           response, responseCapacity,
                                           responseLength);
    }
    else if (responseCapacity < (uint16_t)UDS_HEADER_READ_DATA_BY_ID)
    {
        result = UDS_E_SMALL_BUFFER;
    }
    else
    {
        dataIdentifier = (uint16_t)(((uint16_t)request[1] << 8u) |
                                    (uint16_t)request[2]);

        /* Header first so the manager can write straight after it */
        response[0] = (uint8_t)(UDS_SID_READ_DATA_BY_IDENTIFIER +
                                UDS_POSITIVE_RESPONSE_OFFSET);
        response[1] = request[1];
        response[2] = request[2];

        didResult = DidManager_ReadData(
                        dataIdentifier,
                        environment->appContext,
                        &response[UDS_HEADER_READ_DATA_BY_ID],
                        (uint8_t)(responseCapacity - UDS_HEADER_READ_DATA_BY_ID),
                        &dataLength);

        if (didResult == DID_E_OK)
        {
            *responseLength = (uint16_t)(UDS_HEADER_READ_DATA_BY_ID +
                                         dataLength);
            result = UDS_E_OK;
        }
        else if (didResult == DID_E_NOT_SUPPORTED)
        {
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_READ_DATA_BY_IDENTIFIER,
                                        UDS_NRC_REQUEST_OUT_OF_RANGE,
                                        response, responseCapacity,
                                        responseLength);
        }
        else
        {
            /*
             * The identifier is known but the value cannot be produced right
             * now, either because the buffer is too small or the application
             * layer refused the read.
             */
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_READ_DATA_BY_IDENTIFIER,
                                        UDS_NRC_CONDITIONS_NOT_CORRECT,
                                        response, responseCapacity,
                                        responseLength);
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Service 0x3E TesterPresent
 *--------------------------------------------------------------------------*/

/**
 * @brief   Handle a TesterPresent request.
 *
 * @details The silent form of the sub-function requires the server to stay
 *          quiet, which is reported as a response length of zero. That is a
 *          normal outcome, not an error.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK, UDS_E_NO_RESPONSE, UDS_E_NEGATIVE or UDS_E_SMALL_BUFFER.
 */
static Uds_ReturnType Uds_HandleTesterPresent(
                                    const Uds_EnvironmentType *environment,
                                    const uint8_t             *request,
                                    uint16_t                   requestLength,
                                    uint8_t                   *response,
                                    uint16_t                   responseCapacity,
                                    uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    uint8_t        subFunction;

    if (requestLength != (uint16_t)UDS_LENGTH_TESTER_PRESENT)
    {
        result = Uds_BuildNegativeResponse(environment->udsContext,
                                           UDS_SID_TESTER_PRESENT,
                                           UDS_NRC_INCORRECT_LENGTH,
                                           response, responseCapacity,
                                           responseLength);
    }
    else
    {
        subFunction = request[1];

        if (subFunction == UDS_TESTER_PRESENT_SILENT)
        {
            /* The standard requires no answer for this sub-function */
            *responseLength = 0u;
            result = UDS_E_NO_RESPONSE;
        }
        else if (subFunction != UDS_TESTER_PRESENT_RESPOND)
        {
            result = Uds_BuildNegativeResponse(
                                        environment->udsContext,
                                        UDS_SID_TESTER_PRESENT,
                                        UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED,
                                        response, responseCapacity,
                                        responseLength);
        }
        else if (responseCapacity < 2u)
        {
            result = UDS_E_SMALL_BUFFER;
        }
        else
        {
            response[0] = (uint8_t)(UDS_SID_TESTER_PRESENT +
                                    UDS_POSITIVE_RESPONSE_OFFSET);
            response[1] = subFunction;

            *responseLength = 2u;

            result = UDS_E_OK;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Initialise the diagnostic server. See uds.h.
 */
Uds_ReturnType Uds_Init(Uds_ContextType *context)
{
    Uds_ReturnType result;

    if (context == 0)
    {
        result = UDS_E_NULL_PTR;
    }
    else
    {
        context->currentSession  = UDS_SESSION_DEFAULT;
        context->resetPending    = 0u;
        context->requestCounter  = 0u;
        context->negativeCounter = 0u;
        context->initialised     = 1u;

        result = UDS_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Request processing
 *--------------------------------------------------------------------------*/

/**
 * @brief   Process one diagnostic request. See uds.h.
 */
Uds_ReturnType Uds_ProcessRequest(const Uds_EnvironmentType *environment,
                                  const uint8_t             *request,
                                  uint16_t                   requestLength,
                                  uint8_t                   *response,
                                  uint16_t                   responseCapacity,
                                  uint16_t                  *responseLength)
{
    Uds_ReturnType result;
    uint8_t        serviceId;

    if ((environment == 0) || (request == 0) ||
        (response == 0) || (responseLength == 0))
    {
        result = UDS_E_NULL_PTR;
    }
    else if ((environment->udsContext == 0) ||
             (environment->appContext == 0) ||
             (environment->dtcContext == 0))
    {
        result = UDS_E_NULL_PTR;
    }
    else if (environment->udsContext->initialised == 0u)
    {
        result = UDS_E_NOT_INIT;
    }
    else if ((requestLength == 0u) ||
             (requestLength > (uint16_t)UDS_MAX_REQUEST_SIZE))
    {
        result = UDS_E_SMALL_BUFFER;
    }
    else
    {
        environment->udsContext->requestCounter++;

        serviceId = request[0];

        switch (serviceId)
        {
            case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
                result = Uds_HandleSessionControl(environment, request,
                                                  requestLength, response,
                                                  responseCapacity,
                                                  responseLength);
                break;

            case UDS_SID_ECU_RESET:
                result = Uds_HandleEcuReset(environment, request,
                                            requestLength, response,
                                            responseCapacity, responseLength);
                break;

            case UDS_SID_READ_DTC_INFORMATION:
                result = Uds_HandleReadDtcInformation(environment, request,
                                                      requestLength, response,
                                                      responseCapacity,
                                                      responseLength);
                break;

            case UDS_SID_READ_DATA_BY_IDENTIFIER:
                result = Uds_HandleReadDataByIdentifier(environment, request,
                                                        requestLength, response,
                                                        responseCapacity,
                                                        responseLength);
                break;

            case UDS_SID_TESTER_PRESENT:
                result = Uds_HandleTesterPresent(environment, request,
                                                 requestLength, response,
                                                 responseCapacity,
                                                 responseLength);
                break;

            default:
                result = Uds_BuildNegativeResponse(
                                            environment->udsContext,
                                            serviceId,
                                            UDS_NRC_SERVICE_NOT_SUPPORTED,
                                            response, responseCapacity,
                                            responseLength);
                break;
        }
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Session and reset state
 *--------------------------------------------------------------------------*/

/**
 * @brief   Read the active session. See uds.h.
 */
Uds_ReturnType Uds_GetCurrentSession(const Uds_ContextType *context,
                                     uint8_t               *session)
{
    Uds_ReturnType result;

    if ((context == 0) || (session == 0))
    {
        result = UDS_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = UDS_E_NOT_INIT;
    }
    else
    {
        *session = context->currentSession;
        result   = UDS_E_OK;
    }

    return result;
}

/**
 * @brief   Report a pending reset. See uds.h.
 */
Uds_ReturnType Uds_IsResetPending(const Uds_ContextType *context,
                                  uint8_t               *pending)
{
    Uds_ReturnType result;

    if ((context == 0) || (pending == 0))
    {
        result = UDS_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = UDS_E_NOT_INIT;
    }
    else
    {
        *pending = context->resetPending;
        result   = UDS_E_OK;
    }

    return result;
}

/**
 * @brief   Clear the pending reset flag. See uds.h.
 */
Uds_ReturnType Uds_ClearResetPending(Uds_ContextType *context)
{
    Uds_ReturnType result;

    if (context == 0)
    {
        result = UDS_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = UDS_E_NOT_INIT;
    }
    else
    {
        context->resetPending = 0u;
        result = UDS_E_OK;
    }

    return result;
}

/**
 * @brief Performs the volatile software reset defined for this project.
 */
Uds_ReturnType Uds_ExecuteSoftReset(Uds_ContextType *context)
{
    Uds_ReturnType result;

    if (context == 0)
    {
        result = UDS_E_NULL_PTR;
    }
    else if (context->initialised == 0U)
    {
        result = UDS_E_NOT_INIT;
    }
    else if (context->resetPending == 0U)
    {
        result = UDS_E_NO_RESPONSE;
    }
    else
    {
        context->currentSession = UDS_SESSION_DEFAULT;
        context->resetPending = 0U;
        result = UDS_E_OK;
    }

    return result;
}
