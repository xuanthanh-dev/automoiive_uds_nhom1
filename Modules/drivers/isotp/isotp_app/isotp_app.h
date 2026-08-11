/**
 * @file    isotp_app.h
 * @brief   Lop keo noi ISO-TP voi CanIf ban da merge vao main.
 *
 * @note    Lop nay KHONG sua CanIf. No chi:
 *            - Dien CAN ID khi ISO-TP nho gui khung
 *            - Nhan khung qua ham CAN_IF_OnFrameReceived (ham yeu trong CanIf)
 *            - In ban tin da ghep va gui nguoc lai
 */

#ifndef ISOTP_APP_H
#define ISOTP_APP_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * CAN ID chan doan theo ISO 15765-4
 *   Board A (mac dinh) : nhan 0x7E0, gui 0x7E8
 *   Board B (ROLE_B)   : nhan 0x7E8, gui 0x7E0
 *--------------------------------------------------------------------------*/
#ifdef ISOTP_APP_ROLE_B
  #define ISOTP_APP_ID_RX   (0x7E8u)
  #define ISOTP_APP_ID_TX   (0x7E0u)
#else
  #define ISOTP_APP_ID_RX   (0x7E0u)
  #define ISOTP_APP_ID_TX   (0x7E8u)
#endif

typedef enum
{
    ISOTP_APP_OK          = 0u,
    ISOTP_APP_ERROR_STATE = 1u,
    ISOTP_APP_ERROR_NULL  = 2u
} IsoTpApp_StatusType;

/**
 * @brief  Khoi tao lop keo va module ISO-TP ben duoi.
 * @return ISOTP_APP_OK neu thanh cong.
 */
IsoTpApp_StatusType IsoTpApp_Init(void);

/**
 * @brief  Ham chu ky, goi deu dan tu vong lap chinh.
 * @param  currentTimeMs  Thoi diem hien tai.
 */
IsoTpApp_StatusType IsoTpApp_MainFunction(uint32_t currentTimeMs);

/**
 * @brief  Chu dong gui mot ban tin qua ISO-TP.
 */
IsoTpApp_StatusType IsoTpApp_SendMessage(const uint8_t *message,
                                         uint16_t       length,
                                         uint32_t       currentTimeMs);

/**
 * @brief  So ban tin da ghep xong tu luc khoi tao.
 */
uint32_t IsoTpApp_GetReceivedCount(void);

#endif /* ISOTP_APP_H */
