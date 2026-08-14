/**
 * @file    isotp.h
 * @brief   Giao dien cong khai cua tang van chuyen ISO 15765-2.
 *
 * @details Chia ban tin dai hon bay byte thanh nhieu khung CAN, va ghep lai
 *          o phia nhan.
 *
 * @note    Header nay la ban giao uoc. No chi lo ra thu ma nguoi dung can:
 *          ma trang thai, ma loi, hai kieu ham noi tang, va sau ham cong khai.
 *          Toan bo trang thai noi bo nam trong isotp.c, ben ngoai khong voi
 *          toi duoc, nen khong the phu thuoc vao chi tiet cai dat.
 *
 * @note    Module nay khong phu thuoc phan cung. No chi include stdint.h,
 *          nho vay kiem thu duoc tren may tinh ma khong can board.
 */

#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Cac gioi han kich thuoc - dung khi thiet ke ca kiem thu bien
 *--------------------------------------------------------------------------*/

/** Ban tin dai nhat module chuyen duoc, tinh bang byte. */
#define ISOTP_MAX_MESSAGE_SIZE      (64u)

/** So byte lon nhat con vua mot khung don. */
#define ISOTP_SINGLE_FRAME_MAX      (7u)

/** So byte du lieu ma khung dau mang theo. */
#define ISOTP_FF_FIRST_PAYLOAD      (6u)

/** Do dai mot khung CAN, tinh bang byte. */
#define ISOTP_CAN_FRAME_SIZE        (8u)

/** Han cho khung dieu khien luong sau khi gui khung dau, don vi mili giay. */
#define ISOTP_TIMEOUT_N_BS_MS       (1000u)

/** Han cho khung noi tiep ke tiep khi dang nhan, don vi mili giay. */
#define ISOTP_TIMEOUT_N_CR_MS       (1000u)

/*----------------------------------------------------------------------------
 * Byte dieu khien - bon loai khung
 *--------------------------------------------------------------------------*/
#define ISOTP_PCI_SINGLE_FRAME      (0x00u)  /**< Ca ban tin trong mot khung */
#define ISOTP_PCI_FIRST_FRAME       (0x10u)  /**< Manh dau cua ban tin dai   */
#define ISOTP_PCI_CONSECUTIVE       (0x20u)  /**< Cac manh tiep theo         */
#define ISOTP_PCI_FLOW_CONTROL      (0x30u)  /**< Ben nhan cho phep gui tiep */

/*----------------------------------------------------------------------------
 * Lenh trong khung dieu khien luong, nam o bon bit thap cua byte dau
 *--------------------------------------------------------------------------*/
#define ISOTP_FC_CONTINUE_TO_SEND   (0x00u)  /**< Cu gui tiep              */
#define ISOTP_FC_WAIT               (0x01u)  /**< Cho them mot lat          */
#define ISOTP_FC_OVERFLOW           (0x02u)  /**< Ben nhan khong chua noi   */

/*----------------------------------------------------------------------------
 * Ma trang thai tra ve
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_OK             = 0u,  /**< Thanh cong                             */
    ISOTP_BUSY           = 1u,  /**< Dang ban voi ban tin khac              */
    ISOTP_ERROR_NULL     = 2u,  /**< Co tham so con tro rong                */
    ISOTP_ERROR_SIZE     = 3u,  /**< Do dai bang khong hoac vuot gioi han   */
    ISOTP_ERROR_STATE    = 4u,  /**< Chua khoi tao, hoac trang thai khong hop */
    ISOTP_ERROR_FRAME    = 5u,  /**< Khung khong dung dinh dang             */
    ISOTP_ERROR_SEQUENCE = 6u   /**< Khung noi tiep den sai thu tu          */
} IsoTp_StatusType;

/*----------------------------------------------------------------------------
 * Ma loi chi tiet, doc bang IsoTp_GetLastError
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_ERR_NONE              = 0u,   /**< Chua ghi nhan loi nao        */
    ISOTP_ERR_NULL_POINTER      = 1u,   /**< Nhan duoc con tro rong       */
    ISOTP_ERR_ZERO_LENGTH       = 2u,   /**< Do dai bang khong            */
    ISOTP_ERR_LENGTH_EXCEEDED   = 3u,   /**< Vuot qua gioi han kich thuoc */
    ISOTP_ERR_INVALID_FRAME     = 4u,   /**< Khong nhan ra loai khung     */
    ISOTP_ERR_SF_BAD_LENGTH     = 5u,   /**< Khung don khai do dai sai    */
    ISOTP_ERR_SEQUENCE_MISMATCH = 6u,   /**< Da mat mot khung o giua      */
    ISOTP_ERR_UNEXPECTED_CF     = 7u,   /**< Khung den sai thoi diem      */
    ISOTP_ERR_FC_OVERFLOW       = 8u,   /**< Ben kia bao khong chua noi   */
    ISOTP_ERR_TX_TIMEOUT        = 9u,   /**< Cho xin phep qua lau         */
    ISOTP_ERR_RX_TIMEOUT        = 10u,  /**< Cho manh ke tiep qua lau     */
    ISOTP_ERR_NOT_INITIALISED   = 11u,  /**< Dung truoc khi khoi tao      */
    ISOTP_ERR_SEND_FAILED       = 12u   /**< Tang duoi tu choi khung      */
} IsoTp_ErrorCodeType;

