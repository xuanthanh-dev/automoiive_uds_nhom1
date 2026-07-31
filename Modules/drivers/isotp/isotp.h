/**
 * @file    isotp.h
 * @brief   ISO-TP (ISO 15765-2) transport layer tren CAN.
 * @details Chia nho payload UDS lon hon 7 byte thanh nhieu CAN frame,
 *          va ghep lai o phia nhan.
 *          Bon loai frame: Single, First, Consecutive, Flow Control.
 *          Issue: #19 (Single Frame), #21 (Multi-frame FF/CF/FC)
 *          Requirement: SWR-ISO-TP-001, SWR-ISO-TP-002, SYS-003
 *
 * @note    Coding standard:
 *          - Moi ham chi co mot lenh return (single-exit)
 *          - Moi tham so dau vao deu duoc kiem tra
 *          - Moi nhanh if deu co else
 *          - Loi duoc luu vao bo dem de chan doan
 */

#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Hang so giao thuc ISO-TP
 *--------------------------------------------------------------------------*/
#define ISOTP_CAN_FRAME_SIZE        (8u)   /* Mot CAN frame co 8 byte         */
#define ISOTP_SF_MAX_PAYLOAD        (7u)   /* Single Frame chua toi da 7 byte */
#define ISOTP_FF_FIRST_PAYLOAD      (6u)   /* First Frame chua 6 byte dau     */
#define ISOTP_CF_MAX_PAYLOAD        (7u)   /* Consecutive Frame chua 7 byte   */
#define ISOTP_MAX_MESSAGE_SIZE      (64u)  /* Do dai payload lon nhat ho tro  */

/*----------------------------------------------------------------------------
 * Ma loai frame (4 bit cao cua byte dau tien - PCI type)
 *--------------------------------------------------------------------------*/
#define ISOTP_PCI_SINGLE_FRAME      (0x00u)
#define ISOTP_PCI_FIRST_FRAME       (0x10u)
#define ISOTP_PCI_CONSECUTIVE_FRAME (0x20u)
#define ISOTP_PCI_FLOW_CONTROL      (0x30u)

#define ISOTP_PCI_TYPE_MASK         (0xF0u)  /* Mask lay 4 bit loai frame */
#define ISOTP_PCI_VALUE_MASK        (0x0Fu)  /* Mask lay 4 bit gia tri    */

/*----------------------------------------------------------------------------
 * Ma Flow Control (byte thu 2 cua Flow Control frame)
 *--------------------------------------------------------------------------*/
#define ISOTP_FC_CONTINUE_TO_SEND   (0x00u)
#define ISOTP_FC_WAIT               (0x01u)
#define ISOTP_FC_OVERFLOW           (0x02u)

/*----------------------------------------------------------------------------
 * Timeout (don vi ms)
 *--------------------------------------------------------------------------*/
#define ISOTP_TIMEOUT_N_BS_MS       (1000u)  /* Cho Flow Control          */
#define ISOTP_TIMEOUT_N_CR_MS       (1000u)  /* Cho Consecutive Frame tiep */

/*----------------------------------------------------------------------------
 * Ma trang thai tra ve cua ham public
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_OK            = 0u,   /* Thao tac thanh cong        */
    ISOTP_BUSY          = 1u,   /* Dang ban truyen/nhan       */
    ISOTP_ERROR_NULL    = 2u,   /* Con tro NULL               */
    ISOTP_ERROR_SIZE    = 3u,   /* Payload vuot qua gioi han  */
    ISOTP_ERROR_TIMEOUT = 4u,   /* Het thoi gian cho          */
    ISOTP_ERROR_STATE   = 5u,   /* Sai trang thai             */
    ISOTP_ERROR_SEQUENCE = 6u,  /* Sai so thu tu (SN)         */
    ISOTP_ERROR_FRAME   = 7u    /* Loai frame khong hop le    */
} IsoTp_StatusType;

/*----------------------------------------------------------------------------
 * Ma loi noi bo, luu vao bo dem de chan doan (yeu cau: luu moi loi)
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_ERR_NONE              = 0u,
    ISOTP_ERR_NULL_POINTER      = 1u,   /* Ham nhan con tro NULL          */
    ISOTP_ERR_ZERO_LENGTH       = 2u,   /* Do dai bang 0                  */
    ISOTP_ERR_LENGTH_EXCEEDED   = 3u,   /* Do dai vuot buffer             */
    ISOTP_ERR_INVALID_FRAME     = 4u,   /* Byte PCI khong thuoc 4 loai    */
    ISOTP_ERR_SF_BAD_LENGTH     = 5u,   /* Single Frame do dai sai        */
    ISOTP_ERR_SEQUENCE_MISMATCH = 6u,   /* Consecutive Frame sai SN       */
    ISOTP_ERR_UNEXPECTED_CF     = 7u,   /* Nhan CF khi khong o trang thai */
    ISOTP_ERR_FC_OVERFLOW       = 8u,   /* Phia nhan bao tran             */
    ISOTP_ERR_TX_TIMEOUT        = 9u,   /* Het gio cho Flow Control       */
    ISOTP_ERR_RX_TIMEOUT        = 10u,  /* Het gio cho Consecutive Frame  */
    ISOTP_ERR_NOT_INITIALISED   = 11u   /* Chua goi IsoTp_Init            */
} IsoTp_ErrorCodeType;

