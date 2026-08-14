#ifndef APP_CAN_H
#define APP_CAN_H

#include <stdint.h>

/**
 * @brief Các chức năng CAN mà User Interface có thể lựa chọn.
 */
typedef enum
{
    APP_CAN_CASE_SEND_ENGINE_STATUS = 0,
    APP_CAN_CASE_SEND_TEST_MESSAGE,
    APP_CAN_CASE_READ_CAN_FRAME,
    APP_CAN_CASE_SHOW_STATUS
} AppCan_CaseType;

/**
 * @brief Trạng thái của Application CAN.
 */
typedef enum
{
    APP_CAN_OK = 0,
    APP_CAN_ERROR_NULL,
    APP_CAN_ERROR_INVALID_CASE,
    APP_CAN_ERROR_NOT_INITIALIZED,
    APP_CAN_ERROR_TRANSMIT
} AppCan_StatusType;

/**
 * @brief Khởi tạo Application CAN.
 *
 * @note
 * Hàm này không khởi tạo phần cứng CAN.
 * CAN hardware được quản lý bởi CAN_IF.
 */
void AppCan_Init(void);

/**
 * @brief Hiển thị menu giao diện CAN qua UART.
 */
void AppCan_ShowMenu(void);

/**
 * @brief Xử lý lựa chọn của người dùng.
 *
 * @param caseId Case được chọn từ User Interface.
 *
 * @return APP_CAN_OK nếu xử lý thành công.
 */
AppCan_StatusType AppCan_HandleCase(AppCan_CaseType caseId);

/**
 * @brief Gửi Engine Status thông qua tầng Application.
 *
 * @return APP_CAN_OK nếu yêu cầu gửi được chấp nhận.
 */
AppCan_StatusType AppCan_SendEngineStatus(void);

/**
 * @brief Gửi một CAN test message.
 *
 * @param data Dữ liệu cần gửi.
 * @param length Số byte dữ liệu.
 *
 * @return APP_CAN_OK nếu gửi thành công.
 */
AppCan_StatusType AppCan_SendTestMessage(const uint8_t *data,
                                         uint16_t length);

/**
 * @brief Hiển thị trạng thái CAN qua UART.
 */
void AppCan_ShowStatus(void);

#endif /* APP_CAN_H */