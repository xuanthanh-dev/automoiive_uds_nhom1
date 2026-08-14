/**
 * @file    app_engine.c
 * @brief   Application Engine - quan ly va gui EngineStatus.
 *
 * Architecture:
 *
 *   User Interface
 *        |
 *        v
 *    AppEngine
 *        |
 *        v
 *      AppCan
 *        |
 *        v
 *      CAN_IF
 *        |
 *        v
 *       CAN
 */

#include "app_engine.h"
#include "app_can.h"
#include "uart_log.h"

#include <stdio.h>

/*----------------------------------------------------------------------------
 * Private variables
 *----------------------------------------------------------------------------*/

static uint8_t appEngineInitialized = 0U;

static AppEngine_StatusDataType appEngineStatus =
{
    .vehicleSpeed  = 0U,
    .engineRPM     = 0U,
    .engineTemp    = 0U,
    .batteryVoltage = 0U
};

static uint32_t appEngineLastTxTime = 0U;
static uint32_t appEngineTxCount = 0U;
static uint32_t appEngineErrorCount = 0U;

/*----------------------------------------------------------------------------
 * Private functions
 *----------------------------------------------------------------------------*/

/**
 * @brief Encode EngineStatus thanh CAN payload 8 byte.
 *
 * Format:
 *
 * Byte 0-1 : VehicleSpeed
 * Byte 2-3 : EngineRPM
 * Byte 4-5 : EngineTemp
 * Byte 6-7 : BatteryVoltage
 *
 * Cac gia tri duoc encode theo Big Endian.
 */
static void AppEngine_EncodeStatus(uint8_t *data)
{
    if (data == NULL)
    {
        /* Khong lam gi neu pointer NULL */
    }
    else
    {
        data[0] = (uint8_t)((appEngineStatus.vehicleSpeed >> 8U) & 0xFFU);
        data[1] = (uint8_t)(appEngineStatus.vehicleSpeed & 0xFFU);

        data[2] = (uint8_t)((appEngineStatus.engineRPM >> 8U) & 0xFFU);
        data[3] = (uint8_t)(appEngineStatus.engineRPM & 0xFFU);

        data[4] = (uint8_t)((appEngineStatus.engineTemp >> 8U) & 0xFFU);
        data[5] = (uint8_t)(appEngineStatus.engineTemp & 0xFFU);

        data[6] = (uint8_t)((appEngineStatus.batteryVoltage >> 8U) & 0xFFU);
        data[7] = (uint8_t)(appEngineStatus.batteryVoltage & 0xFFU);
    }
}

/**
 * @brief Log EngineStatus ra UART.
 */
static void AppEngine_LogTx(const uint8_t *data)
{
    char buffer[160];

    if (data == NULL)
    {
        /* Khong log neu NULL */
    }
    else
    {
        (void)snprintf(
            buffer,
            sizeof(buffer),
            "[ENGINE] TX ID: 0x%03X | "
            "Data: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
            APP_ENGINE_CAN_ID,
            data[0],
            data[1],
            data[2],
            data[3],
            data[4],
            data[5],
            data[6],
            data[7]
        );

        uartlog(buffer);
    }
}

/*----------------------------------------------------------------------------
 * Public API
 *----------------------------------------------------------------------------*/

/**
 * @brief Khoi tao Application Engine.
 */
void AppEngine_Init(void)
{
    appEngineInitialized = 1U;

    appEngineLastTxTime = 0U;
    appEngineTxCount = 0U;
    appEngineErrorCount = 0U;

    /*
     * Gia tri demo ban dau.
     * Co the thay doi thong qua AppEngine_SetStatus().
     */
    appEngineStatus.vehicleSpeed = 60U;
    appEngineStatus.engineRPM = 2000U;
    appEngineStatus.engineTemp = 90U;
    appEngineStatus.batteryVoltage = 12000U;

    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("       ENGINE APPLICATION READY\r\n");
    uartlog("========================================\r\n");

    AppEngine_ShowStatus();
}

/**
 * @brief Xu ly chu ky EngineStatus.
 *
 * EngineStatus duoc gui moi 1000 ms.
 */
AppEngine_StatusType AppEngine_MainFunction(uint32_t currentTimeMs)
{
    AppEngine_StatusType status;

    if (appEngineInitialized == 0U)
    {
        status = APP_ENGINE_ERROR_NOT_INITIALIZED;
    }
    else
    {
        status = APP_ENGINE_OK;

        if ((currentTimeMs - appEngineLastTxTime)
            >= APP_ENGINE_CYCLE_TIME_MS)
        {
            status = AppEngine_SendStatus();

            if (status == APP_ENGINE_OK)
            {
                appEngineLastTxTime = currentTimeMs;
            }
            else
            {
                appEngineErrorCount++;
            }
        }
        else
        {
            /* Chua den chu ky gui tiep theo */
        }
    }

    return status;
}

