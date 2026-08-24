/**
 * @file    uds.h
 * @brief   Unified Diagnostic Services server logic.
 *
 * @details Implements SWR-UDS-001 to SWR-UDS-003. The module receives a
 *          complete diagnostic request as a byte array and produces a
 *          complete diagnostic response as a byte array.
 *
 *          It knows nothing about CAN frames, identifiers or the transport
 *          protocol. Whether the response fits in one frame or needs
 *          segmentation is decided by the transport/glue below it. This
 *          separation is what allows the module to be unit tested on a host
 *          machine with no hardware at all.
 *
 *          Data values and trouble codes are not stored here either. They are
 *          fetched from the data identifier manager and the trouble code
 *          manager through the environment structure.
 *
 * @note    Session state lives in Uds_ContextType, allocated by the caller.
 */

#ifndef UDS_H
#define UDS_H

void UDS_Init(void);
#include <stdint.h>
#include "app_engine.h"
#include "dtc_manager.h"

/*----------------------------------------------------------------------------
 * Service identifiers
 *--------------------------------------------------------------------------*/

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL  (0x10u)
#define UDS_SID_ECU_RESET                   (0x11u)
#define UDS_SID_READ_DTC_INFORMATION        (0x19u)
#define UDS_SID_READ_DATA_BY_IDENTIFIER     (0x22u)
#define UDS_SID_TESTER_PRESENT              (0x3Eu)

/** @brief Value added to a request identifier to form a positive response. */
#define UDS_POSITIVE_RESPONSE_OFFSET        (0x40u)

/** @brief First byte of every negative response. */
#define UDS_NEGATIVE_RESPONSE_SID           (0x7Fu)

/*----------------------------------------------------------------------------
 * Negative response codes, per SWR-UDS-003
 *--------------------------------------------------------------------------*/

#define UDS_NRC_SERVICE_NOT_SUPPORTED       (0x11u)
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED  (0x12u)
#define UDS_NRC_INCORRECT_LENGTH            (0x13u)
#define UDS_NRC_CONDITIONS_NOT_CORRECT      (0x22u)
#define UDS_NRC_REQUEST_OUT_OF_RANGE        (0x31u)
#define UDS_NRC_SECURITY_ACCESS_DENIED      (0x33u)

/*----------------------------------------------------------------------------
 * Sub-function values
 *--------------------------------------------------------------------------*/

/** @brief Default diagnostic session. */
#define UDS_SESSION_DEFAULT                 (0x01u)

/** @brief Extended diagnostic session. */
#define UDS_SESSION_EXTENDED                (0x03u)

/** @brief Soft reset, the only reset type supported by this project. */
#define UDS_RESET_SOFT                      (0x03u)

/** @brief Report the trouble codes matching a status mask. */
#define UDS_DTC_REPORT_BY_STATUS_MASK       (0x02u)

/** @brief Keep the session alive and answer. */
#define UDS_TESTER_PRESENT_RESPOND          (0x00u)

/** @brief Keep the session alive without answering. */
#define UDS_TESTER_PRESENT_SILENT           (0x80u)

/*----------------------------------------------------------------------------
 * Buffer sizing
 *--------------------------------------------------------------------------*/

/** @brief Largest response this module can produce, in bytes. */
#define UDS_MAX_RESPONSE_SIZE               (64u)

/** @brief Largest request this module accepts, in bytes. */
#define UDS_MAX_REQUEST_SIZE                (64u)

/*----------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** @brief Return code shared by every function of this module. */
typedef enum
{
    UDS_E_OK            = 0u,   /**< Positive response was produced         */
    UDS_E_NEGATIVE      = 1u,   /**< Negative response was produced         */
    UDS_E_NO_RESPONSE   = 2u,   /**< Request handled, nothing to send back  */
    UDS_E_NULL_PTR      = 3u,   /**< A required pointer was NULL            */
    UDS_E_NOT_INIT      = 4u,   /**< Context has not been initialised       */
    UDS_E_SMALL_BUFFER  = 5u    /**< Response buffer is too small           */
} Uds_ReturnType;

/**
 * @brief Session state of the diagnostic server.
 *
 * @details The caller allocates one of these and passes its address to every
 *          function. Nothing is stored inside the module itself.
 */
typedef struct
{
    uint8_t  currentSession;        /**< Active diagnostic session          */
    uint8_t  resetPending;          /**< 1 when a reset has been requested  */
    uint8_t  initialised;           /**< 1 after a successful init          */
    uint32_t requestCounter;        /**< Requests processed since init      */
    uint32_t negativeCounter;       /**< Negative responses produced        */
} Uds_ContextType;

