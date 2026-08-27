/**
 * @file    app_diag.h
 * @brief   Diagnostic Tool application request/response and tester control.
 */
#ifndef APP_DIAG_H
#define APP_DIAG_H

#include <stdint.h>

#define APP_DIAG_MAX_MESSAGE_LENGTH (64U)

/*----------------------------------------------------------------------------
 * Tester configuration
 *--------------------------------------------------------------------------*/
#define DIAG_CAN_ID_REQUEST         (0x7E0U)
#define DIAG_CAN_ID_RESPONSE        (0x7E8U)
#define DIAG_RESPONSE_TIMEOUT_MS    (10000U)

/*----------------------------------------------------------------------------
 * AppDiag return codes
 *--------------------------------------------------------------------------*/
typedef enum
{
    APP_DIAG_E_OK = 0U,
    APP_DIAG_E_NULL_PTR,
    APP_DIAG_E_NOT_INITIALIZED,
    APP_DIAG_E_BUSY,
    APP_DIAG_E_INVALID_PARAMETER,
    APP_DIAG_E_INVALID_RESPONSE,
    APP_DIAG_E_SMALL_BUFFER,
    APP_DIAG_E_NO_DATA
} AppDiag_ReturnType;

/*----------------------------------------------------------------------------
 * Request lifecycle
 *--------------------------------------------------------------------------*/
typedef enum
{
    APP_DIAG_STATE_UNINITIALIZED = 0U,
    APP_DIAG_STATE_IDLE,
    APP_DIAG_STATE_REQUEST_READY,
    APP_DIAG_STATE_WAIT_RESPONSE,
    APP_DIAG_STATE_RESPONSE_READY,
    APP_DIAG_STATE_ERROR
} AppDiag_StateType;

/*----------------------------------------------------------------------------
 * Tester UI/state types
 *--------------------------------------------------------------------------*/
typedef enum
{
    DIAG_MENU_MAIN = 0U,
    DIAG_MENU_SESSION,
    DIAG_MENU_DID
} Diag_MenuType;

typedef enum
{
    DIAG_ACTION_NONE = 0U,
    DIAG_ACTION_SESSION_DEFAULT,
    DIAG_ACTION_SESSION_EXTENDED,
    DIAG_ACTION_RESET,
    DIAG_ACTION_DTC,
    DIAG_ACTION_DID_SINGLE,
    DIAG_ACTION_ENGINE_STATUS
} Diag_ActionType;

typedef struct
{
    uint16_t did;
    const char *name;
} Diag_DidItemType;

/*----------------------------------------------------------------------------
 * Application context
 *--------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------
 * Request/response manager API
 *--------------------------------------------------------------------------*/
AppDiag_ReturnType AppDiag_Init(AppDiag_ContextType *context);

AppDiag_ReturnType AppDiag_PrepareSessionControl(
    AppDiag_ContextType *context,
    uint8_t session);

AppDiag_ReturnType AppDiag_PrepareSoftReset(
    AppDiag_ContextType *context);

AppDiag_ReturnType AppDiag_PrepareReadDid(
    AppDiag_ContextType *context,
    uint16_t dataIdentifier);

AppDiag_ReturnType AppDiag_PrepareReadDtc(
    AppDiag_ContextType *context,
    uint8_t statusMask);

AppDiag_ReturnType AppDiag_PrepareTesterPresent(
    AppDiag_ContextType *context,
    uint8_t suppressResponse);

AppDiag_ReturnType AppDiag_GetPreparedRequest(
    const AppDiag_ContextType *context,
    uint8_t *buffer,
    uint16_t bufferCapacity,
    uint16_t *requestLength);

AppDiag_ReturnType AppDiag_NotifyRequestTransmitted(
    AppDiag_ContextType *context);

AppDiag_ReturnType AppDiag_RxIndication(
    AppDiag_ContextType *context,
    const uint8_t *response,
    uint16_t responseLength);

AppDiag_ReturnType AppDiag_GetResponse(
    const AppDiag_ContextType *context,
    uint8_t *buffer,
    uint16_t bufferCapacity,
    uint16_t *responseLength);

AppDiag_ReturnType AppDiag_ClearResponse(
    AppDiag_ContextType *context);

/*----------------------------------------------------------------------------
 * Complete Diagnostic Tester application API
 *--------------------------------------------------------------------------*/

/**
 * @brief Initialise AppDiag, ISO-TP tester transport and UART command input.
 */
void AppDiag_TesterInit(void);
void Diag_PrintMainMenu(void);
void Diag_HandleCommand(uint8_t command);
void Diag_ProcessCan(void);

/**
 * @brief Cyclic Diagnostic Tester task.
 *
 * Call continuously from main loop. It processes UART commands, CAN RX,
 * ISO-TP state/timers and application response timeout.
 */
void AppDiag_MainFunction(void);
#endif /* APP_DIAG_H */
