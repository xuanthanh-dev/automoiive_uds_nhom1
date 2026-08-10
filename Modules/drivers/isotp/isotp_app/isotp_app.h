/**
 * @file    isotp_app.h
 * @brief   Lop keo toi thieu de tich hop ISO-TP vao project (TUAN 3-4).
 * @details CHUA CO UDS. Chi chung minh ISO-TP chia/ghep ban tin dung
 *          tren phan cung that.
 *
 *          Che do hoat dong: ECHO
 *            - Nhan mot ban tin (dai bao nhieu cung duoc, toi 64 byte)
 *            - In ra UART
 *            - Gui nguoc lai chinh ban tin do
 *          => Chung minh ca 2 chieu cua ISO-TP deu hoat dong.
 *
 *          Tuan 5-6 se thay lop nay bang diag_router + uds.
 *
 * @note    Chi can 3 module: CAN_if, isotp, va file nay.
 */

#ifndef ISOTP_APP_H
#define ISOTP_APP_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * CAN ID
 * LUU Y: hai board phai dat NGUOC nhau.
 *   Board A: RX = 0x7E0, TX = 0x7E8
 *   Board B: RX = 0x7E8, TX = 0x7E0
 * Doi bang cach dinh nghia ISOTP_APP_ROLE_B truoc khi include.
 *--------------------------------------------------------------------------*/
#ifdef ISOTP_APP_ROLE_B
  #define ISOTP_APP_ID_RX   (0x7E8u)
  #define ISOTP_APP_ID_TX   (0x7E0u)
#else
  #define ISOTP_APP_ID_RX   (0x7E8u)
  #define ISOTP_APP_ID_TX   (0x7E8u)
#endif

typedef enum
{
    ISOTP_APP_OK          = 0u,
    ISOTP_APP_ERROR_STATE = 1u,
    ISOTP_APP_ERROR_NULL  = 2u
} IsoTpApp_StatusType;

/** @brief Khoi tao (tu goi IsoTp_Init ben trong). */
IsoTpApp_StatusType IsoTpApp_Init(void);

/**
 * @brief  Ham chu ky, goi tu main loop.
 * @param  currentTimeMs  HAL_GetTick().
 */
IsoTpApp_StatusType IsoTpApp_MainFunction(uint32_t currentTimeMs);

/**
 * @brief  Ham nhan frame, dang ky voi CanIf luc khoi tao.
 * @param  id    CAN ID cua frame.
 * @param  data  Du lieu frame.
 * @param  dlc   So byte.
 * @param  timestampMs  Moc thoi gian CanIf dua sang.
 *
 * @note    Duoc CanIf goi TU NGAT. Chu ky phai khop CanIf_RxCallbackType.
 *          Khong tra ve gi vi trong ngat khong ai nhan ket qua.
 */
void IsoTpApp_OnCanRx(uint32_t       id,
                      const uint8_t *data,
                      uint8_t        dlc,
                      uint32_t       timestampMs);

/**
 * @brief  Chu dong gui mot ban tin qua ISO-TP (dung de test tu board gui).
 * @param  message        Du lieu can gui.
 * @param  length         So byte (1..64).
 * @param  currentTimeMs  HAL_GetTick().
 */
IsoTpApp_StatusType IsoTpApp_SendMessage(const uint8_t *message,
                                         uint16_t       length,
                                         uint32_t       currentTimeMs);

/** @brief So ban tin da ghep thanh cong (dung de kiem tra). */
uint32_t IsoTpApp_GetReceivedCount(void);

#endif /* ISOTP_APP_H */
