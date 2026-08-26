/**
 * @file    app_engine.c
 * @brief   ECU application and simulated engine data provider.
 *
 * Processing:
 *
 *   Engine simulation
 *          |
 *          +---- DTC update
 *          |
 *          +---- CAN RX
 *                  |
 *                  v
 *                ISO-TP
 *                  |
 *                  v
 *                 UDS
 *                  |
 *          +-------+-------+
 *          |               |
 *       AppEngine       DTC Manager
 *          |
 *          +---- ECU reset handling
 */

#include "app_engine.h"

#include "main.h"
#include "Can_if.h"
#include "isotp.h"
#include "uds.h"
#include "dtc_manager.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 * INTERNAL CONFIGURATION
 * ============================================================ */

#define APP_ENGINE_STEP_COUNT         (40U)
#define APP_ENGINE_RPM_PER_KMH        (30U)
#define APP_ENGINE_TEMP_BASE_C        (70U)
#define APP_ENGINE_BATTERY_NOMINAL_DV (138U)

/* ============================================================
 * INTERNAL ECU CONTEXT
 *
 * IMPORTANT:
 * This structure must NOT be in app_engine.h.
 *
 * Keeping it here prevents circular dependencies between:
 *
 * app_engine.h <-> uds.h <-> did_manager.h
 * ============================================================ */

typedef struct
{
    AppEngine_ContextType engine;

    Uds_ContextType       uds;

    Dtc_ContextType       dtc;

    Uds_EnvironmentType   udsEnvironment;

    uint8_t responseBuffer[UDS_MAX_RESPONSE_SIZE];

    uint16_t responseLength;

    uint8_t initialised;

} AppEngine_EcuContextType;

/* ============================================================
 * GLOBAL ECU APPLICATION CONTEXT
 * ============================================================ */

static AppEngine_EcuContextType appEngineEcu;
static void AppEngine_PrintHex(const uint8_t *data, uint16_t length);
static void AppEngine_DeriveSignals(AppEngine_ContextType *context);
static void AppEngine_AdvanceRamp(AppEngine_ContextType *context);
static CAN_StatusTypeDef AppEngine_CanSend(const uint8_t *frame, uint8_t length);
static void AppEngine_UpdateDtcStatus(void);
static void AppEngine_UdsRxCallback(const uint8_t *request, uint16_t length);
static void AppEngine_ProcessCanRx(uint32_t currentTimeMs);
static void AppEngine_HandlePendingReset(void);

/* ============================================================
 * HEX DEBUG PRINT
 * ============================================================ */

static void AppEngine_PrintHex(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t index;

    if (data == 0)
    {
        return;
    }

    for (index = 0U; index < length; index++)
    {
        printf("%02X ", data[index]);
    }
}

/* ============================================================
 * ENGINE SIGNAL DERIVATION
 * ============================================================ */

static void AppEngine_DeriveSignals(
    AppEngine_ContextType *context)
{
    uint16_t speed;

    if (context == 0)
    {
        return;
    }

    speed = (uint16_t)(
        APP_ENGINE_SPEED_MIN_KMH +
        context->simulationStep
    );

    if (speed > APP_ENGINE_SPEED_MAX_KMH)
    {
        speed = APP_ENGINE_SPEED_MAX_KMH;
    }

    context->signals.vehicleSpeedKmh =
        speed;

    context->signals.engineSpeedRpm =
        (uint16_t)(
            speed *
            APP_ENGINE_RPM_PER_KMH
        );

    context->signals.engineTempCelsius =
        (uint8_t)(
            APP_ENGINE_TEMP_BASE_C +
            (context->simulationStep / 2U)
        );

    context->signals.batteryVoltageDeciV =
        APP_ENGINE_BATTERY_NOMINAL_DV;
}

/* ============================================================
 * ENGINE RAMP
 * ============================================================ */

static void AppEngine_AdvanceRamp(
    AppEngine_ContextType *context)
{
    if (context == 0)
    {
        return;
    }

    if (context->rampDirectionUp != 0U)
    {
        if (context->simulationStep >=
            APP_ENGINE_STEP_COUNT)
        {
            context->rampDirectionUp = 0U;

            if (context->simulationStep > 0U)
            {
                context->simulationStep--;
            }
        }
        else
        {
            context->simulationStep++;
        }
    }
    else
    {
        if (context->simulationStep == 0U)
        {
            context->rampDirectionUp = 1U;
            context->simulationStep++;
        }
        else
        {
            context->simulationStep--;
        }
    }
}

