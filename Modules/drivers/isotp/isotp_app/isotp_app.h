/**
 * @file    isotp_app.h
 * @brief   Lop keo noi ISO-TP voi tang giao tiep CAN.
 *
 * @details ISO-TP biet chia va ghep ban tin, nhung co y khong biet gi ve ma
 *          dinh danh CAN hay driver ben duoi. Lop nay lap ba cho trong do:
 *            - dien ma dinh danh khi gui khung di
 *            - loc khung den theo ma dinh danh
 *            - quyet dinh lam gi voi ban tin sau khi ghep xong
 *
 * @note    Day la lop keo cho giai doan chi co tang van chuyen. Khi cac dich
 *          vu chan doan xuat hien, no se duoc thay bang mot bo dinh tuyen,
 *          va ca isotp.c lan tang giao tiep CAN deu khong phai sua.
 */

#ifndef ISOTP_APP_H
#define ISOTP_APP_H

#include <stdint.h>
#include "isotp.h"

/*----------------------------------------------------------------------------
 * Ma dinh danh chan doan.
 * Hai board phai nguoc nhau: ben nay gui gi thi ben kia nghe cai do.
 * Dinh nghia ISOTP_APP_ROLE_B khi bien dich cho board thu hai.
 *--------------------------------------------------------------------------*/
#ifdef ISOTP_APP_ROLE_B
  #define ISOTP_APP_ID_RX   (0x7E8u)   /**< Board thu hai nghe o day  */
  #define ISOTP_APP_ID_TX   (0x7E0u)   /**< Board thu hai gui ra day  */
#else
  #define ISOTP_APP_ID_RX   (0x7E0u)   /**< Mac dinh nghe o day       */
  #define ISOTP_APP_ID_TX   (0x7E8u)   /**< Mac dinh gui ra day       */
#endif

/*----------------------------------------------------------------------------
 * Ma trang thai tra ve
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_APP_OK          = 0u,  /**< Thanh cong                            */
    ISOTP_APP_ERROR_STATE = 1u,  /**< Chua khoi tao, hoac tang duoi bao loi */
    ISOTP_APP_ERROR_NULL  = 2u   /**< Co tham so con tro rong               */
} IsoTpApp_StatusType;

/*----------------------------------------------------------------------------
 * Giao dien cong khai
 *--------------------------------------------------------------------------*/

/**
 * @brief  Chuan bi lop nay va module ISO-TP ben duoi.
 * @return ISOTP_APP_OK neu thanh cong.
 */
IsoTpApp_StatusType IsoTpApp_Init(void);

/**
 * @brief  Ham chu ky, goi deu dan tu vong lap chinh.
 * @param  currentTimeMs  Thoi diem hien tai.
 * @return ISOTP_APP_OK khi chay xong mot chu ky.
 *
 * @note   Moi viec cham hoac de long nhau deu dien ra o day: lay khung khoi
 *         hang doi nhan, gui ban tin doi lai, va in log. Khong viec nao trong
 *         so do an toan neu lam trong ngat.
 */
IsoTpApp_StatusType IsoTpApp_MainFunction(uint32_t currentTimeMs);

/**
 * @brief  Gui mot ban tin do dai bat ky qua ISO-TP.
 * @param  message        Du lieu can gui.
 * @param  length         So byte.
 * @param  currentTimeMs  Thoi diem hien tai.
 * @return ISOTP_APP_OK khi viec gui duoc nhan.
 */
IsoTpApp_StatusType IsoTpApp_SendMessage(const uint8_t *message,
                                         uint16_t       length,
                                         uint32_t       currentTimeMs);

/**
 * @brief  Cho biet da nhan duoc bao nhieu ban tin day du tu luc khoi tao.
 * @return So luong, dem tu khong.
 */
uint32_t IsoTpApp_GetReceivedCount(void);

#endif /* ISOTP_APP_H */
