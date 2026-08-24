/**
 * @file    dtc_manager.h
 * @brief   Storage and reporting of diagnostic trouble codes.
 *
 * @details Implements SWR-DTC-001. The module owns a fixed table of supported
 *          trouble codes, remembers which of them are currently active, and
 *          can serialise the active ones into the byte layout expected by UDS
 *          service ReadDTCInformation.
 *
 *          The module performs no fault detection of its own. Deciding that a
 *          fault exists is the job of the application layer, which then calls
 *          DtcManager_SetStatus(). Nor does the module know anything about
 *          UDS service identifiers or CAN frames.
 *
 * @note    All state lives in Dtc_ContextType, allocated by the caller.
 */

#ifndef DTC_MANAGER_H
#define DTC_MANAGER_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Configuration constants
 *--------------------------------------------------------------------------*/

/** @brief Number of trouble codes supported by the demonstrator. */
#define DTC_MANAGER_COUNT               (2u)

/** @brief Bytes used to encode one trouble code plus its status byte. */
#define DTC_MANAGER_RECORD_SIZE         (4u)

/** @brief Trouble code raised on engine over temperature, per SYS-006. */
#define DTC_CODE_ENGINE_OVER_TEMP       (0x010001uL)

/** @brief Trouble code raised on low battery voltage, per SYS-006. */
#define DTC_CODE_LOW_BATTERY            (0x010002uL)

/** @brief Status byte reported for a trouble code that is currently active. */
#define DTC_STATUS_ACTIVE               (0x09u)

/** @brief Status byte reported for a trouble code that is not active. */
#define DTC_STATUS_INACTIVE             (0x00u)

/*----------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** @brief Return code shared by every function of this module. */
typedef enum
{
    DTC_E_OK            = 0u,   /**< Success                                */
    DTC_E_NULL_PTR      = 1u,   /**< A required pointer was NULL            */
    DTC_E_NOT_INIT      = 2u,   /**< Context has not been initialised       */
    DTC_E_UNKNOWN_CODE  = 3u,   /**< The trouble code is not in the table   */
    DTC_E_SMALL_BUFFER  = 4u    /**< Destination buffer is too small        */
} Dtc_ReturnType;

/** @brief One entry of the trouble code table. */
typedef struct
{
    uint32_t code;                  /**< Trouble code, 24 bits used         */
    uint8_t  active;                /**< 1 while the fault is present       */
} Dtc_EntryType;

/**
 * @brief Complete state of the trouble code manager.
 *
 * @details The caller allocates one of these and passes its address to every
 *          function. Nothing is stored inside the module itself.
 */
typedef struct
{
    Dtc_EntryType entry[DTC_MANAGER_COUNT];
    uint8_t       initialised;      /**< 1 after a successful init          */
} Dtc_ContextType;

/*----------------------------------------------------------------------------
 * Lifecycle
 *--------------------------------------------------------------------------*/

/**
 * @brief   Populate the trouble code table and mark every code inactive.
 *
 * @param[out] context   Context to initialise.
 *
 * @return  DTC_E_OK on success.
 * @return  DTC_E_NULL_PTR if context is NULL.
 */
Dtc_ReturnType DtcManager_Init(Dtc_ContextType *context);

/*----------------------------------------------------------------------------
 * Fault status
 *--------------------------------------------------------------------------*/

/**
 * @brief   Set or clear the active state of one trouble code.
 *
 * @param[in,out] context   Trouble code context.
 * @param[in]     code      Trouble code to update.
 * @param[in]     active    1 to raise the code, 0 to clear it.
 *
 * @return  DTC_E_OK on success.
 * @return  DTC_E_NULL_PTR if context is NULL.
 * @return  DTC_E_NOT_INIT if the context was never initialised.
 * @return  DTC_E_UNKNOWN_CODE if the code is not in the table.
 */
Dtc_ReturnType DtcManager_SetStatus(Dtc_ContextType *context,
                                    uint32_t         code,
                                    uint8_t          active);

/**
 * @brief   Read the active state of one trouble code.
 *
 * @param[in]  context   Trouble code context.
 * @param[in]  code      Trouble code to query.
 * @param[out] active    Set to 1 when the code is currently active.
 *
 * @return  DTC_E_OK on success.
 * @return  DTC_E_NULL_PTR if any pointer is NULL.
 * @return  DTC_E_NOT_INIT if the context was never initialised.
 * @return  DTC_E_UNKNOWN_CODE if the code is not in the table.
 */
Dtc_ReturnType DtcManager_GetStatus(const Dtc_ContextType *context,
                                    uint32_t               code,
                                    uint8_t               *active);

/**
 * @brief   Clear every trouble code at once.
 *
 * @param[in,out] context   Trouble code context.
 *
 * @return  DTC_E_OK on success.
 * @return  DTC_E_NULL_PTR if context is NULL.
 * @return  DTC_E_NOT_INIT if the context was never initialised.
 */
Dtc_ReturnType DtcManager_ClearAll(Dtc_ContextType *context);

/**
 * @brief   Count how many trouble codes are currently active.
 *
 * @param[in]  context      Trouble code context.
 * @param[out] activeCount  Destination for the number of active codes.
 *
 * @return  DTC_E_OK on success.
 * @return  DTC_E_NULL_PTR if any pointer is NULL.
 * @return  DTC_E_NOT_INIT if the context was never initialised.
 */
Dtc_ReturnType DtcManager_CountActive(const Dtc_ContextType *context,
                                      uint8_t               *activeCount);

/*----------------------------------------------------------------------------
 * Serialisation
 *--------------------------------------------------------------------------*/

/**
 * @brief   Write every active trouble code into a byte buffer.
 *
 * @details Each active code occupies four bytes: three bytes of code, most
 *          significant byte first, followed by one status byte. Inactive
 *          codes are omitted. The layout matches what UDS service
 *          ReadDTCInformation expects, but this module does not build the UDS
 *          response itself.
 *
 * @param[in]  context          Trouble code context.
 * @param[out] buffer           Destination buffer.
 * @param[in]  bufferCapacity   Size of the destination buffer.
 * @param[out] writtenLength    Number of bytes actually written.
 *
 * @return  DTC_E_OK on success, including the case where no code is active
 *          and zero bytes are written.
 * @return  DTC_E_NULL_PTR if any pointer is NULL.
 * @return  DTC_E_NOT_INIT if the context was never initialised.
 * @return  DTC_E_SMALL_BUFFER if the buffer cannot hold every active code.
 */
Dtc_ReturnType DtcManager_SerialiseActive(const Dtc_ContextType *context,
                                          uint8_t               *buffer,
                                          uint8_t                bufferCapacity,
                                          uint8_t               *writtenLength);

#endif /* DTC_MANAGER_H */