/**
 * @file    isotp.c
 * @brief   Trien khai tang van chuyen ISO 15765-2TP (ISO 15765-2) tren CAN.
 * @details Issue: #19 (Single Frame), #21 (Multi-frame FF/CF/FC)
 *          Requirement: SWR-ISO-TP-001, SWR-ISO-TP-002, SYS-003
 *
 * @note    Coding standard (production, MISRA-friendly):
 *          - Moi ham chi co mot lenh return
 *          - Moi tham so deu duoc kiem tra truoc khi dung
 *          - Moi chuoi if deu ket thuc bang else
 *          - Loi duoc ghi vao trang thai module.lastError va tang errorCounter
 */

#include "isotp.h"

/*----------------------------------------------------------------------------
 * Trang thai module nam trong mot struct duy nhat
 *--------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Hang so noi bo - khong thuoc ban giao uoc cong khai
 *--------------------------------------------------------------------------*/

/** Mat na tach loai khung ra khoi byte dau. */
#define ISOTP_PCI_TYPE_MASK           (0xF0u)

/** Mat na tach gia tri di kem loai khung. */
#define ISOTP_PCI_VALUE_MASK          (0x0Fu)

/** Ten goi khac cho de doc o cho ghi loai khung noi tiep. */
#define ISOTP_PCI_CONSECUTIVE_FRAME   (ISOTP_PCI_CONSECUTIVE)

/** So byte du lieu cua khung don. */
#define ISOTP_SF_MAX_PAYLOAD          (ISOTP_SINGLE_FRAME_MAX)

/** So byte du lieu cua khung noi tiep. */
#define ISOTP_CF_MAX_PAYLOAD          (7u)

/*
 * Do dai toi thieu cua tung loai khung.
 * Nguoi goi bao dlc la so byte hop le, nen module chi duoc doc trong pham vi
 * do. Neu khong kiem tra, mot khung khai dlc bang mot van co the bi dua vao
 * ham xu ly khung dau - ham do doc toi byte thu tam.
 */

/** Khung don: mot byte dieu khien va it nhat mot byte du lieu. */
#define ISOTP_MIN_DLC_SINGLE_FRAME    (2u)

/** Khung dau: hai byte dieu khien va sau byte du lieu. */
#define ISOTP_MIN_DLC_FIRST_FRAME     (8u)

/** Khung noi tiep: mot byte dieu khien va it nhat mot byte du lieu. */
#define ISOTP_MIN_DLC_CONSECUTIVE     (2u)

/** Khung dieu khien luong: lenh, kich thuoc khoi, thoi gian nghi. */
#define ISOTP_MIN_DLC_FLOW_CONTROL    (3u)


/*----------------------------------------------------------------------------
 * Trang thai noi bo - co y de ngoai header cong khai, nen nguoi dung khong
 * the phu thuoc vao no, va co the sua doi ma khong lam hong ai.
 *--------------------------------------------------------------------------*/

/** Chieu gui dang o dau. */
typedef enum
{
    ISOTP_TX_IDLE       = 0u,   /**< Ranh, san sang nhan ban tin moi        */
    ISOTP_TX_WAIT_FC    = 1u,   /**< Da gui khung dau, dang cho xin phep */
    ISOTP_TX_SENDING_CF = 2u    /**< Da duoc phep, dang gui phan con lai  */
} IsoTp_TxStateType;

/** Chieu nhan dang o dau. */
typedef enum
{
    ISOTP_RX_IDLE         = 0u, /**< Ranh, khong ghep ban tin nao        */
    ISOTP_RX_RECEIVING_CF = 1u  /**< Dang ghep, cho khung tiep theo */
} IsoTp_RxStateType;

/** Nhung gi chieu gui can nho giua cac khung. */
typedef struct
{
    IsoTp_TxStateType state;
    uint8_t           buffer[ISOTP_MAX_MESSAGE_SIZE];
    uint16_t          totalLength;     /**< Tong so byte can gui   */
    uint16_t          sentIndex;       /**< Da gui duoc bao nhieu  */
    uint8_t           sequenceNumber;  /**< So thu tu khung ke tiep mang */
    uint32_t          timerStartMs;    /**< Bat dau cho tu luc nao        */
} IsoTp_TxContextType;

