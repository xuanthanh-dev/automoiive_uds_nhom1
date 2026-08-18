/**
 * @file    app_engine.h
 * @brief   Simulated engine and vehicle data provider.
 *
 * @details This module owns the simulated vehicle signals required by
 *          SWR-APP-001 and evaluates the simulated fault conditions required
 *          by SYS-006. SYS-001 cyclic EngineStatus transmission is obsolete
 *          and is deliberately not implemented.
 *
 *          The module knows nothing about diagnostics, CAN identifiers or
 *          transport protocols or the DTC manager. It only maintains numeric
 *          signal values and reports fault conditions. main.c decides how
 *          those conditions update diagnostic trouble codes.
 *
 * @note    All state lives in AppEngine_ContextType, allocated by the caller.
 */

#ifndef APP_ENGINE_H
#define APP_ENGINE_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Configuration constants
 *--------------------------------------------------------------------------*/

/** @brief Lower bound of the simulated vehicle speed, in km/h. */
#define APP_ENGINE_SPEED_MIN_KMH        (60u)

/** @brief Upper bound of the simulated vehicle speed, in km/h. */
#define APP_ENGINE_SPEED_MAX_KMH        (100u)

/** @brief Engine temperature above which an over temperature fault exists. */
#define APP_ENGINE_TEMP_THRESHOLD_C     (110u)

/** @brief Battery voltage below which a low battery fault exists, in 0.1 V. */
#define APP_ENGINE_BATTERY_THRESHOLD_DV (110u)

/** @brief Period of one simulation step, in milliseconds. */
#define APP_ENGINE_STEP_PERIOD_MS       (100u)

/*----------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** @brief Return code shared by every function of this module. */
typedef enum
{
    APP_E_OK            = 0u,   /**< Success                                */
    APP_E_NULL_PTR      = 1u,   /**< A required pointer was NULL            */
    APP_E_NOT_INIT      = 2u,   /**< Context has not been initialised       */
    APP_E_INVALID_PARAM = 3u,   /**< A value is outside its valid range     */
    APP_E_SMALL_BUFFER  = 4u    /**< Destination buffer is too small        */
} AppEngine_ReturnType;

/** @brief Simulated vehicle signals, as required by SWR-APP-001. */
typedef struct
{
    uint16_t vehicleSpeedKmh;       /**< Vehicle speed in km/h              */
    uint16_t engineSpeedRpm;        /**< Engine speed in revolutions/minute */
    uint8_t  engineTempCelsius;     /**< Coolant temperature in degrees C   */
    uint8_t  batteryVoltageDeciV;   /**< Battery voltage in units of 0.1 V  */
} AppEngine_SignalsType;

/**
 * @brief Complete state of the simulation.
 *
 * @details The caller allocates one of these and passes its address to every
 *          function. Nothing is stored inside the module itself.
 */
typedef struct
{
    AppEngine_SignalsType signals;      /**< Current signal values          */

    uint8_t  simulationStep;            /**< Position in the ramp, 0 to 40  */
    uint8_t  rampDirectionUp;           /**< 1 while the ramp is rising     */
    uint8_t  manualOverrideActive;      /**< 1 when values were forced      */
    uint8_t  initialised;               /**< 1 after a successful init      */
    uint32_t lastStepTimeMs;            /**< Time of the last ramp update   */
} AppEngine_ContextType;

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Initialise the simulation context to its starting values.
 *
 * @param[out] context   Context to initialise.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if context is NULL.
 */
AppEngine_ReturnType AppEngine_Init(AppEngine_ContextType *context);

/**
 * @brief   Advance the simulation by one step if the period has elapsed.
 *
 * @details Vehicle speed follows a triangular ramp between
 *          APP_ENGINE_SPEED_MIN_KMH and APP_ENGINE_SPEED_MAX_KMH, and the
 *          other signals are derived from it. When a manual override is
 *          active the function returns immediately so that forced values are
 *          preserved.
 *
 *          The caller supplies the current time. The module never reads a
 *          clock itself, which keeps it testable on a host machine.
 *
 * @param[in,out] context        Simulation context.
 * @param[in]     currentTimeMs  Current time in milliseconds.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if context is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_RunSimulationStep(AppEngine_ContextType *context,
                                                 uint32_t currentTimeMs);

/*----------------------------------------------------------------------------
 * Signal access
 *--------------------------------------------------------------------------*/

/**
 * @brief   Read the current value of every simulated signal.
 *
 * @param[in]  context   Simulation context.
 * @param[out] signals   Destination for the signal values.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if any pointer is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_GetSignals(const AppEngine_ContextType *context,
                                          AppEngine_SignalsType       *signals);

/**
 * @brief   Force every signal to a chosen value and suspend the ramp.
 *
 * @details Used to inject the fault conditions demanded by SYS-006 without
 *          waiting for the simulation to reach them. The ramp stays suspended
 *          until AppEngine_ResumeSimulation() is called.
 *
 * @param[in,out] context   Simulation context.
 * @param[in]     signals   Values to apply.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if any pointer is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_SetSignals(AppEngine_ContextType       *context,
                                          const AppEngine_SignalsType *signals);

/**
 * @brief   Clear the manual override and let the ramp run again.
 *
 * @param[in,out] context   Simulation context.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if context is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_ResumeSimulation(AppEngine_ContextType *context);

/*----------------------------------------------------------------------------
 * Fault conditions
 *--------------------------------------------------------------------------*/

/**
 * @brief   Report whether the engine temperature exceeds its threshold.
 *
 * @param[in]  context      Simulation context.
 * @param[out] faultActive  Set to 1 when the fault condition holds.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if any pointer is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_IsOverTemperature(
                                        const AppEngine_ContextType *context,
                                        uint8_t                     *faultActive);

/**
 * @brief   Report whether the battery voltage is below its threshold.
 *
 * @param[in]  context      Simulation context.
 * @param[out] faultActive  Set to 1 when the fault condition holds.
 *
 * @return  APP_E_OK on success.
 * @return  APP_E_NULL_PTR if any pointer is NULL.
 * @return  APP_E_NOT_INIT if the context was never initialised.
 */
AppEngine_ReturnType AppEngine_IsLowBattery(
                                        const AppEngine_ContextType *context,
                                        uint8_t                     *faultActive);

#endif /* APP_ENGINE_H */
