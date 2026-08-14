/**
 * @file    app_engine.h
 * @brief   Application Engine - quan ly du lieu trang thai dong co.
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

#ifndef APP_ENGINE_H
#define APP_ENGINE_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Cau hinh Engine Status
 *--------------------------------------------------------------------------*/

/** CAN ID cua ban tin EngineStatus */
#define APP_ENGINE_CAN_ID          (0x100U)

/** DLC cua ban tin EngineStatus */
#define APP_ENGINE_CAN_DLC         (8U)

/** Chu ky gui EngineStatus: 1000 ms */
#define APP_ENGINE_CYCLE_TIME_MS   (1000U)

/*----------------------------------------------------------------------------
 * Du lieu trang thai xe
 *--------------------------------------------------------------------------*/

/**
 * @brief Du lieu trang thai dong co / xe.
 */
typedef struct
{
    uint16_t vehicleSpeed;
    uint16_t engineRPM;
    uint16_t engineTemp;
    uint16_t batteryVoltage;

} AppEngine_StatusDataType;

/*----------------------------------------------------------------------------
 * Trang thai module
 *--------------------------------------------------------------------------*/

typedef enum
{
    APP_ENGINE_OK = 0,
    APP_ENGINE_ERROR_NULL,
    APP_ENGINE_ERROR_NOT_INITIALIZED,
    APP_ENGINE_ERROR_TRANSMIT

} AppEngine_StatusType;

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief Khoi tao Application Engine.
 */
void AppEngine_Init(void);

/**
 * @brief Xu ly chu ky Engine Status.
 *
 * Ham nay nen duoc goi thuong xuyen trong main loop.
 * Module se tu dong gui EngineStatus moi 1000 ms.
 *
 * @param currentTimeMs Thoi gian hien tai tinh bang ms.
 *
 * @return Trang thai xu ly.
 */
AppEngine_StatusType AppEngine_MainFunction(uint32_t currentTimeMs);

/**
 * @brief Gui Engine Status ngay lap tuc.
 *
 * @return APP_ENGINE_OK neu gui thanh cong.
 */
AppEngine_StatusType AppEngine_SendStatus(void);

/**
 * @brief Cap nhat du lieu Engine Status.
 *
 * @param status Con tro toi du lieu moi.
 *
 * @return Trang thai xu ly.
 */
AppEngine_StatusType AppEngine_SetStatus(
    const AppEngine_StatusDataType *status
);

/**
 * @brief Lay du lieu Engine Status hien tai.
 *
 * @param status Noi nhan du lieu.
 *
 * @return Trang thai xu ly.
 */
AppEngine_StatusType AppEngine_GetStatus(
    AppEngine_StatusDataType *status
);

/**
 * @brief Hien thi Engine Status ra UART.
 *
 * Dung cho User Interface / debug.
 */
void AppEngine_ShowStatus(void);

/**
 * @brief Hien thi menu dieu khien Engine.
 */
void AppEngine_ShowMenu(void);

#endif /* APP_ENGINE_H */