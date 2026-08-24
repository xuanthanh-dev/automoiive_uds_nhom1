/**
 * @file    app_engine.c
 * @brief   Implementation of the simulated engine data provider.
 *          See app_engine.h.
 */

#include "app_engine.h"

/*----------------------------------------------------------------------------
 * Local constants
 *--------------------------------------------------------------------------*/

/** @brief Number of steps in one half of the triangular ramp. */
#define APP_ENGINE_STEP_COUNT           (40u)

/** @brief Engine speed produced per km/h of vehicle speed. */
#define APP_ENGINE_RPM_PER_KMH          (30u)

/** @brief Base engine temperature at minimum vehicle speed, in degrees C. */
#define APP_ENGINE_TEMP_BASE_C          (70u)

/** @brief Nominal battery voltage in units of 0.1 V. */
#define APP_ENGINE_BATTERY_NOMINAL_DV   (138u)

/*----------------------------------------------------------------------------
 * Local helpers
 *--------------------------------------------------------------------------*/

/**
 * @brief   Derive every signal from the current ramp position.
 *
 * @param[in,out] context   Simulation context to update.
 */
static void AppEngine_DeriveSignals(AppEngine_ContextType *context)
{
    uint16_t speed;

    speed = (uint16_t)(APP_ENGINE_SPEED_MIN_KMH + context->simulationStep);

    context->signals.vehicleSpeedKmh   = speed;
    context->signals.engineSpeedRpm    = (uint16_t)(speed *
                                                    APP_ENGINE_RPM_PER_KMH);
    context->signals.engineTempCelsius = (uint8_t)(APP_ENGINE_TEMP_BASE_C +
                                                   (context->simulationStep / 2u));
    context->signals.batteryVoltageDeciV = APP_ENGINE_BATTERY_NOMINAL_DV;
}

/**
 * @brief   Move the ramp one step and reverse direction at either end.
 *
 * @param[in,out] context   Simulation context to update.
 */
static void AppEngine_AdvanceRamp(AppEngine_ContextType *context)
{
    if (context->rampDirectionUp != 0u)
    {
        if (context->simulationStep >= APP_ENGINE_STEP_COUNT)
        {
            context->rampDirectionUp = 0u;
            context->simulationStep--;
        }
        else
        {
            context->simulationStep++;
        }
    }
    else
    {
        if (context->simulationStep == 0u)
        {
            context->rampDirectionUp = 1u;
            context->simulationStep++;
        }
        else
        {
            context->simulationStep--;
        }
    }
}

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Initialise the simulation context. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_Init(AppEngine_ContextType *context)
{
    AppEngine_ReturnType result;

    if (context == 0)
    {
        result = APP_E_NULL_PTR;
    }
    else
    {
        context->simulationStep       = 0u;
        context->rampDirectionUp      = 1u;
        context->manualOverrideActive = 0u;
        context->lastStepTimeMs       = 0u;

        AppEngine_DeriveSignals(context);

        context->initialised = 1u;

        result = APP_E_OK;
    }

    return result;
}

/**
 * @brief   Advance the simulation. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_RunSimulationStep(AppEngine_ContextType *context,
                                                 uint32_t currentTimeMs)
{
    AppEngine_ReturnType result;

    if (context == 0)
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else if (context->manualOverrideActive != 0u)
    {
        /*
         * Values were forced by a tester. Leave them untouched, otherwise the
         * next simulation step would silently overwrite the injected fault.
         */
        result = APP_E_OK;
    }
    else if ((currentTimeMs - context->lastStepTimeMs) <
             (uint32_t)APP_ENGINE_STEP_PERIOD_MS)
    {
        /* The step period has not elapsed yet */
        result = APP_E_OK;
    }
    else
    {
        context->lastStepTimeMs = currentTimeMs;

        AppEngine_AdvanceRamp(context);
        AppEngine_DeriveSignals(context);

        result = APP_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Signal access
 *--------------------------------------------------------------------------*/

/**
 * @brief   Read every simulated signal. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_GetSignals(const AppEngine_ContextType *context,
                                          AppEngine_SignalsType       *signals)
{
    AppEngine_ReturnType result;

    if ((context == 0) || (signals == 0))
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else
    {
        signals->vehicleSpeedKmh     = context->signals.vehicleSpeedKmh;
        signals->engineSpeedRpm      = context->signals.engineSpeedRpm;
        signals->engineTempCelsius   = context->signals.engineTempCelsius;
        signals->batteryVoltageDeciV = context->signals.batteryVoltageDeciV;

        result = APP_E_OK;
    }

    return result;
}

/**
 * @brief   Force every signal and suspend the ramp. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_SetSignals(AppEngine_ContextType       *context,
                                          const AppEngine_SignalsType *signals)
{
    AppEngine_ReturnType result;

    if ((context == 0) || (signals == 0))
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else
    {
        context->signals.vehicleSpeedKmh     = signals->vehicleSpeedKmh;
        context->signals.engineSpeedRpm      = signals->engineSpeedRpm;
        context->signals.engineTempCelsius   = signals->engineTempCelsius;
        context->signals.batteryVoltageDeciV = signals->batteryVoltageDeciV;

        context->manualOverrideActive = 1u;

        result = APP_E_OK;
    }

    return result;
}

/**
 * @brief   Clear the manual override. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_ResumeSimulation(AppEngine_ContextType *context)
{
    AppEngine_ReturnType result;

    if (context == 0)
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else
    {
        context->manualOverrideActive = 0u;
        result = APP_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Fault conditions
 *--------------------------------------------------------------------------*/

/**
 * @brief   Report an over temperature condition. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_IsOverTemperature(
                                        const AppEngine_ContextType *context,
                                        uint8_t                     *faultActive)
{
    AppEngine_ReturnType result;

    if ((context == 0) || (faultActive == 0))
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else
    {
        if (context->signals.engineTempCelsius > APP_ENGINE_TEMP_THRESHOLD_C)
        {
            *faultActive = 1u;
        }
        else
        {
            *faultActive = 0u;
        }

        result = APP_E_OK;
    }

    return result;
}

/**
 * @brief   Report a low battery condition. See app_engine.h.
 */
AppEngine_ReturnType AppEngine_IsLowBattery(
                                        const AppEngine_ContextType *context,
                                        uint8_t                     *faultActive)
{
    AppEngine_ReturnType result;

    if ((context == 0) || (faultActive == 0))
    {
        result = APP_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = APP_E_NOT_INIT;
    }
    else
    {
        if (context->signals.batteryVoltageDeciV <
            APP_ENGINE_BATTERY_THRESHOLD_DV)
        {
            *faultActive = 1u;
        }
        else
        {
            *faultActive = 0u;
        }

        result = APP_E_OK;
    }

    return result;
}