/*----------------------------------------------------------------------------
 * Hai kieu ham noi module nay voi cac tang xung quanh
 *--------------------------------------------------------------------------*/

/**
 * @brief  Dua mot khung CAN xuong tang duoi.
 * @param  frame  Tam byte can dat len bus.
 * @param  dlc    So byte hop le.
 * @return Bang khong neu khung duoc nhan, khac khong neu bi tu choi.
 *
 * @note   Nguoi goi cung cap ham nay, nho vay ISO-TP khong can biet ben duoi
 *         la driver CAN nao, cung khong can biet dung ma dinh danh gi.
 */
typedef uint8_t (*IsoTp_CanSendType)(const uint8_t *frame, uint8_t dlc);

/**
 * @brief  Bao len tang tren khi da ghep xong mot ban tin.
 * @param  message  Ban tin day du.
 * @param  length   So byte cua no.
 */
typedef void (*IsoTp_RxCallbackType)(const uint8_t *message, uint16_t length);

/*----------------------------------------------------------------------------
 * Giao dien cong khai
 *--------------------------------------------------------------------------*/

/**
 * @brief  Chuan bi module de dung.
 * @param  canSendFunction  Ham dat khung len bus.
 * @param  rxCallback       Ham nhan ban tin da ghep xong.
 * @return ISOTP_OK neu thanh cong, ISOTP_ERROR_NULL neu mot tham so bi rong.
 *
 * @post   Ca chieu gui va chieu nhan deu ve trang thai nghi, moi bo dem ve
 *         khong. Goi lai ham nay la cach an toan de dat lai module.
 */
IsoTp_StatusType IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                            IsoTp_RxCallbackType rxCallback);

/**
 * @brief  Gui mot ban tin, tu chia khung khi can.
 * @param  message        Du lieu can gui.
 * @param  length         So byte, tu mot den ISOTP_MAX_MESSAGE_SIZE.
 * @param  currentTimeMs  Thoi diem hien tai, dung de bat dau dem han cho.
 * @return ISOTP_OK khi da nhan viec. Voi ban tin dai, dieu nay nghia la khung
 *         dau da ra ngoai, phan con lai duoc gui dan trong ham chu ky.
 *         ISOTP_BUSY neu ban tin truoc con dang gui do.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message,
                            uint16_t       length,
                            uint32_t       currentTimeMs);

/**
 * @brief  Xu ly mot khung CAN vua nhan duoc.
 * @param  frame          Tam byte nhan duoc.
 * @param  dlc            So byte hop le, tu mot den tam.
 * @param  currentTimeMs  Thoi diem hien tai.
 * @return ISOTP_OK khi da xu ly, nguoc lai la ly do khong xu ly duoc.
 *
 * @note   Loai khung doc tu bon bit cao cua byte dau, roi khung duoc chuyen
 *         cho ham xu ly tuong ung.
 */
IsoTp_StatusType IsoTp_OnCanFrame(const uint8_t *frame,
                                  uint8_t        dlc,
                                  uint32_t       currentTimeMs);

/**
 * @brief  Ham chu ky, goi deu dan tu vong lap chinh.
 * @param  currentTimeMs  Thoi diem hien tai.
 * @return ISOTP_OK khi chay xong mot chu ky.
 *
 * @note   Gui khung noi tiep ke tiep khi den luot, va bo cuoc voi ban tin da
 *         qua han cho, nho vay module khong bao gio bi ket.
 *
 * @note   Vong lap chinh khong duoc phep bi chan, neu khong cac khung con lai
 *         cua ban tin dai se khong bao gio duoc gui.
 */
IsoTp_StatusType IsoTp_MainFunction(uint32_t currentTimeMs);

/**
 * @brief  Doc ma loi gan nhat.
 * @return Mot gia tri trong IsoTp_ErrorCodeType. Doc khong lam xoa ma loi.
 */
IsoTp_ErrorCodeType IsoTp_GetLastError(void);

/**
 * @brief  Doc so lan gap loi tu luc khoi tao.
 * @return So lan, dem tu khong.
 */
uint32_t IsoTp_GetErrorCounter(void);

#endif /* ISOTP_H */
