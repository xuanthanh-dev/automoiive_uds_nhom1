/**
 * @file    did_manager.c
 * @brief   Implementation of the diagnostic data identifier table.
 *          See did_manager.h.
 */

#include "did_manager.h"

/*----------------------------------------------------------------------------
 * Constant data
 *--------------------------------------------------------------------------*/

/** @brief Vehicle identification number, encoded as ASCII per SYS-003. */
static const uint8_t didVehicleIdentification[DID_VIN_LENGTH] =
{
    'V', 'N', 'U', 'U', 'E', 'T', '2', '0', '2',
    '6', 'S', 'T', 'M', '3', '2', '0', '1'
};

/** @brief Software version string, encoded as ASCII per SYS-004. */
static const uint8_t didSoftwareVersion[DID_SOFTWARE_VERSION_LENGTH] =
{
    'V', '1', '.', '0', '.', '0', '0', '1'
};

/*----------------------------------------------------------------------------
 * Local helpers
 *--------------------------------------------------------------------------*/

/**
 * @brief   Copy a constant block into the destination buffer.
 *
 * @param[in]  source           Constant data to copy.
 * @param[in]  sourceLength     Number of bytes to copy.
 * @param[out] buffer           Destination buffer.
 * @param[in]  bufferCapacity   Size of the destination buffer.
 * @param[out] writtenLength    Number of bytes written.
 *
 * @return  DID_E_OK on success, DID_E_SMALL_BUFFER if there is no room.
 */
static Did_ReturnType DidManager_CopyConstant(const uint8_t *source,
                                              uint8_t        sourceLength,
                                              uint8_t       *buffer,
                                              uint8_t        bufferCapacity,
                                              uint8_t       *writtenLength)
{
    Did_ReturnType result;
    uint8_t        index;

    if (bufferCapacity < sourceLength)
    {
        result = DID_E_SMALL_BUFFER;
    }
    else
    {
        for (index = 0u; index < sourceLength; index++)
        {
            buffer[index] = source[index];
        }

        *writtenLength = sourceLength;
        result         = DID_E_OK;
    }

    return result;
}

/**
 * @brief   Write a live value taken from the application layer.
 *
 * @param[in]  dataIdentifier   Identifier being served.
 * @param[in]  appContext       Application context providing live values.
 * @param[out] buffer           Destination buffer.
 * @param[in]  bufferCapacity   Size of the destination buffer.
 * @param[out] writtenLength    Number of bytes written.
 *
 * @return  DID_E_OK on success, or an error describing what went wrong.
 */
static Did_ReturnType DidManager_ReadLiveValue(
                                    uint16_t                     dataIdentifier,
                                    const AppEngine_ContextType *appContext,
                                    uint8_t                     *buffer,
                                    uint8_t                      bufferCapacity,
                                    uint8_t                     *writtenLength)
{
    Did_ReturnType        result;
    AppEngine_SignalsType signals;
    uint8_t               requiredLength;

    if (dataIdentifier == DID_VEHICLE_SPEED)
    {
        requiredLength = 2u;
    }
    else if (dataIdentifier == DID_ENGINE_SPEED)
    {
        requiredLength = 2u;
    }
    else
    {
        /* Temperature and battery voltage are single byte values */
        requiredLength = 1u;
    }

    if (bufferCapacity < requiredLength)
    {
        result = DID_E_SMALL_BUFFER;
    }
    else if (AppEngine_GetSignals(appContext, &signals) != APP_E_OK)
    {
        result = DID_E_SOURCE_FAILED;
    }
    else
    {
        if (dataIdentifier == DID_VEHICLE_SPEED)
        {
            buffer[0] = (uint8_t)((signals.vehicleSpeedKmh >> 8u) & 0xFFu);
            buffer[1] = (uint8_t)(signals.vehicleSpeedKmh & 0xFFu);
        }
        else if (dataIdentifier == DID_ENGINE_SPEED)
        {
            buffer[0] = (uint8_t)((signals.engineSpeedRpm >> 8u) & 0xFFu);
            buffer[1] = (uint8_t)(signals.engineSpeedRpm & 0xFFu);
        }
        else if (dataIdentifier == DID_ENGINE_TEMPERATURE)
        {
            buffer[0] = signals.engineTempCelsius;
        }
        else
        {
            buffer[0] = signals.batteryVoltageDeciV;
        }

        *writtenLength = requiredLength;
        result         = DID_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Interface
 *--------------------------------------------------------------------------*/

/**
 * @brief   Report whether an identifier exists. See did_manager.h.
 */
Did_ReturnType DidManager_IsSupported(uint16_t  dataIdentifier,
                                      uint8_t  *supported)
{
    Did_ReturnType result;

    if (supported == 0)
    {
        result = DID_E_NULL_PTR;
    }
    else
    {
        if ((dataIdentifier == DID_VIN) ||
            (dataIdentifier == DID_SOFTWARE_VERSION) ||
            (dataIdentifier == DID_VEHICLE_SPEED) ||
            (dataIdentifier == DID_ENGINE_SPEED) ||
            (dataIdentifier == DID_ENGINE_TEMPERATURE) ||
            (dataIdentifier == DID_BATTERY_VOLTAGE))
        {
            *supported = 1u;
        }
        else
        {
            *supported = 0u;
        }

        result = DID_E_OK;
    }

    return result;
}

/**
 * @brief   Write the data for an identifier. See did_manager.h.
 */
Did_ReturnType DidManager_ReadData(uint16_t                     dataIdentifier,
                                   const AppEngine_ContextType *appContext,
                                   uint8_t                     *buffer,
                                   uint8_t                      bufferCapacity,
                                   uint8_t                     *writtenLength)
{
    Did_ReturnType result;

    if ((appContext == 0) || (buffer == 0) || (writtenLength == 0))
    {
        result = DID_E_NULL_PTR;
    }
    else if (dataIdentifier == DID_VIN)
    {
        result = DidManager_CopyConstant(didVehicleIdentification,
                                         DID_VIN_LENGTH,
                                         buffer, bufferCapacity,
                                         writtenLength);
    }
    else if (dataIdentifier == DID_SOFTWARE_VERSION)
    {
        result = DidManager_CopyConstant(didSoftwareVersion,
                                         DID_SOFTWARE_VERSION_LENGTH,
                                         buffer, bufferCapacity,
                                         writtenLength);
    }
    else if ((dataIdentifier == DID_VEHICLE_SPEED) ||
             (dataIdentifier == DID_ENGINE_SPEED) ||
             (dataIdentifier == DID_ENGINE_TEMPERATURE) ||
             (dataIdentifier == DID_BATTERY_VOLTAGE))
    {
        result = DidManager_ReadLiveValue(dataIdentifier, appContext,
                                          buffer, bufferCapacity,
                                          writtenLength);
    }
    else
    {
        result = DID_E_NOT_SUPPORTED;
    }

    return result;
}
