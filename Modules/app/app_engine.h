/**
 * @file    app_engine.h
 * @brief   ECU application and simulated engine data provider.
 *
 * This module owns:
 *   - Simulated engine/vehicle signals
 *   - Fault evaluation
 *   - DTC status update
 *   - CAN RX processing
 *   - ISO-TP processing
 *   - UDS processing
 *   - ECU diagnostic reset handling
 *
 * main_ec.c only performs MCU/peripheral initialisation
 * and cyclic scheduling.
 */

#ifndef APP_ENGINE_H
#define APP_ENGINE_H

#include <stdint.h>

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

#define APP_ENGINE_SPEED_MIN_KMH        (60U)
#define APP_ENGINE_SPEED_MAX_KMH        (100U)

#define APP_ENGINE_TEMP_THRESHOLD_C     (110U)
#define APP_ENGINE_BATTERY_THRESHOLD_DV (110U)

#define APP_ENGINE_STEP_PERIOD_MS       (100U)

/* ECU diagnostic CAN IDs */
#define APP_ENGINE_CAN_ID_REQUEST       (0x7E0U)
#define APP_ENGINE_CAN_ID_RESPONSE      (0x7E8U)

/* ============================================================
 * RETURN TYPE
 * ============================================================ */

typedef enum
{
    APP_E_OK            = 0U,
    APP_E_NULL_PTR      = 1U,
    APP_E_NOT_INIT      = 2U,
    APP_E_INVALID_PARAM = 3U,
    APP_E_SMALL_BUFFER  = 4U

} AppEngine_ReturnType;

/* ============================================================
 * SIMULATED ENGINE SIGNALS
 * ============================================================ */

typedef struct
{
    uint16_t vehicleSpeedKmh;
    uint16_t engineSpeedRpm;
    uint8_t  engineTempCelsius;
    uint8_t  batteryVoltageDeciV;

} AppEngine_SignalsType;

/* ============================================================
 * ENGINE SIMULATION CONTEXT
 *
 * IMPORTANT:
 * This type is deliberately defined here before uds.h is
 * involved. UDS only needs a pointer to this type.
 * ============================================================ */

typedef struct AppEngine_ContextType
{
    AppEngine_SignalsType signals;

    uint8_t  simulationStep;
    uint8_t  rampDirectionUp;
    uint8_t  manualOverrideActive;
    uint8_t  initialised;

    uint32_t lastStepTimeMs;

} AppEngine_ContextType;

/* ============================================================
 * ENGINE SIMULATION API
 * ============================================================ */

/**
 * @brief Initialise engine simulation.
 */
AppEngine_ReturnType AppEngine_Init(
    AppEngine_ContextType *context);

/**
 * @brief Execute one simulation step when the period expires.
 */
AppEngine_ReturnType AppEngine_RunSimulationStep(
    AppEngine_ContextType *context,
    uint32_t currentTimeMs);

/**
 * @brief Read current simulated signals.
 */
AppEngine_ReturnType AppEngine_GetSignals(
    const AppEngine_ContextType *context,
    AppEngine_SignalsType *signals);


/* ============================================================
 * FAULT EVALUATION API
 * ============================================================ */

/**
 * @brief Check engine over-temperature condition.
 */
AppEngine_ReturnType AppEngine_IsOverTemperature(
    const AppEngine_ContextType *context,
    uint8_t *faultActive);

/**
 * @brief Check low-battery condition.
 */
AppEngine_ReturnType AppEngine_IsLowBattery(
    const AppEngine_ContextType *context,
    uint8_t *faultActive);

/* ============================================================
 * ECU APPLICATION API
 * ============================================================ */

/**
 * @brief Initialise complete ECU application.
 *
 * Initialises:
 *   - Engine simulation
 *   - DTC manager
 *   - UDS
 *   - ISO-TP
 *   - UDS environment
 */
AppEngine_ReturnType AppEngine_EcuInit(void);

/**
 * @brief Execute one ECU cyclic task.
 *
 * main_ec.c calls this function continuously.
 */
void AppEngine_MainFunction(
    uint32_t currentTimeMs);

#endif /* APP_ENGINE_H */
