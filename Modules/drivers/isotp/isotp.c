/**
<<<<<<< HEAD
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
=======
 * @file isotp.c
 * @brief Minimal passive ISO-TP segmentation and reassembly.
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
 */

#include "isotp.h"

<<<<<<< HEAD
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
=======
#define ISOTP_PCI_TYPE_MASK       (0xF0U)
#define ISOTP_PCI_VALUE_MASK      (0x0FU)
#define ISOTP_PCI_SINGLE          (0x00U)
#define ISOTP_PCI_FIRST           (0x10U)
#define ISOTP_PCI_CONSECUTIVE     (0x20U)
#define ISOTP_PCI_FLOW_CONTROL    (0x30U)
#define ISOTP_FLOW_STATUS_CTS     (0x00U)

/**
 * @brief Copies a validated byte range.
 * @param destination Destination address.
 * @param source Source address.
 * @param length Number of bytes not exceeding the ISO-TP buffer.
 * @return ISOTP_E_OK or an input error.
 */
static IsoTp_ReturnType IsoTp_CopyBytes(uint8_t *destination,
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
                                        const uint8_t *source,
                                        uint16_t length)
{
    IsoTp_ReturnType result;
    uint16_t index;

    if ((destination == 0) || (source == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (length > ISOTP_MAX_PAYLOAD_LENGTH)
    {
        result = ISOTP_E_INVALID_LENGTH;
    }
    else
    {
        for (index = 0U; index < length; index++)
        {
            destination[index] = source[index];
        }
        result = ISOTP_E_OK;
    }

    return result;
}

<<<<<<< HEAD
/*----------------------------------------------------------------------------
 * Gui mot khung don (payload <= 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Single Frame.
 * @param  message  Du lieu, ham goi da kiem tra.
 * @param  length   So byte, da kiem tra khong vuot gioi han khung don.
 * @return ISOTP_OK, hoac ma loi.
=======
/**
 * @brief Initializes a padded Classical CAN frame.
 * @param frame Destination frame.
 * @param identifier Standard CAN identifier.
 * @return ISOTP_E_OK or an input error.
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
 */
static IsoTp_ReturnType IsoTp_PrepareFrame(IsoTp_CanFrameType *frame,
                                           uint16_t identifier)
{
    IsoTp_ReturnType result;
    uint8_t index;

    if (frame == 0)
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (identifier > ISOTP_MAX_STANDARD_CAN_ID)
    {
        result = ISOTP_E_INVALID_CAN_ID;
    }
    else
    {
        frame->identifier = identifier;
        frame->dataLength = ISOTP_CAN_FRAME_LENGTH;
        for (index = 0U; index < ISOTP_CAN_FRAME_LENGTH; index++)
        {
            frame->data[index] = ISOTP_PADDING_VALUE;
        }
        result = ISOTP_E_OK;
    }

    return result;
}

/**
 * @brief Clears only segmentation state.
 * @param context Module context or NULL.
 */
static void IsoTp_ClearTransmit(IsoTp_ContextType *context)
{
    uint16_t index;

    if (context != 0)
    {
        for (index = 0U; index < ISOTP_MAX_PAYLOAD_LENGTH; index++)
        {
            context->transmit.buffer[index] = 0U;
        }
        context->transmit.totalLength = 0U;
        context->transmit.transmittedLength = 0U;
        context->transmit.nextSequenceNumber = 1U;
        context->transmit.separationTimeMs = 0U;
        context->transmit.state = ISOTP_TX_STATE_IDLE;
    }
    else
    {
        /* A NULL internal context cannot be cleared. */
    }
}

/**
 * @brief Clears only reassembly state.
 * @param context Module context or NULL.
 */
static void IsoTp_ClearReceive(IsoTp_ContextType *context)
{
    uint16_t index;

    if (context != 0)
    {
        for (index = 0U; index < ISOTP_MAX_PAYLOAD_LENGTH; index++)
        {
            context->receive.buffer[index] = 0U;
        }
        context->receive.totalLength = 0U;
        context->receive.receivedLength = 0U;
        context->receive.expectedSequenceNumber = 1U;
        context->receive.state = ISOTP_RX_STATE_IDLE;
    }
    else
    {
        /* A NULL internal context cannot be cleared. */
    }
}

/** @brief Initializes one passive ISO-TP connection. */
IsoTp_ReturnType IsoTp_Init(IsoTp_ContextType *context,
                            const IsoTp_ConfigType *config)
{
    IsoTp_ReturnType result;

    if ((context == 0) || (config == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if ((config->transmitCanIdentifier >
              ISOTP_MAX_STANDARD_CAN_ID) ||
             (config->receiveCanIdentifier >
              ISOTP_MAX_STANDARD_CAN_ID) ||
             (config->transmitCanIdentifier ==
              config->receiveCanIdentifier))
    {
        result = ISOTP_E_INVALID_CONFIG;
    }
    else
    {
        context->config = *config;
        context->initialized = 1U;
        IsoTp_ClearTransmit(context);
        IsoTp_ClearReceive(context);
        result = ISOTP_E_OK;
    }

    return result;
}

/** @brief Starts segmentation without transmitting a CAN frame. */
IsoTp_ReturnType IsoTp_StartSegmentation(IsoTp_ContextType *context,
                                         const uint8_t *payload,
                                         uint16_t payloadLength)
{
    IsoTp_ReturnType result;

    if ((context == 0) || (payload == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else if ((payloadLength == 0U) ||
             (payloadLength > ISOTP_MAX_PAYLOAD_LENGTH))
    {
        result = ISOTP_E_INVALID_LENGTH;
    }
    else if (context->transmit.state != ISOTP_TX_STATE_IDLE)
    {
        result = ISOTP_E_BUSY;
    }
    else
    {
        result = IsoTp_CopyBytes(context->transmit.buffer, payload,
                                 payloadLength);
        if (result == ISOTP_E_OK)
        {
            context->transmit.totalLength = payloadLength;
            context->transmit.transmittedLength = 0U;
            context->transmit.nextSequenceNumber = 1U;
            context->transmit.separationTimeMs = 0U;
            if (payloadLength <= ISOTP_SINGLE_FRAME_PAYLOAD)
            {
                context->transmit.state =
                    ISOTP_TX_STATE_SINGLE_FRAME_READY;
            }
            else
            {
                context->transmit.state =
                    ISOTP_TX_STATE_FIRST_FRAME_READY;
            }
        }
        else
        {
            /* The validated copy error is returned unchanged. */
        }
    }

    return result;
}

<<<<<<< HEAD
/*----------------------------------------------------------------------------
 * Gui khung dau (bat dau ban tin > 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot First Frame va luu 6 byte dau vao context.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_SendFirstFrame(void)
=======
/** @brief Builds and consumes the next pending data frame. */
IsoTp_ReturnType IsoTp_GetNextFrame(IsoTp_ContextType *context,
                                    IsoTp_CanFrameType *frame)
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
{
    IsoTp_ReturnType result;
    uint16_t remainingLength;
    uint16_t payloadLength;

    if ((context == 0) || (frame == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
<<<<<<< HEAD

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
=======
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else if ((context->transmit.state !=
              ISOTP_TX_STATE_SINGLE_FRAME_READY) &&
             (context->transmit.state !=
              ISOTP_TX_STATE_FIRST_FRAME_READY) &&
             (context->transmit.state !=
              ISOTP_TX_STATE_CONSECUTIVE_FRAME_READY))
    {
        result = ISOTP_E_NO_FRAME_AVAILABLE;
    }
    else
    {
        result = IsoTp_PrepareFrame(
            frame, context->config.transmitCanIdentifier);
        if ((result == ISOTP_E_OK) &&
            (context->transmit.state ==
             ISOTP_TX_STATE_SINGLE_FRAME_READY))
        {
            frame->data[0] = (uint8_t)(context->transmit.totalLength &
                                        ISOTP_PCI_VALUE_MASK);
            result = IsoTp_CopyBytes(&frame->data[1],
                                     context->transmit.buffer,
                                     context->transmit.totalLength);
            if (result == ISOTP_E_OK)
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
            {
                IsoTp_ClearTransmit(context);
            }
            else
            {
                /* The copy error is returned unchanged. */
            }
        }
        else if ((result == ISOTP_E_OK) &&
                 (context->transmit.state ==
                  ISOTP_TX_STATE_FIRST_FRAME_READY))
        {
            frame->data[0] = (uint8_t)(ISOTP_PCI_FIRST |
                ((context->transmit.totalLength >> 8U) &
                 ISOTP_PCI_VALUE_MASK));
            frame->data[1] = (uint8_t)(context->transmit.totalLength &
                                       0xFFU);
            result = IsoTp_CopyBytes(&frame->data[2],
                                     context->transmit.buffer,
                                     ISOTP_FIRST_FRAME_PAYLOAD);
            if (result == ISOTP_E_OK)
            {
                context->transmit.transmittedLength =
                    ISOTP_FIRST_FRAME_PAYLOAD;
                context->transmit.state =
                    ISOTP_TX_STATE_WAIT_FLOW_CONTROL;
            }
            else
            {
                /* The copy error is returned unchanged. */
            }
        }
        else if (result == ISOTP_E_OK)
        {
            remainingLength = (uint16_t)(
                context->transmit.totalLength -
                context->transmit.transmittedLength);
            if (remainingLength > ISOTP_CONSECUTIVE_FRAME_PAYLOAD)
            {
                payloadLength = ISOTP_CONSECUTIVE_FRAME_PAYLOAD;
            }
            else
            {
                payloadLength = remainingLength;
            }

            frame->data[0] = (uint8_t)(ISOTP_PCI_CONSECUTIVE |
                (context->transmit.nextSequenceNumber &
                 ISOTP_PCI_VALUE_MASK));
            result = IsoTp_CopyBytes(
                &frame->data[1],
                &context->transmit.buffer[
                    context->transmit.transmittedLength],
                payloadLength);
            if (result == ISOTP_E_OK)
            {
<<<<<<< HEAD
                isoTpContext.rx.receivedIndex += bytesToCopy;
                isoTpContext.rx.sequenceNumber =
                    (uint8_t)((isoTpContext.rx.sequenceNumber + 1u) &
                              ISOTP_PCI_VALUE_MASK);

                /* Reset dong ho N_Cr sau moi CF hop le */
                isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;

                /* Da nhan du ca ban tin */
                if (isoTpContext.rx.receivedIndex >= isoTpContext.rx.totalLength)
=======
                context->transmit.transmittedLength = (uint16_t)(
                    context->transmit.transmittedLength + payloadLength);
                context->transmit.nextSequenceNumber = (uint8_t)(
                    (context->transmit.nextSequenceNumber + 1U) &
                    ISOTP_PCI_VALUE_MASK);
                if (context->transmit.transmittedLength >=
                    context->transmit.totalLength)
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
                {
                    IsoTp_ClearTransmit(context);
                }
                else
                {
                    context->transmit.state =
                        ISOTP_TX_STATE_CONSECUTIVE_FRAME_READY;
                }
            }
            else
            {
                /* The copy error is returned unchanged. */
            }
        }
<<<<<<< HEAD
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
=======
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
        else
        {
            /* Frame preparation error is returned unchanged. */
        }
    }

    return result;
}

/** @brief Parses one received transport frame without scheduling work. */
IsoTp_ReturnType IsoTp_ProcessFrame(IsoTp_ContextType *context,
                                    const IsoTp_CanFrameType *frame,
                                    IsoTp_RxEventType *event,
                                    IsoTp_CanFrameType *flowControlFrame)
{
    IsoTp_ReturnType result;
    uint8_t frameType;
    uint8_t payloadLength8;
    uint8_t sequenceNumber;
    uint8_t flowStatus;
    uint16_t totalLength;
    uint16_t remainingLength;
    uint16_t payloadLength;

    if ((context == 0) || (frame == 0) || (event == 0) ||
        (flowControlFrame == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else if (frame->identifier != context->config.receiveCanIdentifier)
    {
        result = ISOTP_E_INVALID_CAN_ID;
    }
    else if ((frame->dataLength == 0U) ||
             (frame->dataLength > ISOTP_CAN_FRAME_LENGTH))
    {
        result = ISOTP_E_INVALID_DLC;
    }
    else
    {
        *event = ISOTP_RX_EVENT_NONE;
        frameType = (uint8_t)(frame->data[0] & ISOTP_PCI_TYPE_MASK);

<<<<<<< HEAD
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
=======
        if (frameType == ISOTP_PCI_SINGLE)
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
        {
            payloadLength8 = (uint8_t)(frame->data[0] &
                                       ISOTP_PCI_VALUE_MASK);
            if (context->receive.state != ISOTP_RX_STATE_IDLE)
            {
                result = ISOTP_E_UNEXPECTED_FRAME;
            }
            else if ((payloadLength8 == 0U) ||
                     (payloadLength8 > ISOTP_SINGLE_FRAME_PAYLOAD) ||
                     ((uint8_t)(payloadLength8 + 1U) >
                      frame->dataLength))
            {
                result = ISOTP_E_INVALID_FRAME_FORMAT;
            }
            else
            {
                result = IsoTp_CopyBytes(context->receive.buffer,
                                         &frame->data[1], payloadLength8);
                if (result == ISOTP_E_OK)
                {
                    context->receive.totalLength = payloadLength8;
                    context->receive.receivedLength = payloadLength8;
                    context->receive.state =
                        ISOTP_RX_STATE_PAYLOAD_READY;
                    *event = ISOTP_RX_EVENT_PAYLOAD_COMPLETE;
                }
                else
                {
                    /* The copy error is returned unchanged. */
                }
            }
        }
        else if (frameType == ISOTP_PCI_FIRST)
        {
<<<<<<< HEAD
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
=======
            totalLength = (uint16_t)(
                (((uint16_t)frame->data[0] & ISOTP_PCI_VALUE_MASK) << 8U) |
                (uint16_t)frame->data[1]);
            if (context->receive.state != ISOTP_RX_STATE_IDLE)
            {
                result = ISOTP_E_UNEXPECTED_FRAME;
            }
            else if (frame->dataLength != ISOTP_CAN_FRAME_LENGTH)
            {
                result = ISOTP_E_INVALID_DLC;
            }
            else if (totalLength <= ISOTP_SINGLE_FRAME_PAYLOAD)
            {
                result = ISOTP_E_INVALID_FRAME_FORMAT;
            }
            else if (totalLength > ISOTP_MAX_PAYLOAD_LENGTH)
            {
                result = ISOTP_E_BUFFER_OVERFLOW;
            }
            else
            {
                result = IsoTp_CopyBytes(context->receive.buffer,
                                         &frame->data[2],
                                         ISOTP_FIRST_FRAME_PAYLOAD);
                if (result == ISOTP_E_OK)
                {
                    result = IsoTp_PrepareFrame(
                        flowControlFrame,
                        context->config.transmitCanIdentifier);
                }
                else
                {
                    /* The copy error is returned unchanged. */
                }

                if (result == ISOTP_E_OK)
                {
                    context->receive.totalLength = totalLength;
                    context->receive.receivedLength =
                        ISOTP_FIRST_FRAME_PAYLOAD;
                    context->receive.expectedSequenceNumber = 1U;
                    context->receive.state =
                        ISOTP_RX_STATE_WAIT_CONSECUTIVE_FRAME;
                    flowControlFrame->data[0] = (uint8_t)(
                        ISOTP_PCI_FLOW_CONTROL | ISOTP_FLOW_STATUS_CTS);
                    flowControlFrame->data[1] =
                        ISOTP_BASIC_BLOCK_SIZE;
                    flowControlFrame->data[2] =
                        ISOTP_BASIC_ST_MIN_MS;
                    *event = ISOTP_RX_EVENT_FLOW_CONTROL_REQUIRED;
                }
                else
                {
                    IsoTp_ClearReceive(context);
                }
            }
        }
        else if (frameType == ISOTP_PCI_CONSECUTIVE)
        {
            sequenceNumber = (uint8_t)(frame->data[0] &
                                       ISOTP_PCI_VALUE_MASK);
            if (context->receive.state !=
                ISOTP_RX_STATE_WAIT_CONSECUTIVE_FRAME)
            {
                result = ISOTP_E_UNEXPECTED_FRAME;
            }
            else if (sequenceNumber !=
                     context->receive.expectedSequenceNumber)
            {
                IsoTp_ClearReceive(context);
                result = ISOTP_E_SEQUENCE_MISMATCH;
            }
            else
            {
                remainingLength = (uint16_t)(
                    context->receive.totalLength -
                    context->receive.receivedLength);
                if (remainingLength >
                    ISOTP_CONSECUTIVE_FRAME_PAYLOAD)
                {
                    payloadLength =
                        ISOTP_CONSECUTIVE_FRAME_PAYLOAD;
                }
                else
                {
                    payloadLength = remainingLength;
                }

                if ((uint16_t)frame->dataLength < (payloadLength + 1U))
                {
                    IsoTp_ClearReceive(context);
                    result = ISOTP_E_INVALID_DLC;
                }
                else
                {
                    result = IsoTp_CopyBytes(
                        &context->receive.buffer[
                            context->receive.receivedLength],
                        &frame->data[1], payloadLength);
                    if (result == ISOTP_E_OK)
                    {
                        context->receive.receivedLength = (uint16_t)(
                            context->receive.receivedLength +
                            payloadLength);
                        context->receive.expectedSequenceNumber = (uint8_t)(
                            (context->receive.expectedSequenceNumber + 1U) &
                            ISOTP_PCI_VALUE_MASK);
                        if (context->receive.receivedLength >=
                            context->receive.totalLength)
                        {
                            context->receive.state =
                                ISOTP_RX_STATE_PAYLOAD_READY;
                            *event = ISOTP_RX_EVENT_PAYLOAD_COMPLETE;
                        }
                        else
                        {
                            *event =
                                ISOTP_RX_EVENT_CONSECUTIVE_FRAME_RECEIVED;
                        }
                    }
                    else
                    {
                        IsoTp_ClearReceive(context);
                    }
                }
            }
        }
        else if (frameType == ISOTP_PCI_FLOW_CONTROL)
        {
            flowStatus = (uint8_t)(frame->data[0] &
                                   ISOTP_PCI_VALUE_MASK);
            if (frame->dataLength < 3U)
            {
                result = ISOTP_E_INVALID_DLC;
            }
            else if (context->transmit.state !=
                     ISOTP_TX_STATE_WAIT_FLOW_CONTROL)
            {
                result = ISOTP_E_UNEXPECTED_FRAME;
            }
            else if ((flowStatus != ISOTP_FLOW_STATUS_CTS) ||
                     (frame->data[1] != ISOTP_BASIC_BLOCK_SIZE) ||
                     (frame->data[2] > 0x7FU))
            {
                IsoTp_ClearTransmit(context);
                result = ISOTP_E_UNSUPPORTED_FLOW_CONTROL;
            }
            else
            {
                context->transmit.separationTimeMs = frame->data[2];
                context->transmit.state =
                    ISOTP_TX_STATE_CONSECUTIVE_FRAME_READY;
                *event = ISOTP_RX_EVENT_FLOW_CONTROL_RECEIVED;
                result = ISOTP_E_OK;
            }
        }
        else
        {
            result = ISOTP_E_INVALID_FRAME_TYPE;
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
        }
    }

    return result;
}

/** @brief Copies and consumes one fully reassembled payload. */
IsoTp_ReturnType IsoTp_ReadPayload(IsoTp_ContextType *context,
                                   uint8_t *payload,
                                   uint16_t payloadCapacity,
                                   uint16_t *payloadLength)
{
    IsoTp_ReturnType result;

    if ((context == 0) || (payload == 0) || (payloadLength == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else if (context->receive.state != ISOTP_RX_STATE_PAYLOAD_READY)
    {
        result = ISOTP_E_NO_PAYLOAD_AVAILABLE;
    }
    else if (payloadCapacity < context->receive.totalLength)
    {
        result = ISOTP_E_SMALL_BUFFER;
    }
    else
    {
<<<<<<< HEAD
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
=======
        result = IsoTp_CopyBytes(payload, context->receive.buffer,
                                 context->receive.totalLength);
        if (result == ISOTP_E_OK)
        {
            *payloadLength = context->receive.totalLength;
            IsoTp_ClearReceive(context);
        }
        else
        {
            /* The copy error is returned unchanged. */
>>>>>>> e98a2444867808aac73b75870be508765c73a64d
        }
    }

    return result;
}

/** @brief Reads passive transport status for main.c. */
IsoTp_ReturnType IsoTp_GetStatus(const IsoTp_ContextType *context,
                                 IsoTp_StatusType *status)
{
    IsoTp_ReturnType result;

    if ((context == 0) || (status == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else
    {
        status->transmitState = context->transmit.state;
        status->receiveState = context->receive.state;
        status->separationTimeMs = context->transmit.separationTimeMs;
        result = ISOTP_E_OK;
    }

    return result;
}

/** @brief Resets the selected passive direction. */
IsoTp_ReturnType IsoTp_Reset(IsoTp_ContextType *context,
                             IsoTp_ResetDirectionType direction)
{
    IsoTp_ReturnType result;

    if (context == 0)
    {
        result = ISOTP_E_NULL_PTR;
    }
    else if (context->initialized == 0U)
    {
        result = ISOTP_E_NOT_INITIALIZED;
    }
    else if ((direction != ISOTP_RESET_TRANSMIT) &&
             (direction != ISOTP_RESET_RECEIVE) &&
             (direction != ISOTP_RESET_BOTH))
    {
        result = ISOTP_E_INVALID_STATE;
    }
    else
    {
        if ((direction == ISOTP_RESET_TRANSMIT) ||
            (direction == ISOTP_RESET_BOTH))
        {
            IsoTp_ClearTransmit(context);
        }
        else
        {
            /* The transmit direction remains unchanged. */
        }

        if ((direction == ISOTP_RESET_RECEIVE) ||
            (direction == ISOTP_RESET_BOTH))
        {
            IsoTp_ClearReceive(context);
        }
        else
        {
            /* The receive direction remains unchanged. */
        }
        result = ISOTP_E_OK;
    }

    return result;
}