/*----------------------------------------------------------------------------
 * Kieu ham callback
 *--------------------------------------------------------------------------*/

/** Ham goi len khi nhan xong ban tin day du. */
typedef void (*IsoTp_RxCallbackType)(const uint8_t *message, uint16_t length);

/** Ham gui mot CAN frame xuong tang duoi (CanIf). */
typedef void (*IsoTp_CanSendType)(const uint8_t *frame, uint8_t dlc);

/*----------------------------------------------------------------------------
 * Trang thai may truyen (TX)
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_TX_IDLE       = 0u,   /* Khong truyen                     */
    ISOTP_TX_WAIT_FC    = 1u,   /* Da gui FF, dang cho Flow Control */
    ISOTP_TX_SENDING_CF = 2u    /* Dang gui cac Consecutive Frame   */
} IsoTp_TxStateType;

/*----------------------------------------------------------------------------
 * Trang thai may nhan (RX)
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_RX_IDLE         = 0u,  /* Khong nhan                      */
    ISOTP_RX_RECEIVING_CF = 1u   /* Da nhan FF, dang cho Consecutive */
} IsoTp_RxStateType;

/*----------------------------------------------------------------------------
 * Khoi du lieu phia truyen (TX) - gom chung trong mot struct
 *--------------------------------------------------------------------------*/
typedef struct
{
    IsoTp_TxStateType state;                       /* Trang thai may truyen  */
    uint8_t           buffer[ISOTP_MAX_MESSAGE_SIZE]; /* Du lieu dang gui    */
    uint16_t          totalLength;                 /* Tong so byte can gui   */
    uint16_t          sentIndex;                   /* So byte da gui         */
    uint8_t           sequenceNumber;              /* SN cua CF tiep theo    */
    uint32_t          timerStartMs;                /* Moc thoi gian cho FC   */
} IsoTp_TxContextType;

/*----------------------------------------------------------------------------
 * Khoi du lieu phia nhan (RX) - gom chung trong mot struct
 *--------------------------------------------------------------------------*/
typedef struct
{
    IsoTp_RxStateType state;                       /* Trang thai may nhan    */
    uint8_t           buffer[ISOTP_MAX_MESSAGE_SIZE]; /* Du lieu dang ghep   */
    uint16_t          totalLength;                 /* Tong so byte se nhan   */
    uint16_t          receivedIndex;               /* So byte da nhan        */
    uint8_t           sequenceNumber;              /* SN mong doi tiep theo  */
    uint32_t          timerStartMs;                /* Moc thoi gian cho CF   */
} IsoTp_RxContextType;

/*----------------------------------------------------------------------------
 * Khoi du lieu toan cuc cua module - gom tat ca vao mot struct
 *--------------------------------------------------------------------------*/
typedef struct
{
    uint8_t              isInitialised;   /* 1 sau khi IsoTp_Init thanh cong */
    IsoTp_CanSendType    canSend;         /* Ham gui frame xuong CanIf       */
    IsoTp_RxCallbackType rxCallback;      /* Ham bao len tang tren           */
    IsoTp_TxContextType  tx;              /* Ngu canh truyen                 */
    IsoTp_RxContextType  rx;              /* Ngu canh nhan                   */
    IsoTp_ErrorCodeType  lastError;       /* Loi gan nhat (chan doan)        */
    uint32_t             errorCounter;    /* Tong so lan gap loi             */
} IsoTp_ContextType;

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao tang ISO-TP ve trang thai Idle.
 * @param  canSendFunction  Ham gui CAN frame xuong CanIf (khong duoc NULL).
 * @param  rxCallback       Ham goi len khi nhan xong (khong duoc NULL).
 * @return ISOTP_OK neu thanh cong, ISOTP_ERROR_NULL neu tham so NULL.
 */
IsoTp_StatusType IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                            IsoTp_RxCallbackType rxCallback);

/**
 * @brief  Gui mot ban tin qua ISO-TP.
 * @param  message  Con tro toi du lieu can gui (khong duoc NULL).
 * @param  length   So byte cua du lieu (1..ISOTP_MAX_MESSAGE_SIZE).
 * @return ISOTP_OK, hoac ma loi tuong ung.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message, uint16_t length);

/**
 * @brief  Xu ly mot CAN frame nhan duoc tu tang duoi.
 * @param  frame  Con tro toi 8 byte cua CAN frame (khong duoc NULL).
 * @param  dlc    So byte hop le trong frame (1..8).
 * @return ISOTP_OK, hoac ma loi tuong ung.
 */
IsoTp_StatusType IsoTp_OnCanFrame(const uint8_t *frame, uint8_t dlc);

/**
 * @brief  Ham chu ky, goi tu main loop de gui CF va xu ly timeout.
 * @param  currentTimeMs  Thoi diem hien tai (HAL_GetTick).
 * @return ISOTP_OK, hoac ma loi tuong ung.
 */
IsoTp_StatusType IsoTp_MainFunction(uint32_t currentTimeMs);

/**
 * @brief  Lay ma loi noi bo gan nhat (phuc vu chan doan).
 * @return Ma loi trong IsoTp_ErrorCodeType.
 */
IsoTp_ErrorCodeType IsoTp_GetLastError(void);

/**
 * @brief  Lay tong so lan gap loi ke tu khi khoi tao.
 * @return So dem loi.
 */
uint32_t IsoTp_GetErrorCounter(void);

#endif /* ISOTP_H */
