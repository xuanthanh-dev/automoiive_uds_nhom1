/**
 * @file    isotp.h
 * @brief   ISO-TP (ISO 15765-2) transport layer tren CAN.
 * @details Chia nho payload UDS lon hon 7 byte thanh nhieu CAN frame,
 *          va ghep lai o phia nhan.
 *          Bon loai frame: Single, First, Consecutive, Flow Control.
 *          Issue: #19 (Single Frame), #21 (Multi-frame FF/CF/FC)
 *          Requirement: SWR-ISO-TP-001, SWR-ISO-TP-002, SYS-003
 */

#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Hang so giao thuc ISO-TP
 *--------------------------------------------------------------------------*/
#define ISOTP_CAN_FRAME_SIZE        (8u)   /* Mot CAN frame co 8 byte        */
#define ISOTP_SF_MAX_PAYLOAD        (7u)   /* Single Frame chua toi da 7 byte */
#define ISOTP_FF_FIRST_PAYLOAD      (6u)   /* First Frame chua 6 byte dau    */
#define ISOTP_CF_MAX_PAYLOAD        (7u)   /* Consecutive Frame chua 7 byte  */
#define ISOTP_MAX_MESSAGE_SIZE      (64u)  /* Do dai payload lon nhat ho tro */

/*----------------------------------------------------------------------------
 * Ma loai frame (4 bit cao cua byte dau tien - PCI type)
 *--------------------------------------------------------------------------*/
#define ISOTP_PCI_SINGLE_FRAME      (0x00u)  /* 0x0X - Single Frame          */
#define ISOTP_PCI_FIRST_FRAME       (0x10u)  /* 0x1X - First Frame           */
#define ISOTP_PCI_CONSECUTIVE_FRAME (0x20u)  /* 0x2X - Consecutive Frame     */
#define ISOTP_PCI_FLOW_CONTROL      (0x30u)  /* 0x3X - Flow Control          */

#define ISOTP_PCI_TYPE_MASK         (0xF0u)  /* Mask lay 4 bit loai frame    */
#define ISOTP_PCI_VALUE_MASK        (0x0Fu)  /* Mask lay 4 bit gia tri       */

/*----------------------------------------------------------------------------
 * Ma Flow Control (byte thu 2 cua Flow Control frame)
 *--------------------------------------------------------------------------*/
#define ISOTP_FC_CONTINUE_TO_SEND   (0x00u)  /* Cho phep gui tiep (CTS)      */
#define ISOTP_FC_WAIT               (0x01u)  /* Yeu cau cho                  */
#define ISOTP_FC_OVERFLOW           (0x02u)  /* Tran buffer                  */

/*----------------------------------------------------------------------------
 * Timeout (don vi ms) - SWR-ISO-TP-002
 *--------------------------------------------------------------------------*/
#define ISOTP_TIMEOUT_N_BS_MS       (1000u)  /* Cho Flow Control tu phia nhan */
#define ISOTP_TIMEOUT_N_CR_MS       (1000u)  /* Cho Consecutive Frame tiep theo */

/*----------------------------------------------------------------------------
 * Ma trang thai tra ve
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_OK          = 0u,   /* Thao tac thanh cong           */
    ISOTP_BUSY        = 1u,   /* Dang ban truyen/nhan          */
    ISOTP_ERROR_NULL  = 2u,   /* Con tro NULL                  */
    ISOTP_ERROR_SIZE  = 3u,   /* Payload vuot qua gioi han     */
    ISOTP_ERROR_TIMEOUT = 4u  /* Het thoi gian cho             */
} IsoTp_StatusType;

/*----------------------------------------------------------------------------
 * Kieu ham callback bao co ban tin day du da nhan xong
 *--------------------------------------------------------------------------*/
typedef void (*IsoTp_RxCallbackType)(const uint8_t *message, uint16_t length);

/*----------------------------------------------------------------------------
 * Kieu ham gui mot CAN frame xuong tang duoi (CanIf)
 *--------------------------------------------------------------------------*/
typedef void (*IsoTp_CanSendType)(const uint8_t *frame, uint8_t dlc);

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao tang ISO-TP ve trang thai Idle.
 * @param  canSendFunction  Ham gui CAN frame xuong CanIf.
 * @param  rxCallback       Ham goi len khi nhan xong ban tin day du.
 */
void IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                IsoTp_RxCallbackType rxCallback);

/**
 * @brief  Gui mot ban tin qua ISO-TP.
 * @details Neu payload <= 7 byte: gui bang Single Frame.
 *          Neu payload > 7 byte: gui First Frame roi cho Flow Control,
 *          sau do gui cac Consecutive Frame.
 * @param  message  Con tro toi du lieu can gui.
 * @param  length   So byte cua du lieu.
 * @return ISOTP_OK neu bat dau gui thanh cong.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message, uint16_t length);

/**
 * @brief  Xu ly mot CAN frame nhan duoc tu tang duoi.
 * @details Phan loai frame (SF/FF/CF/FC) va xu ly tuong ung.
 *          Khi nhan du mot ban tin hoan chinh, goi rxCallback.
 * @param  frame  Con tro toi 8 byte cua CAN frame.
 * @param  dlc    So byte hop le trong frame.
 */
void IsoTp_OnCanFrame(const uint8_t *frame, uint8_t dlc);

/**
 * @brief  Ham chu ky, goi tu main loop de xu ly timeout.
 * @param  currentTimeMs  Thoi diem hien tai (HAL_GetTick).
 */
void IsoTp_MainFunction(uint32_t currentTimeMs);

#endif /* ISOTP_H */