/* ============================================================
 * CAN TX CALLBACK
 *
 * ISO-TP -> CAN
 * ============================================================ */

static CAN_StatusTypeDef AppEngine_CanSend(
    const uint8_t *frame,
    uint8_t length)
{
    CAN_StatusTypeDef result;

    if ((frame == 0) || (length != 8U))
    {
        return ERROR_INVALID_LENGTH;
    }

    printf(
        "[EC] CAN TX 0x%03X: ",
        APP_ENGINE_CAN_ID_RESPONSE
    );

    AppEngine_PrintHex(
        frame,
        length
    );

    printf("\r\n");

    result = CAN_IF_Transmit(
        APP_ENGINE_CAN_ID_RESPONSE,
        (uint8_t *)frame,
        length
    );

    if (result != OK)
    {
        printf(
            "[EC] CAN TX ERROR: %d\r\n",
            (int)result
        );
    }

    return result;
}

/* ============================================================
 * DTC STATUS UPDATE
 * ============================================================ */

static void AppEngine_UpdateDtcStatus(void)
{
    uint8_t overTemperature;
    uint8_t lowBattery;

    if (appEngineEcu.initialised == 0U)
    {
        return;
    }

    overTemperature = 0U;
    lowBattery = 0U;

    /* Engine over-temperature */

    if (AppEngine_IsOverTemperature(
            &appEngineEcu.engine,
            &overTemperature) == APP_E_OK)
    {
        (void)DtcManager_SetStatus(
            &appEngineEcu.dtc,
            DTC_CODE_ENGINE_OVER_TEMP,
            overTemperature
        );
    }

    /* Low battery */

    if (AppEngine_IsLowBattery(
            &appEngineEcu.engine,
            &lowBattery) == APP_E_OK)
    {
        (void)DtcManager_SetStatus(
            &appEngineEcu.dtc,
            DTC_CODE_LOW_BATTERY,
            lowBattery
        );
    }
}

/* ============================================================
 * UDS RX CALLBACK
 *
 * ISO-TP -> UDS
 * ============================================================ */

static void AppEngine_UdsRxCallback(
    const uint8_t *request,
    uint16_t length)
{
    Uds_ReturnType udsResult;

    uint32_t now;

    if ((request == 0) ||
        (length == 0U) ||
        (length > UDS_MAX_REQUEST_SIZE) ||
        (appEngineEcu.initialised == 0U))
    {
        return;
    }

    now = HAL_GetTick();

    printf("[EC] UDS RX: ");

    AppEngine_PrintHex(
        request,
        length
    );

    printf("\r\n");

    appEngineEcu.responseLength = 0U;

    udsResult = Uds_ProcessRequest(
        &appEngineEcu.udsEnvironment,
        request,
        length,
        appEngineEcu.responseBuffer,
        UDS_MAX_RESPONSE_SIZE,
        &appEngineEcu.responseLength
    );

    if ((udsResult == UDS_E_OK) ||
        (udsResult == UDS_E_NEGATIVE))
    {
        printf("[EC] UDS TX: ");

        AppEngine_PrintHex(
            appEngineEcu.responseBuffer,
            appEngineEcu.responseLength
        );

        printf("\r\n");

        /*
         * Do not send a zero-length response.
         */
        if (appEngineEcu.responseLength > 0U)
        {
            if (IsoTp_Send(
                    appEngineEcu.responseBuffer,
                    appEngineEcu.responseLength,
                    now) != ISOTP_OK)
            {
                printf(
                    "[EC] ISO-TP response send error\r\n"
                );
            }
        }
    }
    else if (udsResult == UDS_E_NO_RESPONSE)
    {
        printf(
            "[EC] UDS: no response required\r\n"
        );
    }
    else
    {
        printf(
            "[EC] UDS processing error: %d\r\n",
            (int)udsResult
        );
    }
}

/* ============================================================
 * CAN RX PROCESSING
 * ============================================================ */

