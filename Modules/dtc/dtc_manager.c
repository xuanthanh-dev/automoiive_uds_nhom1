/**
 * @file    dtc_manager.c
 * @brief   Implementation of the diagnostic trouble code manager.
 *          See dtc_manager.h.
 */

#include "dtc_manager.h"

/*----------------------------------------------------------------------------
 * Local helpers
 *--------------------------------------------------------------------------*/

/**
 * @brief   Locate a trouble code in the table.
 *
 * @param[in]  context   Trouble code context.
 * @param[in]  code      Trouble code to find.
 * @param[out] index     Position in the table when found.
 *
 * @return  1 when the code exists in the table, 0 otherwise.
 */
static uint8_t DtcManager_FindIndex(const Dtc_ContextType *context,
                                    uint32_t               code,
                                    uint8_t               *index)
{
    uint8_t position;
    uint8_t found;

    found = 0u;

    for (position = 0u; position < DTC_MANAGER_COUNT; position++)
    {
        if ((found == 0u) && (context->entry[position].code == code))
        {
            *index = position;
            found  = 1u;
        }
        else
        {
            /* Either already found, or this entry does not match */
        }
    }

    return found;
}

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Populate the trouble code table. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_Init(Dtc_ContextType *context)
{
    Dtc_ReturnType result;

    if (context == 0)
    {
        result = DTC_E_NULL_PTR;
    }
    else
    {
        context->entry[0].code   = DTC_CODE_ENGINE_OVER_TEMP;
        context->entry[0].active = 0u;

        context->entry[1].code   = DTC_CODE_LOW_BATTERY;
        context->entry[1].active = 0u;

        context->initialised = 1u;

        result = DTC_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Fault status
 *--------------------------------------------------------------------------*/

/**
 * @brief   Set or clear one trouble code. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_SetStatus(Dtc_ContextType *context,
                                    uint32_t         code,
                                    uint8_t          active)
{
    Dtc_ReturnType result;
    uint8_t        index;

    index = 0u;

    if (context == 0)
    {
        result = DTC_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = DTC_E_NOT_INIT;
    }
    else if (DtcManager_FindIndex(context, code, &index) == 0u)
    {
        result = DTC_E_UNKNOWN_CODE;
    }
    else
    {
        if (active != 0u)
        {
            context->entry[index].active = 1u;
        }
        else
        {
            context->entry[index].active = 0u;
        }

        result = DTC_E_OK;
    }

    return result;
}

/**
 * @brief   Read the state of one trouble code. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_GetStatus(const Dtc_ContextType *context,
                                    uint32_t               code,
                                    uint8_t               *active)
{
    Dtc_ReturnType result;
    uint8_t        index;

    index = 0u;

    if ((context == 0) || (active == 0))
    {
        result = DTC_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = DTC_E_NOT_INIT;
    }
    else if (DtcManager_FindIndex(context, code, &index) == 0u)
    {
        result = DTC_E_UNKNOWN_CODE;
    }
    else
    {
        *active = context->entry[index].active;
        result  = DTC_E_OK;
    }

    return result;
}

/**
 * @brief   Clear every trouble code. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_ClearAll(Dtc_ContextType *context)
{
    Dtc_ReturnType result;
    uint8_t        index;

    if (context == 0)
    {
        result = DTC_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = DTC_E_NOT_INIT;
    }
    else
    {
        for (index = 0u; index < DTC_MANAGER_COUNT; index++)
        {
            context->entry[index].active = 0u;
        }

        result = DTC_E_OK;
    }

    return result;
}

/**
 * @brief   Count the active trouble codes. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_CountActive(const Dtc_ContextType *context,
                                      uint8_t               *activeCount)
{
    Dtc_ReturnType result;
    uint8_t        index;
    uint8_t        counter;

    if ((context == 0) || (activeCount == 0))
    {
        result = DTC_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = DTC_E_NOT_INIT;
    }
    else
    {
        counter = 0u;

        for (index = 0u; index < DTC_MANAGER_COUNT; index++)
        {
            if (context->entry[index].active != 0u)
            {
                counter++;
            }
            else
            {
                /* This code is not active */
            }
        }

        *activeCount = counter;
        result       = DTC_E_OK;
    }

    return result;
}

/*----------------------------------------------------------------------------
 * Serialisation
 *--------------------------------------------------------------------------*/

/**
 * @brief   Write the active trouble codes into a buffer. See dtc_manager.h.
 */
Dtc_ReturnType DtcManager_SerialiseActive(const Dtc_ContextType *context,
                                          uint8_t               *buffer,
                                          uint8_t                bufferCapacity,
                                          uint8_t               *writtenLength)
{
    Dtc_ReturnType result;
    uint8_t        index;
    uint8_t        position;
    uint8_t        activeCount;

    if ((context == 0) || (buffer == 0) || (writtenLength == 0))
    {
        result = DTC_E_NULL_PTR;
    }
    else if (context->initialised == 0u)
    {
        result = DTC_E_NOT_INIT;
    }
    else
    {
        activeCount = 0u;

        for (index = 0u; index < DTC_MANAGER_COUNT; index++)
        {
            if (context->entry[index].active != 0u)
            {
                activeCount++;
            }
            else
            {
                /* This code is not active */
            }
        }

        if (bufferCapacity < (uint8_t)(activeCount * DTC_MANAGER_RECORD_SIZE))
        {
            result = DTC_E_SMALL_BUFFER;
        }
        else
        {
            position = 0u;

            for (index = 0u; index < DTC_MANAGER_COUNT; index++)
            {
                if (context->entry[index].active != 0u)
                {
                    /* Three code bytes, most significant byte first */
                    buffer[position] =
                        (uint8_t)((context->entry[index].code >> 16u) & 0xFFuL);
                    buffer[position + 1u] =
                        (uint8_t)((context->entry[index].code >> 8u) & 0xFFuL);
                    buffer[position + 2u] =
                        (uint8_t)(context->entry[index].code & 0xFFuL);

                    /* One status byte */
                    buffer[position + 3u] = DTC_STATUS_ACTIVE;

                    position = (uint8_t)(position + DTC_MANAGER_RECORD_SIZE);
                }
                else
                {
                    /* Inactive codes are omitted from the report */
                }
            }

            *writtenLength = position;
            result         = DTC_E_OK;
        }
    }

    return result;
}