/** Nhung gi chieu nhan can nho giua cac khung. */
typedef struct
{
    IsoTp_RxStateType state;
    uint8_t           buffer[ISOTP_MAX_MESSAGE_SIZE];
    uint16_t          totalLength;     /**< Tong so byte mong doi */
    uint16_t          receivedIndex;   /**< Da nhan duoc bao nhieu   */
    uint8_t           sequenceNumber;  /**< So thu tu khung ke tiep phai la */
    uint32_t          timerStartMs;    /**< Khung gan nhat den luc nao   */
} IsoTp_RxContextType;

/** Toan bo trang thai cua module. */
typedef struct
{
    IsoTp_TxContextType  tx;
    IsoTp_RxContextType  rx;
    IsoTp_CanSendType    canSend;        /**< Cung cap luc khoi tao */
    IsoTp_RxCallbackType rxCallback;     /**< Cung cap luc khoi tao */
    IsoTp_ErrorCodeType  lastError;
    uint32_t             errorCounter;
    uint8_t              isInitialised;
} IsoTp_ContextType;


/** The hien duy nhat cua trang thai module. */
static IsoTp_ContextType isoTpContext;

/* Thoi diem cua lan goi hien tai (set boi Send/OnCanFrame/MainFunction).
   Cac ham xu ly noi bo doc gia tri nay khi dat han cho, nen khong phu thuoc
   vao thu tu goi cac ham cong khai. */
static uint32_t isoTpCurrentTimeMs;

/*----------------------------------------------------------------------------
 * Ham phu tro: ghi nhan mot loi
 *--------------------------------------------------------------------------*/

/**
 * @brief  Ghi nhan mot loi de doc lai ve sau.
 * @param  errorCode  Ma loi can ghi.
 */
static void IsoTp_RecordError(IsoTp_ErrorCodeType errorCode)
{
    isoTpContext.lastError = errorCode;
    isoTpContext.errorCounter++;
}

/*----------------------------------------------------------------------------
 * Ham phu tro: sao chep byte co kiem tra day du
 *--------------------------------------------------------------------------*/