static void AppEngine_ProcessCanRx(
    uint32_t currentTimeMs)
{
    uint32_t canId;

    uint8_t frame[8];

    uint8_t dlc;

    CAN_StatusTypeDef status;

    do
    {
        status = CAN_IF_GetReceivedFrame(
            &canId,
            frame,
            &dlc
        );

        if (status == OK)
        {
            /*
             * Only diagnostic request ID is accepted.
             */

            if (canId == APP_ENGINE_CAN_ID_REQUEST)
            {
                printf(
                    "[EC] CAN RX 0x%03lX: ",
                    (unsigned long)canId
                );

                AppEngine_PrintHex(
                    frame,
                    dlc
                );

                printf("\r\n");

                (void)IsoTp_OnCanFrame(
                    frame,
                    dlc,
                    currentTimeMs
                );
            }
        }

    } while (status == OK);
}

/* ============================================================
 * ECU RESET HANDLING
 * ============================================================ */

static void AppEngine_HandlePendingReset(void)
{
    uint8_t pending;

    if (appEngineEcu.initialised == 0U)
    {
        return;
    }

    pending = 0U;

    if (Uds_IsResetPending(
            &appEngineEcu.uds,
            &pending) != UDS_E_OK)
    {
        return;
    }

    if (pending != 0U)
    {
        printf(
            "[EC] ECU soft reset pending\r\n"
        );

        /*
         * Project-level software reset.
         *
         * This is intentionally NOT NVIC_SystemReset().
         */
        (void)Uds_ExecuteSoftReset(
            &appEngineEcu.uds
        );

        /*
         * Reset ISO-TP state as well.
         */
        IsoTp_Reset();
    }
}

/* ============================================================
 * ECU APPLICATION INITIALIZATION
 * ============================================================ */

AppEngine_ReturnType AppEngine_EcuInit(void)
{
    AppEngine_ReturnType result;

    /*
     * Clear complete ECU context.
     */
    (void)memset(
        &appEngineEcu,
        0,
        sizeof(appEngineEcu)
    );

    /* --------------------------------------------------------
     * 1. Engine simulation
     * -------------------------------------------------------- */

    result = AppEngine_Init(
        &appEngineEcu.engine
    );

    if (result != APP_E_OK)
    {
        return result;
    }

    /* --------------------------------------------------------
     * 2. DTC manager
     * -------------------------------------------------------- */

    if (DtcManager_Init(
            &appEngineEcu.dtc) != DTC_E_OK)
    {
        return APP_E_NOT_INIT;
    }

    /* --------------------------------------------------------
     * 3. UDS
     * -------------------------------------------------------- */

    if (Uds_Init(
            &appEngineEcu.uds) != UDS_E_OK)
    {
        return APP_E_NOT_INIT;
    }

    /* --------------------------------------------------------
     * 4. UDS environment
     * -------------------------------------------------------- */

    appEngineEcu.udsEnvironment.udsContext =
        &appEngineEcu.uds;

    appEngineEcu.udsEnvironment.appContext =
        &appEngineEcu.engine;

    appEngineEcu.udsEnvironment.dtcContext =
        &appEngineEcu.dtc;

    /* --------------------------------------------------------
     * 5. ISO-TP
     * -------------------------------------------------------- */

    if (IsoTp_Init(
            AppEngine_CanSend,
            AppEngine_UdsRxCallback) != ISOTP_OK)
    {
        return APP_E_NOT_INIT;
    }

    appEngineEcu.initialised = 1U;

    printf(
        "[EC] AppEngine ECU initialized\r\n"
    );

    printf(
        "[EC] CAN RX ID: 0x%03X\r\n",
        APP_ENGINE_CAN_ID_REQUEST
    );

    printf(
        "[EC] CAN TX ID: 0x%03X\r\n",
        APP_ENGINE_CAN_ID_RESPONSE
    );

    return APP_E_OK;
}

/* ============================================================
 * ECU MAIN FUNCTION
 * ============================================================ */