/**
 * @brief Gui EngineStatus ngay lap tuc.
 */
AppEngine_StatusType AppEngine_SendStatus(void)
{
    AppEngine_StatusType status;
    AppCan_StatusType appCanStatus;
    uint8_t data[APP_ENGINE_CAN_DLC];

    if (appEngineInitialized == 0U)
    {
        status = APP_ENGINE_ERROR_NOT_INITIALIZED;
    }
    else
    {
        AppEngine_EncodeStatus(data);

        /*
         * AppEngine khong truy cap CAN_IF truc tiep.
         * Du lieu duoc dua xuong AppCan.
         */
        appCanStatus = AppCan_Send(
            APP_ENGINE_CAN_ID,
            data,
            APP_ENGINE_CAN_DLC
        );

        if (appCanStatus == APP_CAN_OK)
        {
            appEngineTxCount++;

            AppEngine_LogTx(data);

            status = APP_ENGINE_OK;
        }
        else
        {
            appEngineErrorCount++;

            uartlog("[ENGINE] ERROR: CAN transmit failed\r\n");

            status = APP_ENGINE_ERROR_TRANSMIT;
        }
    }

    return status;
}

/**
 * @brief Cap nhat EngineStatus.
 */
AppEngine_StatusType AppEngine_SetStatus(
    const AppEngine_StatusDataType *status)
{
    AppEngine_StatusType result;

    if (appEngineInitialized == 0U)
    {
        result = APP_ENGINE_ERROR_NOT_INITIALIZED;
    }
    else if (status == NULL)
    {
        result = APP_ENGINE_ERROR_NULL;
    }
    else
    {
        appEngineStatus.vehicleSpeed =
            status->vehicleSpeed;

        appEngineStatus.engineRPM =
            status->engineRPM;

        appEngineStatus.engineTemp =
            status->engineTemp;

        appEngineStatus.batteryVoltage =
            status->batteryVoltage;

        result = APP_ENGINE_OK;
    }

    return result;
}

/**
 * @brief Lay EngineStatus hien tai.
 */
AppEngine_StatusType AppEngine_GetStatus(
    AppEngine_StatusDataType *status)
{
    AppEngine_StatusType result;

    if (appEngineInitialized == 0U)
    {
        result = APP_ENGINE_ERROR_NOT_INITIALIZED;
    }
    else if (status == NULL)
    {
        result = APP_ENGINE_ERROR_NULL;
    }
    else
    {
        status->vehicleSpeed =
            appEngineStatus.vehicleSpeed;

        status->engineRPM =
            appEngineStatus.engineRPM;

        status->engineTemp =
            appEngineStatus.engineTemp;

        status->batteryVoltage =
            appEngineStatus.batteryVoltage;

        result = APP_ENGINE_OK;
    }

    return result;
}

/**
 * @brief Hien thi EngineStatus ra UART.
 */
void AppEngine_ShowStatus(void)
{
    char buffer[160];

    if (appEngineInitialized == 0U)
    {
        uartlog("[ENGINE] ERROR: Not initialized\r\n");
    }
    else
    {
        uartlog("\r\n");
        uartlog("----------------------------------------\r\n");
        uartlog("          ENGINE STATUS\r\n");
        uartlog("----------------------------------------\r\n");

        (void)snprintf(
            buffer,
            sizeof(buffer),
            " Vehicle Speed   : %u km/h\r\n",
            (unsigned)appEngineStatus.vehicleSpeed
        );
        uartlog(buffer);

        (void)snprintf(
            buffer,
            sizeof(buffer),
            " Engine RPM      : %u rpm\r\n",
            (unsigned)appEngineStatus.engineRPM
        );
        uartlog(buffer);

        (void)snprintf(
            buffer,
            sizeof(buffer),
            " Engine Temp     : %u C\r\n",
            (unsigned)appEngineStatus.engineTemp
        );
        uartlog(buffer);

        (void)snprintf(
            buffer,
            sizeof(buffer),
            " Battery Voltage : %u mV\r\n",
            (unsigned)appEngineStatus.batteryVoltage
        );
        uartlog(buffer);

        uartlog("----------------------------------------\r\n");
    }
}

/**
 * @brief Hien thi menu User Interface cho Engine.
 */
void AppEngine_ShowMenu(void)
{
    uartlog("\r\n");
    uartlog("========================================\r\n");
    uartlog("          ENGINE APPLICATION\r\n");
    uartlog("========================================\r\n");
    uartlog(" [0] Show Engine Status\r\n");
    uartlog(" [1] Send Engine Status Now\r\n");
    uartlog(" [2] Start/Continue Cyclic Transmission\r\n");
    uartlog("========================================\r\n");
}