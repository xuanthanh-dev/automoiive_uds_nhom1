#ifndef APP_DIAG_H
#define APP_DIAG_H

#include <stdint.h>

/**
 * @file    app_diag.h
 * @brief   Giao dien Application Diagnostic.
 *
 * Architecture:
 *
 *   User Interface
 *        |
 *        v
 *    AppDiag
 *        |
 *        v
 *   IsoTpApp_SendMessage()
 *        |
 *        v
 *      ISO-TP
 *        |
 *        v
 *      CAN_IF
 */

/**
 * @brief Cac chuc nang diagnostic ma User Interface co the lua chon.
 */
typedef enum
{
    APP_DIAG_CASE_READ_VIN = 0,
    APP_DIAG_CASE_READ_SW_VERSION,
    APP_DIAG_CASE_ECU_RESET,
    APP_DIAG_CASE_TESTER_PRESENT
} AppDiag_CaseType;

/**
 * @brief Trang thai cua Application Diagnostic.
 */
typedef enum
{
    APP_DIAG_OK = 0,
    APP_DIAG_ERROR_NULL,
    APP_DIAG_ERROR_INVALID_CASE,
    APP_DIAG_ERROR_NOT_INITIALIZED,
    APP_DIAG_ERROR_TRANSMIT
} AppDiag_StatusType;

/**
 * @brief Khoi tao Application Diagnostic.
 */
void AppDiag_Init(void);

/**
 * @brief Hien thi menu Diagnostic qua UART.
 */
void AppDiag_ShowMenu(void);

/**
 * @brief Xu ly case do User Interface lua chon.
 *
 * @param caseId Case diagnostic can thuc hien.
 *
 * @return APP_DIAG_OK neu xu ly thanh cong.
 */
AppDiag_StatusType AppDiag_HandleCase(AppDiag_CaseType caseId);

/**
 * @brief Gui request Read VIN - DID 0xF190.
 *
 * Request:
 *     22 F1 90
 *
 * Positive response:
 *     62 F1 90 <VIN>
 */
AppDiag_StatusType AppDiag_ReadVIN(void);

/**
 * @brief Gui request Read Software Version - DID 0xF187.
 *
 * Request:
 *     22 F1 87
 *
 * Positive response:
 *     62 F1 87 <Software Version>
 */
AppDiag_StatusType AppDiag_ReadSWVersion(void);

/**
 * @brief Gui request ECU Reset.
 *
 * Request:
 *     11 01
 */
AppDiag_StatusType AppDiag_ECUReset(void);

/**
 * @brief Gui request Tester Present.
 *
 * Request:
 *     3E 00
 */
AppDiag_StatusType AppDiag_TesterPresent(void);

/**
 * @brief Hien thi trang thai Application Diagnostic.
 */
void AppDiag_ShowStatus(void);

#endif /* APP_DIAG_H */