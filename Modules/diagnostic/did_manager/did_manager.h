/**
 * @file    did_manager.h
 * @brief   Lookup table for diagnostic data identifiers.
 *
 * @details Implements SWR-DID-001 and SWR-DID-002. The module maps a data
 *          identifier onto the bytes that describe it, whether those bytes
 *          are a constant such as the vehicle identification number or a
 *          live value taken from the application layer.
 *
 *          The module contains no diagnostic protocol logic. It does not know
 *          what service 0x22 is, how a positive response is built or what a
 *          negative response code means. It answers one question only: what
 *          are the bytes for this identifier.
 *
 * @note    The table itself is constant, so no context structure is needed.
 *          Live values are obtained from an application context supplied by
 *          the caller on every call.
 */

#ifndef DID_MANAGER_H
#define DID_MANAGER_H

#include <stdint.h>
#include "app_engine.h"

/*----------------------------------------------------------------------------
 * Supported identifiers
 *--------------------------------------------------------------------------*/

/** @brief Vehicle identification number, per SYS-003. */
#define DID_VIN                         (0xF190u)

/** @brief Software version string, per SYS-004. */
#define DID_SOFTWARE_VERSION            (0xF187u)

/** @brief Live vehicle speed, per SYS-005. */
#define DID_VEHICLE_SPEED               (0x0101u)

/** @brief Live engine speed, per SYS-005. */
#define DID_ENGINE_SPEED                (0x0102u)

/** @brief Live engine temperature, per SYS-005. */
#define DID_ENGINE_TEMPERATURE          (0x0103u)

/** @brief Live battery voltage, per SYS-005. */
#define DID_BATTERY_VOLTAGE             (0x0104u)

/*----------------------------------------------------------------------------
 * Data lengths
 *--------------------------------------------------------------------------*/

/** @brief Length of the vehicle identification number, in bytes. */
#define DID_VIN_LENGTH                  (17u)

/** @brief Length of the software version string, in bytes. */
#define DID_SOFTWARE_VERSION_LENGTH     (8u)

/** @brief Longest data block any identifier can return. */
#define DID_MAX_DATA_LENGTH             (17u)

/*----------------------------------------------------------------------------
 * Types
 *--------------------------------------------------------------------------*/

/** @brief Return code shared by every function of this module. */
typedef enum
{
    DID_E_OK            = 0u,   /**< Success                                */
    DID_E_NULL_PTR      = 1u,   /**< A required pointer was NULL            */
    DID_E_NOT_SUPPORTED = 2u,   /**< The identifier is not in the table     */
    DID_E_SMALL_BUFFER  = 3u,   /**< Destination buffer is too small        */
    DID_E_SOURCE_FAILED = 4u    /**< The application layer refused the read */
} Did_ReturnType;

/*----------------------------------------------------------------------------
 * Interface
 *--------------------------------------------------------------------------*/

/**
 * @brief   Report whether an identifier exists in the table.
 *
 * @param[in]  dataIdentifier   Identifier to look up.
 * @param[out] supported        Set to 1 when the identifier is known.
 *
 * @return  DID_E_OK on success.
 * @return  DID_E_NULL_PTR if supported is NULL.
 */
Did_ReturnType DidManager_IsSupported(uint16_t  dataIdentifier,
                                      uint8_t  *supported);

/**
 * @brief   Write the data belonging to an identifier into a buffer.
 *
 * @details Constant identifiers are copied from internal tables. Live
 *          identifiers are read from the application context supplied by the
 *          caller, which keeps this module free of any simulation logic.
 *
 *          Multi byte values are stored most significant byte first, matching
 *          the byte order used everywhere else in the project.
 *
 * @param[in]  dataIdentifier   Identifier to read.
 * @param[in]  appContext       Application context providing live values.
 * @param[out] buffer           Destination buffer.
 * @param[in]  bufferCapacity   Size of the destination buffer.
 * @param[out] writtenLength    Number of bytes actually written.
 *
 * @return  DID_E_OK on success.
 * @return  DID_E_NULL_PTR if any pointer is NULL.
 * @return  DID_E_NOT_SUPPORTED if the identifier is not in the table.
 * @return  DID_E_SMALL_BUFFER if the buffer cannot hold the data.
 * @return  DID_E_SOURCE_FAILED if the application layer refused the read.
 */
Did_ReturnType DidManager_ReadData(uint16_t                     dataIdentifier,
                                   const AppEngine_ContextType *appContext,
                                   uint8_t                     *buffer,
                                   uint8_t                      bufferCapacity,
                                   uint8_t                     *writtenLength);

#endif /* DID_MANAGER_H */