/**
 * @brief Collaborating modules needed while a request is processed.
 *
 * @details Passing these as one structure keeps the service signatures short
 *          and makes the dependencies of the module explicit. A unit test can
 *          supply its own contexts without any hardware being present.
 */
typedef struct
{
    Uds_ContextType             *udsContext;     /**< Session state        */
    const AppEngine_ContextType *appContext;     /**< Live vehicle data    */
    Dtc_ContextType             *dtcContext;     /**< Trouble code storage */
} Uds_EnvironmentType;

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Initialise the diagnostic server to the default session.
 *
 * @param[out] context   Context to initialise.
 *
 * @return  UDS_E_OK on success.
 * @return  UDS_E_NULL_PTR if context is NULL.
 */
Uds_ReturnType Uds_Init(Uds_ContextType *context);

/*----------------------------------------------------------------------------
 * Request processing
 *--------------------------------------------------------------------------*/

/**
 * @brief   Process one complete diagnostic request.
 *
 * @details The first byte of the request selects the service. Unsupported
 *          services produce a negative response with code
 *          UDS_NRC_SERVICE_NOT_SUPPORTED, as required by SYS-007.
 *
 *          A response length of zero is legitimate. It occurs for the silent
 *          form of TesterPresent, where the standard requires the server to
 *          stay quiet.
 *
 * @param[in]  environment      Collaborating module contexts.
 * @param[in]  request          Complete request, service identifier first.
 * @param[in]  requestLength    Number of request bytes.
 * @param[out] response         Buffer receiving the response.
 * @param[in]  responseCapacity Size of the response buffer.
 * @param[out] responseLength   Number of response bytes produced.
 *
 * @return  UDS_E_OK when a positive response was produced.
 * @return  UDS_E_NEGATIVE when a negative response was produced.
 * @return  UDS_E_NO_RESPONSE when the request must stay unanswered.
 * @return  UDS_E_NULL_PTR if any pointer is NULL.
 * @return  UDS_E_NOT_INIT if the context was never initialised.
 * @return  UDS_E_SMALL_BUFFER if the response buffer is too small.
 */
Uds_ReturnType Uds_ProcessRequest(const Uds_EnvironmentType *environment,
                                  const uint8_t             *request,
                                  uint16_t                   requestLength,
                                  uint8_t                   *response,
                                  uint16_t                   responseCapacity,
                                  uint16_t                  *responseLength);

/*----------------------------------------------------------------------------
 * Session and reset state
 *--------------------------------------------------------------------------*/

/**
 * @brief   Read the currently active diagnostic session.
 *
 * @param[in]  context   Session context.
 * @param[out] session   Destination for the session value.
 *
 * @return  UDS_E_OK on success.
 * @return  UDS_E_NULL_PTR if any pointer is NULL.
 * @return  UDS_E_NOT_INIT if the context was never initialised.
 */
Uds_ReturnType Uds_GetCurrentSession(const Uds_ContextType *context,
                                     uint8_t               *session);

/**
 * @brief   Report whether a reset has been requested but not carried out.
 *
 * @details The ECUReset service only raises this flag. The actual reset must
 *          be performed by the caller once the positive response has really
 *          been transmitted, otherwise the tester never receives it.
 *
 * @param[in]  context   Session context.
 * @param[out] pending   Set to 1 when a reset is awaiting execution.
 *
 * @return  UDS_E_OK on success.
 * @return  UDS_E_NULL_PTR if any pointer is NULL.
 * @return  UDS_E_NOT_INIT if the context was never initialised.
 */
Uds_ReturnType Uds_IsResetPending(const Uds_ContextType *context,
                                  uint8_t               *pending);

/**
 * @brief   Clear the pending reset flag.
 *
 * @param[in,out] context   Session context.
 *
 * @return  UDS_E_OK on success.
 * @return  UDS_E_NULL_PTR if context is NULL.
 * @return  UDS_E_NOT_INIT if the context was never initialised.
 */
Uds_ReturnType Uds_ClearResetPending(Uds_ContextType *context);

/**
 * @brief Performs the project-defined software reset after response delivery.
 *
 * @details The operation restores the default diagnostic session and clears
 *          volatile reset state. It does not reset the MCU or CAN hardware.
 *
 * @param[in,out] context Session context.
 * @return UDS_E_OK on success or a validation/state error.
 */
Uds_ReturnType Uds_ExecuteSoftReset(Uds_ContextType *context);

#endif /* UDS_H */
#endif /* UDS_H */