/**
 * @brief  Sao chep mot so byte tu nguon sang dich, co kiem tra loi.
 * @param  destination     Noi chep den, khong duoc rong.
 * @param  source          Noi lay du lieu, khong duoc rong.
 * @param  length          So byte can chep.
 * @param  destCapacity    Suc chua toi da cua buffer dich.
 * @return ISOTP_OK neu thanh cong, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_CopyBytes(uint8_t       *destination,
                                        const uint8_t *source,
                                        uint16_t       length,
                                        uint16_t       destCapacity)
{
    IsoTp_StatusType status;
    uint16_t         index;

    /* Kiem tra con tro NULL */
    if ((destination == 0) || (source == 0))
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
    /* Kiem tra do dai vuot suc chua buffer dich */
    else if (length > destCapacity)
    {
        IsoTp_RecordError(ISOTP_ERR_LENGTH_EXCEEDED);
        status = ISOTP_ERROR_SIZE;
    }
    else
    {
        for (index = 0u; index < length; index++)
        {
            destination[index] = source[index];
        }
        status = ISOTP_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui mot khung don (payload <= 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Single Frame.
 * @param  message  Du lieu, ham goi da kiem tra.
 * @param  length   So byte, da kiem tra khong vuot gioi han khung don.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_SendSingleFrame(const uint8_t *message,
                                              uint16_t       length)
{
    IsoTp_StatusType status;
    uint8_t          frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;

    if (message == 0)
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
    else if (length > ISOTP_SF_MAX_PAYLOAD)
    {
        IsoTp_RecordError(ISOTP_ERR_SF_BAD_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    else
    {
        /* PCI: loai Single Frame + do dai (4 bit thap) */
        frame[0] = (uint8_t)(ISOTP_PCI_SINGLE_FRAME |
                             (length & ISOTP_PCI_VALUE_MASK));

        for (index = 0u; index < ISOTP_SF_MAX_PAYLOAD; index++)
        {
            if (index < (uint8_t)length)
            {
                frame[index + 1u] = message[index];
            }
            else
            {
                frame[index + 1u] = 0x00u;  /* Padding */
            }
        }

        if (isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE) != 0u)
        {
            IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
            status = ISOTP_ERROR_STATE;
        }
        else
        {
            status = ISOTP_OK;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui khung dau (bat dau ban tin > 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot First Frame va luu 6 byte dau vao context.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_SendFirstFrame(void)
{
    IsoTp_StatusType status;
    uint8_t          frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;

    /* PCI 12 bit do dai: 4 bit cao o byte 0, 8 bit thap o byte 1 */
    frame[0] = (uint8_t)(ISOTP_PCI_FIRST_FRAME |
                         ((isoTpContext.tx.totalLength >> 8u) & ISOTP_PCI_VALUE_MASK));
    frame[1] = (uint8_t)(isoTpContext.tx.totalLength & 0xFFu);

    for (index = 0u; index < ISOTP_FF_FIRST_PAYLOAD; index++)
    {
        frame[index + 2u] = isoTpContext.tx.buffer[index];
    }

    /*
     * Cung ly do nhu khung noi tiep: chi ghi nhan da gui sau khi tang duoi
     * nhan khung. Khi that bai con phai dua chieu gui ve nghi, neu khong
     * module se ket o trang thai cho xin phep cho mot khung chua ra bus.
     */
    if (isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE) != 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
        isoTpContext.tx.sentIndex = 0u;
        isoTpContext.tx.state     = ISOTP_TX_IDLE;
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        isoTpContext.tx.sentIndex = ISOTP_FF_FIRST_PAYLOAD;
        status = ISOTP_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui mot Consecutive Frame
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Consecutive Frame tiep theo tu context.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_SendConsecutiveFrame(void)
{
    IsoTp_StatusType status;
    uint8_t          frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;
    uint16_t         remaining;
    uint16_t         bytesToSend;

    frame[0] = (uint8_t)(ISOTP_PCI_CONSECUTIVE_FRAME |
                         (isoTpContext.tx.sequenceNumber & ISOTP_PCI_VALUE_MASK));

    remaining = isoTpContext.tx.totalLength - isoTpContext.tx.sentIndex;

    if (remaining >= ISOTP_CF_MAX_PAYLOAD)
    {
        bytesToSend = ISOTP_CF_MAX_PAYLOAD;
    }
    else
    {
        bytesToSend = remaining;
    }

    for (index = 0u; index < ISOTP_CF_MAX_PAYLOAD; index++)
    {
        if (index < (uint8_t)bytesToSend)
        {
            frame[index + 1u] =
                isoTpContext.tx.buffer[isoTpContext.tx.sentIndex + index];
        }
        else
        {
            frame[index + 1u] = 0x00u;  /* Padding */
        }
    }

    /*
     * Chi cap nhat trang thai SAU khi tang duoi da nhan khung.
     * Neu cap nhat truoc roi gui that bai, module se tuong la da gui duoc
     * trong khi ben kia chua nhan gi - ban tin mat ma khong ai biet.
     */
    if (isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE) != 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        isoTpContext.tx.sentIndex += bytesToSend;

        /* So thu tu cuon vong tu khong den muoi lam */
        isoTpContext.tx.sequenceNumber =
            (uint8_t)((isoTpContext.tx.sequenceNumber + 1u)
                      & ISOTP_PCI_VALUE_MASK);

        status = ISOTP_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly khung don nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly khung don nhan duoc, giao payload len tang tren.
 * @param  frame  Con tro 8 byte frame (da kiem tra NULL o ham goi).
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedSingleFrame(const uint8_t *frame, uint8_t dlc)
{
    IsoTp_StatusType status;
    uint8_t          payloadLength;

    payloadLength = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    if (dlc < ISOTP_MIN_DLC_SINGLE_FRAME)
    {
        /* Khung qua ngan de chua noi mot byte du lieu */
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    /* Kiem tra do dai hop le: 1..7 */
    else if ((payloadLength == 0u) || (payloadLength > ISOTP_SF_MAX_PAYLOAD))
    {
        IsoTp_RecordError(ISOTP_ERR_SF_BAD_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    else if ((uint16_t)(payloadLength + 1u) > (uint16_t)dlc)
    {
        /* Khung khai nhieu byte du lieu hon so byte hop le no mang theo */
        IsoTp_RecordError(ISOTP_ERR_SF_BAD_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    else
    {
        status = IsoTp_CopyBytes(isoTpContext.rx.buffer,
                                 &frame[1],
                                 (uint16_t)payloadLength,
                                 ISOTP_MAX_MESSAGE_SIZE);

        if (status == ISOTP_OK)
        {
            isoTpContext.rxCallback(isoTpContext.rx.buffer,
                                    (uint16_t)payloadLength);
        }
        else
        {
            /* Ham chep da ghi nhan ly do */
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly khung dau nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly khung dau nhan duoc: luu phan du lieu dau, roi cho phep gui tiep.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
/**
 * @brief  Gui khung dieu khien luong cho phep gui tiep.
 * @details Tach rieng de code gon va de mo rong Block Size / STmin sau nay.
 *          Kich thuoc khoi va thoi gian nghi deu bang khong, nen ben kia duoc gui
 *          het khong can dung. Phu hop khi bus con rong.
 * @return ISOTP_OK khi khung da ra ngoai, ISOTP_ERROR_STATE khi khong gui duoc.
 */
static IsoTp_StatusType IsoTp_SendFlowControl(void)
{
    IsoTp_StatusType status;
    uint8_t          flowControlFrame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;
    uint8_t          sendResult;

    flowControlFrame[0] =
        (uint8_t)(ISOTP_PCI_FLOW_CONTROL | ISOTP_FC_CONTINUE_TO_SEND);
    flowControlFrame[1] = 0x00u;  /* Kich thuoc khoi bang khong: khong can nghi         */
    flowControlFrame[2] = 0x00u;  /* Thoi gian nghi bang khong */
    for (index = 3u; index < ISOTP_CAN_FRAME_SIZE; index++)
    {
        flowControlFrame[index] = 0x00u;
    }

    sendResult = isoTpContext.canSend(flowControlFrame, ISOTP_CAN_FRAME_SIZE);

    if (sendResult != 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        status = ISOTP_OK;
    }

    return status;
}

static IsoTp_StatusType IsoTp_HandleReceivedFirstFrame(const uint8_t *frame, uint8_t dlc)
{
    IsoTp_StatusType status;

    if (dlc < ISOTP_MIN_DLC_FIRST_FRAME)
    {
        /* Khung dau phai du tam byte: hai byte dieu khien va sau byte du lieu */
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    else
    {
    /* Ghep 12 bit do dai tu byte 0 va byte 1 */
    isoTpContext.rx.totalLength =
        (uint16_t)(((uint16_t)(frame[0] & ISOTP_PCI_VALUE_MASK) << 8u) |
                   (uint16_t)frame[1]);

    /*
     * Do dai phai lon hon suc chua cua mot khung don. Ban tin bay byte tro
     * xuong bat buoc phai gui bang khung don, nen khung dau khai do dai nho
     * hon la sai chuan. Khong chan thi module se chep sau byte roi thay da
     * du, goi len tang tren voi ban tin sai.
     */
    if ((isoTpContext.rx.totalLength <= ISOTP_SINGLE_FRAME_MAX) ||
        (isoTpContext.rx.totalLength > ISOTP_MAX_MESSAGE_SIZE))
    {
        IsoTp_RecordError(ISOTP_ERR_LENGTH_EXCEEDED);
        isoTpContext.rx.state = ISOTP_RX_IDLE;
        status = ISOTP_ERROR_SIZE;
    }
    else
    {
        status = IsoTp_CopyBytes(isoTpContext.rx.buffer,
                                 &frame[2],
                                 ISOTP_FF_FIRST_PAYLOAD,
                                 ISOTP_MAX_MESSAGE_SIZE);

        if (status == ISOTP_OK)
        {
            isoTpContext.rx.receivedIndex  = ISOTP_FF_FIRST_PAYLOAD;
            isoTpContext.rx.sequenceNumber = 1u;
            isoTpContext.rx.state          = ISOTP_RX_RECEIVING_CF;

            /* Bat dau dem timeout N_Cr cho Consecutive Frame */
            isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;

            /* Cho phep gui phan con lai */
            status = IsoTp_SendFlowControl();
        }
        else
        {
            isoTpContext.rx.state = ISOTP_RX_IDLE;
        }
    }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly khung noi tiep nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly Consecutive Frame: kiem tra SN, ghep du lieu.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedConsecutiveFrame(const uint8_t *frame, uint8_t dlc)
{
    IsoTp_StatusType status;
    uint8_t          receivedSequenceNumber;
    uint16_t         remaining;
    uint16_t         bytesToCopy;

    if (dlc < ISOTP_MIN_DLC_CONSECUTIVE)
    {
        /* Khung qua ngan de chua noi mot byte du lieu */
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    /* Chi co y nghia khi dang ghep mot ban tin */
    else if (isoTpContext.rx.state != ISOTP_RX_RECEIVING_CF)
    {
        IsoTp_RecordError(ISOTP_ERR_UNEXPECTED_CF);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        receivedSequenceNumber = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

        /* Kiem tra SN dung thu tu */
        if (receivedSequenceNumber != isoTpContext.rx.sequenceNumber)
        {
            IsoTp_RecordError(ISOTP_ERR_SEQUENCE_MISMATCH);
            isoTpContext.rx.state = ISOTP_RX_IDLE;
            status = ISOTP_ERROR_SEQUENCE;
        }
        else
        {
            remaining =
                isoTpContext.rx.totalLength - isoTpContext.rx.receivedIndex;

            if (remaining >= ISOTP_CF_MAX_PAYLOAD)
            {
                bytesToCopy = ISOTP_CF_MAX_PAYLOAD;
            }
            else
            {
                bytesToCopy = remaining;
            }

            status = IsoTp_CopyBytes(
                         &isoTpContext.rx.buffer[isoTpContext.rx.receivedIndex],
                         &frame[1],
                         bytesToCopy,
                         (uint16_t)(ISOTP_MAX_MESSAGE_SIZE -
                                    isoTpContext.rx.receivedIndex));

            if (status == ISOTP_OK)
            {
                isoTpContext.rx.receivedIndex += bytesToCopy;
                isoTpContext.rx.sequenceNumber =
                    (uint8_t)((isoTpContext.rx.sequenceNumber + 1u) &
                              ISOTP_PCI_VALUE_MASK);

                /* Reset dong ho N_Cr sau moi CF hop le */
                isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;

                /* Da nhan du ca ban tin */
                if (isoTpContext.rx.receivedIndex >= isoTpContext.rx.totalLength)
                {
                    isoTpContext.rx.state = ISOTP_RX_IDLE;
                    isoTpContext.rxCallback(isoTpContext.rx.buffer,
                                            isoTpContext.rx.totalLength);
                }
                else
                {
                    /* Con byte, tiep tuc cho CF sau */
                }
            }
            else
            {
                isoTpContext.rx.state = ISOTP_RX_IDLE;
            }
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly khung dieu khien luong nhan duoc (phia truyen)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly khung dieu khien luong nhan duoc va phan ung theo lenh trong do.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedFlowControl(const uint8_t *frame, uint8_t dlc)
{
    IsoTp_StatusType status;
    uint8_t          flowStatus;

    if (dlc < ISOTP_MIN_DLC_FLOW_CONTROL)
    {
        /* Phai du ba byte: lenh, kich thuoc khoi, thoi gian nghi */
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    /* Chi xu ly khi dang cho Flow Control */
    else if (isoTpContext.tx.state != ISOTP_TX_WAIT_FC)
    {
        IsoTp_RecordError(ISOTP_ERR_UNEXPECTED_CF);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        flowStatus = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

        if (flowStatus == ISOTP_FC_CONTINUE_TO_SEND)
        {
            isoTpContext.tx.state = ISOTP_TX_SENDING_CF;
            status = ISOTP_OK;
        }
        else if (flowStatus == ISOTP_FC_WAIT)
        {
            /* Ben kia xin cho, nen giu nguyen trang thai */
            status = ISOTP_BUSY;
        }
        else if (flowStatus == ISOTP_FC_OVERFLOW)
        {
            IsoTp_RecordError(ISOTP_ERR_FC_OVERFLOW);
            isoTpContext.tx.state = ISOTP_TX_IDLE;
            status = ISOTP_ERROR_SIZE;
        }
        else
        {
            IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
            isoTpContext.tx.state = ISOTP_TX_IDLE;
            status = ISOTP_ERROR_FRAME;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao tang ISO-TP. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                            IsoTp_RxCallbackType rxCallback)
{
    IsoTp_StatusType status;

    /* Kiem tra tham so dau vao */
    if ((canSendFunction == 0) || (rxCallback == 0))
    {
        status = ISOTP_ERROR_NULL;
    }
    else
    {
        isoTpContext.canSend    = canSendFunction;
        isoTpContext.rxCallback = rxCallback;

        isoTpContext.tx.state          = ISOTP_TX_IDLE;
        isoTpContext.tx.totalLength    = 0u;
        isoTpContext.tx.sentIndex      = 0u;
        isoTpContext.tx.sequenceNumber = 0u;
        isoTpContext.tx.timerStartMs   = 0u;

        isoTpContext.rx.state          = ISOTP_RX_IDLE;
        isoTpContext.rx.totalLength    = 0u;
        isoTpContext.rx.receivedIndex  = 0u;
        isoTpContext.rx.sequenceNumber = 0u;
        isoTpContext.rx.timerStartMs   = 0u;

        isoTpContext.lastError       = ISOTP_ERR_NONE;
        isoTpContext.errorCounter    = 0u;
        isoTpCurrentTimeMs = 0u;
        isoTpContext.isInitialised   = 1u;

        status = ISOTP_OK;
    }

    return status;
}

/**
 * @brief  Gui mot ban tin qua ISO-TP. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message,
                            uint16_t       length,
                            uint32_t       currentTimeMs)
{
    IsoTp_StatusType status;

    /* Ghi lai thoi diem de dat han cho */
    isoTpCurrentTimeMs = currentTimeMs;

    /* Kiem tra da khoi tao chua */
    if (isoTpContext.isInitialised == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_NOT_INITIALISED);
        status = ISOTP_ERROR_STATE;
    }
    /* Kiem tra con tro NULL */
    else if (message == 0)
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
    /* Kiem tra do dai bang 0 */
    else if (length == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_ZERO_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    /* Kiem tra do dai vuot gioi han */
    else if (length > ISOTP_MAX_MESSAGE_SIZE)
    {
        IsoTp_RecordError(ISOTP_ERR_LENGTH_EXCEEDED);
        status = ISOTP_ERROR_SIZE;
    }
    /* Tu choi ban tin moi khi ban tin cu con dang gui */
    else if (isoTpContext.tx.state != ISOTP_TX_IDLE)
    {
        status = ISOTP_BUSY;
    }
    /* Truong hop ngan: Single Frame */
    else if (length <= ISOTP_SF_MAX_PAYLOAD)
    {
        status = IsoTp_SendSingleFrame(message, length);
    }
    /* Truong hop dai: bat dau multi-frame */
    else
    {
        status = IsoTp_CopyBytes(isoTpContext.tx.buffer,
                                 message,
                                 length,
                                 ISOTP_MAX_MESSAGE_SIZE);

        if (status == ISOTP_OK)
        {
            isoTpContext.tx.totalLength    = length;
            isoTpContext.tx.sequenceNumber = 1u;
            isoTpContext.tx.state          = ISOTP_TX_WAIT_FC;

            /* Bat dau dem timeout N_Bs cho Flow Control */
            isoTpContext.tx.timerStartMs = isoTpCurrentTimeMs;

            status = IsoTp_SendFirstFrame();
        }
        else
        {
            /* Ham chep da ghi nhan ly do */
        }
    }

    return status;
}

/**
 * @brief  Xu ly mot khung CAN nhan duoc. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_OnCanFrame(const uint8_t *frame,
                                  uint8_t        dlc,
                                  uint32_t       currentTimeMs)
{
    IsoTp_StatusType status;
    uint8_t          frameType;

    /* Ghi lai thoi diem de cac ham xu ly dat han cho */
    isoTpCurrentTimeMs = currentTimeMs;

    /* Kiem tra da khoi tao chua */
    if (isoTpContext.isInitialised == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_NOT_INITIALISED);
        status = ISOTP_ERROR_STATE;
    }
    /* Kiem tra con tro NULL */
    else if (frame == 0)
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
    /* Kiem tra dlc hop le */
    else if ((dlc == 0u) || (dlc > ISOTP_CAN_FRAME_SIZE))
    {
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    else
    {
        frameType = (uint8_t)(frame[0] & ISOTP_PCI_TYPE_MASK);

        if (frameType == ISOTP_PCI_SINGLE_FRAME)
        {
            status = IsoTp_HandleReceivedSingleFrame(frame, dlc);
        }
        else if (frameType == ISOTP_PCI_FIRST_FRAME)
        {
            status = IsoTp_HandleReceivedFirstFrame(frame, dlc);
        }
        else if (frameType == ISOTP_PCI_CONSECUTIVE_FRAME)
        {
            status = IsoTp_HandleReceivedConsecutiveFrame(frame, dlc);
        }
        else if (frameType == ISOTP_PCI_FLOW_CONTROL)
        {
            status = IsoTp_HandleReceivedFlowControl(frame, dlc);
        }
        else
        {
            /* Bon bit loai khong khop loai khung nao trong bon loai */
            IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
            status = ISOTP_ERROR_FRAME;
        }
    }

    return status;
}

/**
 * @brief  Ham chu ky. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_MainFunction(uint32_t currentTimeMs)
{
    IsoTp_StatusType status;

    if (isoTpContext.isInitialised == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_NOT_INITIALISED);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        status = ISOTP_OK;

        /* Ghi lai thoi diem cho cac ham xu ly */
        isoTpCurrentTimeMs = currentTimeMs;

        /* Gui khung noi tiep ke tiep khi den luot */
        if (isoTpContext.tx.state == ISOTP_TX_SENDING_CF)
        {
            if (isoTpContext.tx.sentIndex < isoTpContext.tx.totalLength)
            {
                status = IsoTp_SendConsecutiveFrame();
            }
            else
            {
                /* Khong con byte nao de gui */
            }

            if (isoTpContext.tx.sentIndex >= isoTpContext.tx.totalLength)
            {
                isoTpContext.tx.state = ISOTP_TX_IDLE;
            }
            else
            {
                /* Con byte, cho lan MainFunction sau */
            }
        }
        /* Kiem tra timeout cho Flow Control (N_Bs) */
        else if (isoTpContext.tx.state == ISOTP_TX_WAIT_FC)
        {
            if ((currentTimeMs - isoTpContext.tx.timerStartMs) >
                ISOTP_TIMEOUT_N_BS_MS)
            {
                IsoTp_RecordError(ISOTP_ERR_TX_TIMEOUT);
                isoTpContext.tx.state = ISOTP_TX_IDLE;
                status = ISOTP_ERROR_STATE;
            }
            else
            {
                /* Chua het gio */
            }
        }
        else
        {
            /* Chieu gui dang nghi */
        }

        /* Kiem tra timeout cho Consecutive Frame (N_Cr) */
        if (isoTpContext.rx.state == ISOTP_RX_RECEIVING_CF)
        {
            if ((currentTimeMs - isoTpContext.rx.timerStartMs) >
                ISOTP_TIMEOUT_N_CR_MS)
            {
                IsoTp_RecordError(ISOTP_ERR_RX_TIMEOUT);
                isoTpContext.rx.state = ISOTP_RX_IDLE;
                status = ISOTP_ERROR_STATE;
            }
            else
            {
                /* Chua het gio */
            }
        }
        else
        {
            /* Chieu nhan dang nghi */
        }
    }

    return status;
}

/**
 * @brief  Lay ma loi gan nhat. Xem mo ta trong isotp.h.
 */
IsoTp_ErrorCodeType IsoTp_GetLastError(void)
{
    return isoTpContext.lastError;
}

/**
 * @brief  Lay tong so lan gap loi. Xem mo ta trong isotp.h.
 */
uint32_t IsoTp_GetErrorCounter(void)
{
    return isoTpContext.errorCounter;
}