void AppEngine_MainFunction(
    uint32_t currentTimeMs)
{
    if (appEngineEcu.initialised == 0U)
    {
        return;
    }

    /*
     * 1. Engine simulation
     */

    (void)AppEngine_RunSimulationStep(
        &appEngineEcu.engine,
        currentTimeMs
    );

    /*
     * 2. Update DTC status
     */

    AppEngine_UpdateDtcStatus();

    /*
     * 3. CAN RX -> ISO-TP
     */

    AppEngine_ProcessCanRx(
        currentTimeMs
    );

    /*
     * 4. ISO-TP cyclic processing
     *
     * Required for:
     *   - Flow Control
     *   - Consecutive Frames
     *   - N_Bs
     *   - N_Cr
     */

    (void)IsoTp_MainFunction(
        currentTimeMs
    );

    /*
     * 5. UDS reset handling
     */

    AppEngine_HandlePendingReset();
}

/* ============================================================
 * ENGINE INITIALIZATION
 * ============================================================ */

AppEngine_ReturnType AppEngine_Init(
    AppEngine_ContextType *context)
{
    if (context == 0)
    {
        return APP_E_NULL_PTR;
    }

    context->simulationStep =
        0U;

    context->rampDirectionUp =
        1U;

    context->manualOverrideActive =
        0U;

    context->initialised =
        0U;

    context->lastStepTimeMs =
        0U;

    AppEngine_DeriveSignals(
        context
    );

    context->initialised =
        1U;

    return APP_E_OK;
}

/* ============================================================
 * RUN SIMULATION
 * ============================================================ */

AppEngine_ReturnType AppEngine_RunSimulationStep(
    AppEngine_ContextType *context,
    uint32_t currentTimeMs)
{
    if (context == 0)
    {
        return APP_E_NULL_PTR;
    }

    if (context->initialised == 0U)
    {
        return APP_E_NOT_INIT;
    }

    /*
     * Manual override freezes simulation.
     */
    if (context->manualOverrideActive != 0U)
    {
        return APP_E_OK;
    }

    /*
     * Wait until simulation period expires.
     *
     * Unsigned subtraction also behaves correctly across
     * uint32_t timer wrap-around.
     */
    if ((uint32_t)(
            currentTimeMs -
            context->lastStepTimeMs) <
        APP_ENGINE_STEP_PERIOD_MS)
    {
        return APP_E_OK;
    }

    context->lastStepTimeMs =
        currentTimeMs;

    AppEngine_AdvanceRamp(
        context
    );

    AppEngine_DeriveSignals(
        context
    );

    return APP_E_OK;
}

/* ============================================================
 * GET SIGNALS
 * ============================================================ */

AppEngine_ReturnType AppEngine_GetSignals(
    const AppEngine_ContextType *context,
    AppEngine_SignalsType *signals)
{
    if ((context == 0) ||
        (signals == 0))
    {
        return APP_E_NULL_PTR;
    }

    if (context->initialised == 0U)
    {
        return APP_E_NOT_INIT;
    }

    signals->vehicleSpeedKmh =
        context->signals.vehicleSpeedKmh;

    signals->engineSpeedRpm =
        context->signals.engineSpeedRpm;

    signals->engineTempCelsius =
        context->signals.engineTempCelsius;

    signals->batteryVoltageDeciV =
        context->signals.batteryVoltageDeciV;

    return APP_E_OK;
}


/* ============================================================
 * OVER TEMPERATURE
 * ============================================================ */

AppEngine_ReturnType AppEngine_IsOverTemperature(
    const AppEngine_ContextType *context,
    uint8_t *faultActive)
{
    if ((context == 0) ||
        (faultActive == 0))
    {
        return APP_E_NULL_PTR;
    }

    if (context->initialised == 0U)
    {
        return APP_E_NOT_INIT;
    }

    if (context->signals.engineTempCelsius >
        APP_ENGINE_TEMP_THRESHOLD_C)
    {
        *faultActive = 1U;
    }
    else
    {
        *faultActive = 0U;
    }

    return APP_E_OK;
}

/* ============================================================
 * LOW BATTERY
 * ============================================================ */

AppEngine_ReturnType AppEngine_IsLowBattery(
    const AppEngine_ContextType *context,
    uint8_t *faultActive)
{
    if ((context == 0) ||
        (faultActive == 0))
    {
        return APP_E_NULL_PTR;
    }

    if (context->initialised == 0U)
    {
        return APP_E_NOT_INIT;
    }

    if (context->signals.batteryVoltageDeciV <
        APP_ENGINE_BATTERY_THRESHOLD_DV)
    {
        *faultActive = 1U;
    }
    else
    {
        *faultActive = 0U;
    }

    return APP_E_OK;
}
