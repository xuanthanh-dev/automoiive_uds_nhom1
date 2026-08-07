/**
 * @file    CAN_if.h
 * @brief   ECU Abstraction Layer cho CAN - CHI lam viec gui/nhan frame.
 *
 * @note    Nguyen tac: ham chuc nang gi thi chi lam viec do.
 *          - CanIf CHI gui/nhan frame va BAO KET QUA.
 *          - CanIf KHONG in log, KHONG bat den, KHONG tu treo he thong.
 *          - Viec XU LY loi (in log, thu lai, phuc hoi) do tang tren lam.
 *
 * @note    CanIf KHONG biet tang tren la ai. Tang tren tu dang ky ham
 *          nhan qua CAN_IF_RegisterRxCallback.
 */

#ifndef CAN_IF_H
#define CAN_IF_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Ma trang thai - CanIf BAO loi, khong XU LY loi
 *--------------------------------------------------------------------------*/
typedef enum
{
    CANIF_OK           = 0u,   /* Thanh cong                        */
    CANIF_ERROR_PARAM  = 1u,   /* Tham so sai (NULL, do dai qua 8)  */
    CANIF_ERROR_BUSY   = 2u,   /* Mailbox day, khong gui duoc       */
    CANIF_ERROR_INIT   = 3u,   /* Khoi tao phan cung that bai       */
    CANIF_ERROR_STATE  = 4u    /* Chua khoi tao                     */
} CanIf_StatusType;

/*----------------------------------------------------------------------------
 * Kieu ham nhan frame
 *--------------------------------------------------------------------------*/

/**
 * @brief  Kieu ham tang tren dang ky de nhan frame.
 * @param  id           CAN ID cua frame nhan duoc.
 * @param  data         Con tro du lieu.
 * @param  dlc          So byte hop le.
 * @param  timestampMs  Thoi diem nhan duoc frame, don vi mili giay.
 *
 * @note    CanIf chi bao "co frame den luc nao". Viec loc ID, ghep ban tin,
 *          in log... deu la viec cua tang tren.
 *
 * @note    Moc thoi gian la mot phan cua su kien nhan, giong nhu ID va do dai.
 *          Nho no ma tang tren khong can goi ham thoi gian cua phan cung,
 *          nen kiem thu duoc tren may tinh ma khong can gia lap phan cung.
 */
typedef void (*CanIf_RxCallbackType)(uint32_t       id,
                                     const uint8_t *data,
                                     uint8_t        dlc,
                                     uint32_t       timestampMs);

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao phan cung CAN (cau hinh, filter, start, bat ngat).
 * @return CANIF_OK neu thanh cong, CANIF_ERROR_INIT neu that bai.
 *
 * @note    KHONG goi Error_Handler. Tra ve loi de tang tren quyet dinh
 *          xu ly the nao.
 */
CanIf_StatusType CAN_IF_Init(void);

/**
 * @brief  Gui mot CAN frame.
 * @param  stdId  CAN ID 11 bit.
 * @param  pData  Con tro du lieu (khong duoc NULL).
 * @param  len    So byte (1..8).
 * @return CANIF_OK neu dua vao mailbox thanh cong, nguoc lai ma loi.
 *
 * @note    KHONG in log khi loi. Chi tra ma loi de tang tren xu ly.
 */
CanIf_StatusType CAN_IF_Transmit(uint16_t stdId, const uint8_t *pData, uint8_t len);

/**
 * @brief  Dang ky ham nhan frame cua tang tren.
 * @param  callback  Ham se duoc goi moi khi nhan duoc frame.
 * @return CANIF_OK neu dang ky duoc, CANIF_ERROR_PARAM neu callback NULL.
 *
 * @note    Nho ham nay CanIf KHONG can include header cua tang tren.
 */
CanIf_StatusType CAN_IF_RegisterRxCallback(CanIf_RxCallbackType callback);

#endif /* CAN_IF_H */
