/**
 * @file app_diag.h
 * @brief Diagnostic Tool application request and response manager.
 */

#ifndef APP_DIAG_H
#define APP_DIAG_H

#include <stdint.h>

#define APP_DIAG_MAX_MESSAGE_LENGTH (64U)

/** Return codes owned by App_Diag. */
typedef enum
{
    APP_DIAG_E_OK = 0,
    APP_DIAG_E_NULL_PTR,
    APP_DIAG_E_NOT_INITIALIZED,
    APP_DIAG_E_BUSY,
    APP_DIAG_E_INVALID_PARAMETER,
    APP_DIAG_E_INVALID_RESPONSE,
    APP_DIAG_E_SMALL_BUFFER,
    APP_DIAG_E_NO_DATA
} AppDiag_ReturnType;

/** Request lifecycle states. */
typedef enum
{
    APP_DIAG_STATE_UNINITIALIZED = 0,
    APP_DIAG_STATE_IDLE,
    APP_DIAG_STATE_REQUEST_READY,
    APP_DIAG_STATE_WAIT_RESPONSE,
    APP_DIAG_STATE_RESPONSE_READY,
    APP_DIAG_STATE_ERROR
} AppDiag_StateType;

/** Complete state of the Diagnostic Tool application. */
typedef struct
{
    AppDiag_StateType state;
    uint8_t request[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint16_t requestLength;
    uint8_t response[APP_DIAG_MAX_MESSAGE_LENGTH];
    uint16_t responseLength;
    uint8_t lastNegativeResponseCode;
    uint32_t requestCounter;
    uint32_t responseCounter;
    uint8_t initialized;
} AppDiag_ContextType;

/**
 * @brief Initializes a Diagnostic Tool application context.
 * @param context Caller-owned context storage.
 * @return APP_DIAG_E_OK or APP_DIAG_E_NULL_PTR.
 */
AppDiag_ReturnType AppDiag_Init(AppDiag_ContextType *context);

/**
 * @brief Builds service 0x10 DiagnosticSessionControl.
 * @param context Initialized application context.
 * @param session Requested nonzero UDS session value.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_PrepareSessionControl(AppDiag_ContextType *context,
                                                 uint8_t session);

/**
 * @brief Builds service 0x11 with the supported soft-reset sub-function.
 * @param context Initialized application context.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_PrepareSoftReset(AppDiag_ContextType *context);

/**
 * @brief Builds service 0x22 ReadDataByIdentifier.
 * @param context Initialized application context.
 * @param dataIdentifier Requested 16-bit DID.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_PrepareReadDid(AppDiag_ContextType *context,
                                          uint16_t dataIdentifier);

/**
 * @brief Builds service 0x19 ReportDTCByStatusMask.
 * @param context Initialized application context.
 * @param statusMask Requested DTC status mask.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_PrepareReadDtc(AppDiag_ContextType *context,
                                          uint8_t statusMask);

/**
 * @brief Builds service 0x3E TesterPresent.
 * @param context Initialized application context.
 * @param suppressResponse Zero for a response or one to suppress it.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_PrepareTesterPresent(AppDiag_ContextType *context,
                                                uint8_t suppressResponse);

/**
 * @brief Copies the prepared request for transport transmission.
 * @param context Initialized application context.
 * @param buffer Destination request buffer.
 * @param bufferCapacity Destination capacity.
 * @param requestLength Destination for the request length.
 * @return APP_DIAG_E_OK or an application error.
 */
AppDiag_ReturnType AppDiag_GetPreparedRequest(
    const AppDiag_ContextType *context,
    uint8_t *buffer,
    uint16_t bufferCapacity,
    uint16_t *requestLength);

/**
 * @brief Marks the prepared request as accepted by the transport layer.
 * @param context Initialized application context.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_NotifyRequestTransmitted(
    AppDiag_ContextType *context);

/**
 * @brief Validates and stores a complete UDS response.
 * @param context Initialized application context waiting for a response.
 * @param response Complete positive or negative UDS response.
 * @param responseLength Number of response bytes.
 * @return APP_DIAG_E_OK or a validation/response error.
 */
AppDiag_ReturnType AppDiag_RxIndication(AppDiag_ContextType *context,
                                        const uint8_t *response,
                                        uint16_t responseLength);

/**
 * @brief Copies the stored UDS response.
 * @param context Initialized application context.
 * @param buffer Destination response buffer.
 * @param bufferCapacity Destination capacity.
 * @param responseLength Destination for the response length.
 * @return APP_DIAG_E_OK or a validation/state/buffer error.
 */
AppDiag_ReturnType AppDiag_GetResponse(const AppDiag_ContextType *context,
                                       uint8_t *buffer,
                                       uint16_t bufferCapacity,
                                       uint16_t *responseLength);

/**
 * @brief Releases the stored response and returns to idle.
 * @param context Initialized application context.
 * @return APP_DIAG_E_OK or a validation/state error.
 */
AppDiag_ReturnType AppDiag_ClearResponse(AppDiag_ContextType *context);

#endif /* APP_DIAG_H */
