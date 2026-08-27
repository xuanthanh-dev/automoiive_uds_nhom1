/**
 * @file    isotp.c
 * @brief   Trien khai tang ISO-TP (ISO 15765-2) tren CAN.
 * @details Issue: #19 (Single Frame), #21 (Multi-frame FF/CF/FC)
 *          Requirement: SWR-ISO-TP-001, SWR-ISO-TP-002, SYS-003
 *
 * @note    Coding standard (production, MISRA-friendly):
 *          - Moi ham chi co mot lenh return
 *          - Moi tham so dau vao deu duoc kiem tra
 *          - Moi nhanh if deu co else
 *          - Loi duoc luu vao context.lastError va tang errorCounter
 *
 */

#include "isotp.h"

/*----------------------------------------------------------------------------
 * Bien module - toan bo trang thai nam trong mot struct duy nhat
 *--------------------------------------------------------------------------*/
static IsoTp_ContextType isoTpContext;

/* Thoi gian cua lan goi hien tai (set boi Send/OnCanFrame/MainFunction).
   Cac ham Handle noi bo dung de set timer, khong phu thuoc thu tu goi. */
static uint32_t isoTpCurrentTimeMs;

/*----------------------------------------------------------------------------
 * Ham phu tro: ghi nhan loi (luu ma loi + tang bo dem)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Ghi nhan mot loi vao context de chan doan.
 * @param  errorCode  Ma loi can ghi nhan.
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
 * @param  destination     Con tro dich (khong duoc NULL).
 * @param  source          Con tro nguon (khong duoc NULL).
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

    if ((destination == 0) || (source == 0))
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
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
 * Giai ma STmin tu gia tri byte FC sang don vi ms
 *--------------------------------------------------------------------------*/

/**
 * @brief  Chuyen doi gia tri STmin trong FC frame sang ms.
 * @details ISO 15765-2 6.5.5.5:
 *          0x00-0x7F : 0..127 ms (truc tiep).
 *          0xF1-0xF9 : 100us..900us -> lam tron len 1 ms.
 *          Gia tri khac (0x80-0xF0, 0xFA-0xFF): du phong, xu ly nhu 0 ms.
 * @param  stMinRaw  Byte STmin tu FC frame[2].
 * @return Gia tri STmin tinh bang ms (uint8_t, toi da 127).
 */
static uint8_t IsoTp_DecodeStMin(uint8_t stMinRaw)
{
    uint8_t stMinMs;

    if (stMinRaw <= ISOTP_STMIN_MAX_MS)
    {
        /* 0x00-0x7F: gia tri ms truc tiep */
        stMinMs = stMinRaw;
    }
    else if ((stMinRaw >= ISOTP_STMIN_US_MIN) && (stMinRaw <= ISOTP_STMIN_US_MAX))
    {
        /* 0xF1-0xF9: 100us-900us, lam tron len 1 ms */
        stMinMs = 1u;
    }
    else
    {
        /* Gia tri du phong: xu ly nhu 0 ms (gui ngay) */
        stMinMs = 0u;
    }

    return stMinMs;
}

/*----------------------------------------------------------------------------
 * Gui Single Frame (payload <= 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Single Frame.
 * @param  message  Con tro du lieu (da duoc kiem tra o ham goi).
 * @param  length   So byte (da duoc kiem tra <= 7 o ham goi).
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
 * Gui First Frame (bat dau ban tin > 7 byte)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot First Frame va luu 6 byte dau vao context.
 * @return ISOTP_OK, hoac ma loi.
 *
 */
static IsoTp_StatusType IsoTp_SendFirstFrame(void)
{
    IsoTp_StatusType status;
    uint8_t          frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;

    /* Bao ve: totalLength phai >= 6 byte de dien day FF payload */
    if (isoTpContext.tx.totalLength < ISOTP_FF_FIRST_PAYLOAD)
    {
        IsoTp_RecordError(ISOTP_ERR_SF_BAD_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    else
    {
        /* PCI 12 bit do dai: 4 bit cao o byte 0, 8 bit thap o byte 1 */
        frame[0] = (uint8_t)(ISOTP_PCI_FIRST_FRAME |
                             ((isoTpContext.tx.totalLength >> 8u) & ISOTP_PCI_VALUE_MASK));
        frame[1] = (uint8_t)(isoTpContext.tx.totalLength & 0xFFu);

        for (index = 0u; index < ISOTP_FF_FIRST_PAYLOAD; index++)
        {
            frame[index + 2u] = isoTpContext.tx.buffer[index];
        }

        if (isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE) != 0u)
        {
            IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
            status = ISOTP_ERROR_STATE;
        }
        else
        {
            /* Only advance once the First Frame was accepted by CAN. */
            isoTpContext.tx.sentIndex = ISOTP_FF_FIRST_PAYLOAD;
            status = ISOTP_OK;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui mot Consecutive Frame
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Consecutive Frame tiep theo tu context.
 * @return ISOTP_OK, hoac ma loi.
 *
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
        /* So sanh index (uint8_t) voi bytesToSend (uint16_t) truc tiep,
           khong can cast vi bytesToSend luon <= 7 trong nhanh nay. */
        if ((uint16_t)index < bytesToSend)
        {
            frame[index + 1u] =
                isoTpContext.tx.buffer[isoTpContext.tx.sentIndex + index];
        }
        else
        {
            frame[index + 1u] = 0x00u;  /* Padding */
        }
    }

    if (isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE) != 0u)
    {
        /* Do not advance index/SN: retry this exact CF in the next cycle. */
        IsoTp_RecordError(ISOTP_ERR_SEND_FAILED);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        isoTpContext.tx.sentIndex += bytesToSend;

        /* SN cuon vong 0..15 */
        isoTpContext.tx.sequenceNumber =
            (uint8_t)((isoTpContext.tx.sequenceNumber + 1u) &
                      ISOTP_PCI_VALUE_MASK);

        /* Cap nhat blockCounter va kiem tra gioi han block */
        isoTpContext.tx.blockCounter++;

        if ((isoTpContext.tx.blockSize != 0u) &&
            (isoTpContext.tx.blockCounter >= isoTpContext.tx.blockSize))
        {
            /* Da gui du so CF trong block nay, cho FC tiep theo */
            isoTpContext.tx.state          = ISOTP_TX_WAIT_FC;
            isoTpContext.tx.timerStartMs   = isoTpCurrentTimeMs;
            isoTpContext.tx.blockCounter   = 0u;
        }
        else
        {
            /* Con trong block, tiep tuc gui o chu ky sau */
        }

        status = ISOTP_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly Single Frame nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly Single Frame nhan duoc, giao payload len tang tren.
 * @param  frame  Con tro 8 byte frame (da kiem tra NULL o ham goi).
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedSingleFrame(const uint8_t *frame,
                                                        uint8_t        dlc)
{
    IsoTp_StatusType status;
    uint8_t          payloadLength;

    payloadLength = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    /* Kiem tra do dai hop le: 1..7 */
    if ((payloadLength == 0u) ||
        (payloadLength > ISOTP_SF_MAX_PAYLOAD) ||
        ((uint16_t)payloadLength > ((uint16_t)dlc - 1u)))
    {
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
            /* Loi da duoc ghi nhan trong IsoTp_CopyBytes */
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui Flow Control (CTS)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Gui mot Flow Control frame (CTS - cho phep gui tiep).
 * @details Block Size = 0 (gui het), STmin = 0 (khong cho giua frame).
 * @return ISOTP_OK neu gui thanh cong, ISOTP_ERROR_STATE neu canSend that bai.
 */
static IsoTp_StatusType IsoTp_SendFlowControl(void)
{
    IsoTp_StatusType status;
    uint8_t          flowControlFrame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;
    uint8_t          sendResult;

    flowControlFrame[0] =
        (uint8_t)(ISOTP_PCI_FLOW_CONTROL | ISOTP_FC_CONTINUE_TO_SEND);
    flowControlFrame[1] = 0x00u;  /* Block Size = 0 (gui het)         */
    flowControlFrame[2] = 0x00u;  /* STmin = 0 (khong cho giua frame) */
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

/*----------------------------------------------------------------------------
 * Xu ly First Frame nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly First Frame: luu 6 byte dau, gui Flow Control.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedFirstFrame(const uint8_t *frame,
                                                       uint8_t        dlc)
{
    IsoTp_StatusType status;

    /* Ghep 12 bit do dai tu byte 0 va byte 1 */
    isoTpContext.rx.totalLength =
        (uint16_t)(((uint16_t)(frame[0] & ISOTP_PCI_VALUE_MASK) << 8u) |
                   (uint16_t)frame[1]);

    /* Kiem tra do dai vuot buffer */
    if ((dlc < ISOTP_CAN_FRAME_SIZE) ||
        (isoTpContext.rx.totalLength <= ISOTP_SF_MAX_PAYLOAD) ||
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

#if (ISOTP_USE_FLOW_CONTROL != 0u)
            isoTpContext.rx.state          = ISOTP_RX_WAIT_FC_TX;
            isoTpContext.rx.timerStartMs   = isoTpCurrentTimeMs;

            /* Gui Flow Control cho phep gui tiep */
            status = IsoTp_SendFlowControl();

            if (status == ISOTP_OK)
            {
                isoTpContext.rx.state        = ISOTP_RX_RECEIVING_CF;
                isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;
            }
            else
            {
                /* CAN mailbox is busy: retain the session and retry in MainFunction. */
            }
#else
            /* Compatibility mode: accept CF immediately after First Frame. */
            isoTpContext.rx.state        = ISOTP_RX_RECEIVING_CF;
            isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;
#endif
        }
        else
        {
            isoTpContext.rx.state = ISOTP_RX_IDLE;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Xu ly Consecutive Frame nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly Consecutive Frame: kiem tra SN, ghep du lieu.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedConsecutiveFrame(const uint8_t *frame,
                                                             uint8_t        dlc)
{
    IsoTp_StatusType status;
    uint8_t          receivedSequenceNumber;
    uint16_t         remaining;
    uint16_t         bytesToCopy;

    /* Chi xu ly khi dang o trang thai nhan CF */
    if (isoTpContext.rx.state != ISOTP_RX_RECEIVING_CF)
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

            if ((uint16_t)dlc < (bytesToCopy + 1u))
            {
                IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
                isoTpContext.rx.state = ISOTP_RX_IDLE;
                status = ISOTP_ERROR_FRAME;
            }
            else
            {
                status = IsoTp_CopyBytes(
                             &isoTpContext.rx.buffer[isoTpContext.rx.receivedIndex],
                             &frame[1],
                             bytesToCopy,
                             (uint16_t)(ISOTP_MAX_MESSAGE_SIZE -
                                        isoTpContext.rx.receivedIndex));
            }

            if (status == ISOTP_OK)
            {
                isoTpContext.rx.receivedIndex += bytesToCopy;
                isoTpContext.rx.sequenceNumber =
                    (uint8_t)((isoTpContext.rx.sequenceNumber + 1u) &
                              ISOTP_PCI_VALUE_MASK);

                /* Reset dong ho N_Cr sau moi CF hop le */
                isoTpContext.rx.timerStartMs = isoTpCurrentTimeMs;

                /* Da nhan du toan bo ban tin */
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
 * Xu ly Flow Control nhan duoc (phia truyen)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly Flow Control: doc BS/STmin, neu CTS thi cho phep gui CF.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 *
 */
static IsoTp_StatusType IsoTp_HandleReceivedFlowControl(const uint8_t *frame,
                                                        uint8_t        dlc)
{
    IsoTp_StatusType status;
#if (ISOTP_USE_FLOW_CONTROL != 0u)
    uint8_t          flowStatus;
#endif

    /* Chi xu ly khi dang cho Flow Control; dung ma loi dung */
#if (ISOTP_USE_FLOW_CONTROL == 0u)
    (void)frame;
    (void)dlc;
    status = ISOTP_OK;
#else
    if (dlc < 3u)
    {
        IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
        status = ISOTP_ERROR_FRAME;
    }
    else if (isoTpContext.tx.state != ISOTP_TX_WAIT_FC)
    {
        IsoTp_RecordError(ISOTP_ERR_UNEXPECTED_FC);
        status = ISOTP_ERROR_STATE;
    }
    else
    {
        flowStatus = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

        if (flowStatus == ISOTP_FC_CONTINUE_TO_SEND)
        {
            /* Luu Block Size va STmin tu FC frame */
            isoTpContext.tx.blockSize    = frame[1];
            isoTpContext.tx.blockCounter = 0u;
            isoTpContext.tx.stMinMs      = IsoTp_DecodeStMin(frame[2]);

            /* Khoi dong timer STmin cho CF dau tien */
            isoTpContext.tx.stMinTimerStartMs = isoTpCurrentTimeMs;

            isoTpContext.tx.state = ISOTP_TX_SENDING_CF;

            status = ISOTP_OK;
        }
        else if (flowStatus == ISOTP_FC_WAIT)
        {
            /* Phia nhan yeu cau cho - reset timer N_Bs, giu nguyen trang thai */
            isoTpContext.tx.timerStartMs = isoTpCurrentTimeMs;
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
#endif

    return status;
}

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao tang ISO-TP. Xem mo ta trong isotp.h.
 *
 */
IsoTp_StatusType IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                            IsoTp_RxCallbackType rxCallback)
{
    IsoTp_StatusType status;
    uint8_t          index;

    if ((canSendFunction == 0) || (rxCallback == 0))
    {
        status = ISOTP_ERROR_NULL;
    }
    else
    {
        isoTpContext.canSend    = canSendFunction;
        isoTpContext.rxCallback = rxCallback;

        /* TX context */
        isoTpContext.tx.state              = ISOTP_TX_IDLE;
        isoTpContext.tx.totalLength        = 0u;
        isoTpContext.tx.sentIndex          = 0u;
        isoTpContext.tx.sequenceNumber     = 0u;
        isoTpContext.tx.timerStartMs       = 0u;
        isoTpContext.tx.blockSize          = 0u;
        isoTpContext.tx.blockCounter       = 0u;
        isoTpContext.tx.stMinMs            = 0u;
        isoTpContext.tx.stMinTimerStartMs  = 0u;

        /* RX context */
        isoTpContext.rx.state          = ISOTP_RX_IDLE;
        isoTpContext.rx.totalLength    = 0u;
        isoTpContext.rx.receivedIndex  = 0u;
        isoTpContext.rx.sequenceNumber = 0u;
        isoTpContext.rx.timerStartMs   = 0u;

        /* Zero-init buffers */
        for (index = 0u; index < ISOTP_MAX_MESSAGE_SIZE; index++)
        {
            isoTpContext.tx.buffer[index] = 0u;
            isoTpContext.rx.buffer[index] = 0u;
        }

        isoTpContext.lastError     = ISOTP_ERR_NONE;
        isoTpContext.errorCounter  = 0u;
        isoTpCurrentTimeMs         = 0u;
        isoTpContext.isInitialised = 1u;

        status = ISOTP_OK;
    }

    return status;
}

/**
 * @brief  Dat lai trang thai TX va RX ve Idle ma khong mat callback.
 */
void IsoTp_Reset(void)
{
    uint8_t index;

    isoTpContext.tx.state             = ISOTP_TX_IDLE;
    isoTpContext.tx.totalLength       = 0u;
    isoTpContext.tx.sentIndex         = 0u;
    isoTpContext.tx.sequenceNumber    = 0u;
    isoTpContext.tx.timerStartMs      = 0u;
    isoTpContext.tx.blockSize         = 0u;
    isoTpContext.tx.blockCounter      = 0u;
    isoTpContext.tx.stMinMs           = 0u;
    isoTpContext.tx.stMinTimerStartMs = 0u;

    isoTpContext.rx.state          = ISOTP_RX_IDLE;
    isoTpContext.rx.totalLength    = 0u;
    isoTpContext.rx.receivedIndex  = 0u;
    isoTpContext.rx.sequenceNumber = 0u;
    isoTpContext.rx.timerStartMs   = 0u;

    for (index = 0u; index < ISOTP_MAX_MESSAGE_SIZE; index++)
    {
        isoTpContext.tx.buffer[index] = 0u;
        isoTpContext.rx.buffer[index] = 0u;
    }

    /* Giu nguyen: canSend, rxCallback, isInitialised, lastError, errorCounter */
}

/**
 * @brief  Gui mot ban tin qua ISO-TP. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message,
                            uint16_t       length,
                            uint32_t       currentTimeMs)
{
    IsoTp_StatusType status;

    isoTpCurrentTimeMs = currentTimeMs;

    if (isoTpContext.isInitialised == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_NOT_INITIALISED);
        status = ISOTP_ERROR_STATE;
    }
    else if (message == 0)
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
    else if (length == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_ZERO_LENGTH);
        status = ISOTP_ERROR_SIZE;
    }
    else if (length > ISOTP_MAX_MESSAGE_SIZE)
    {
        IsoTp_RecordError(ISOTP_ERR_LENGTH_EXCEEDED);
        status = ISOTP_ERROR_SIZE;
    }
    else if (isoTpContext.tx.state != ISOTP_TX_IDLE)
    {
        status = ISOTP_BUSY;
    }
    else if (length <= ISOTP_SF_MAX_PAYLOAD)
    {
        status = IsoTp_SendSingleFrame(message, length);
    }
    else
    {
        status = IsoTp_CopyBytes(isoTpContext.tx.buffer,
                                 message,
                                 length,
                                 ISOTP_MAX_MESSAGE_SIZE);

        if (status == ISOTP_OK)
        {
            isoTpContext.tx.totalLength       = length;
            isoTpContext.tx.sequenceNumber    = 1u;
            isoTpContext.tx.blockSize         = 0u;
            isoTpContext.tx.blockCounter      = 0u;
            isoTpContext.tx.stMinMs           = 0u;
            isoTpContext.tx.stMinTimerStartMs = 0u;
#if (ISOTP_USE_FLOW_CONTROL != 0u)
            isoTpContext.tx.state       = ISOTP_TX_WAIT_FC;
            isoTpContext.tx.timerStartMs = isoTpCurrentTimeMs;
#else
            isoTpContext.tx.state             = ISOTP_TX_SENDING_CF;
            isoTpContext.tx.stMinTimerStartMs = isoTpCurrentTimeMs;
#endif

            status = IsoTp_SendFirstFrame();

            if (status != ISOTP_OK)
            {
                /* A First Frame that was not queued must not wait for FC. */
                isoTpContext.tx.state       = ISOTP_TX_IDLE;
                isoTpContext.tx.totalLength = 0u;
                isoTpContext.tx.sentIndex   = 0u;
            }
            else
            {
                /* First Frame queued; cyclic processing sends remaining CF. */
            }
        }
        else
        {
            /* Loi da ghi nhan trong IsoTp_CopyBytes */
        }
    }

    return status;
}

/**
 * @brief  Xu ly mot CAN frame nhan duoc. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_OnCanFrame(const uint8_t *frame,
                                  uint8_t        dlc,
                                  uint32_t       currentTimeMs)
{
    IsoTp_StatusType status;
    uint8_t          frameType;

    isoTpCurrentTimeMs = currentTimeMs;

    if (isoTpContext.isInitialised == 0u)
    {
        IsoTp_RecordError(ISOTP_ERR_NOT_INITIALISED);
        status = ISOTP_ERROR_STATE;
    }
    else if (frame == 0)
    {
        IsoTp_RecordError(ISOTP_ERR_NULL_POINTER);
        status = ISOTP_ERROR_NULL;
    }
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
            IsoTp_RecordError(ISOTP_ERR_INVALID_FRAME);
            status = ISOTP_ERROR_FRAME;
        }
    }

    return status;
}

/**
 * @brief  Ham chu ky. Xem mo ta trong isotp.h.
 *
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

        isoTpCurrentTimeMs = currentTimeMs;

        /* --- TX state machine --- */
        if (isoTpContext.tx.state == ISOTP_TX_SENDING_CF)
        {
            /* Kiem tra STmin truoc khi gui CF */
            if ((currentTimeMs - isoTpContext.tx.stMinTimerStartMs) >=
                (uint32_t)isoTpContext.tx.stMinMs)
            {
                if (isoTpContext.tx.sentIndex < isoTpContext.tx.totalLength)
                {
                    status = IsoTp_SendConsecutiveFrame();

                    /* Khoi dong lai timer STmin cho CF tiep theo */
                    isoTpContext.tx.stMinTimerStartMs = currentTimeMs;
                }
                else
                {
                    /* Khong con byte de gui */
                }

                /* Kiem tra hoan thanh toan bo ban tin */
                if (isoTpContext.tx.sentIndex >= isoTpContext.tx.totalLength)
                {
                    isoTpContext.tx.state = ISOTP_TX_IDLE;
                }
                else
                {
                    /* Con byte, cho lan MainFunction sau */
                }
            }
            else
            {
                /* Chua het STmin, cho them */
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
                status = ISOTP_ERROR_TIMEOUT;
            }
            else
            {
                /* Chua het gio */
            }
        }
        else
        {
            /* TX Idle, khong lam gi */
        }

        /* --- RX: retry Flow Control, then check N_Cr for CF --- */
        if (isoTpContext.rx.state == ISOTP_RX_WAIT_FC_TX)
        {
            if ((currentTimeMs - isoTpContext.rx.timerStartMs) >
                ISOTP_TIMEOUT_N_CR_MS)
            {
                IsoTp_RecordError(ISOTP_ERR_RX_TIMEOUT);
                isoTpContext.rx.state = ISOTP_RX_IDLE;
                status = ISOTP_ERROR_TIMEOUT;
            }
            else
            {
                status = IsoTp_SendFlowControl();

                if (status == ISOTP_OK)
                {
                    isoTpContext.rx.state        = ISOTP_RX_RECEIVING_CF;
                    isoTpContext.rx.timerStartMs = currentTimeMs;
                }
                else
                {
                    /* Keep RX state so the next cyclic call retries FC. */
                }
            }
        }
        else if (isoTpContext.rx.state == ISOTP_RX_RECEIVING_CF)
        {
            if ((currentTimeMs - isoTpContext.rx.timerStartMs) >
                ISOTP_TIMEOUT_N_CR_MS)
            {
                IsoTp_RecordError(ISOTP_ERR_RX_TIMEOUT);
                isoTpContext.rx.state = ISOTP_RX_IDLE;
                status = ISOTP_ERROR_TIMEOUT;
            }
            else
            {
                /* Chua het gio */
            }
        }
        else
        {
            /* RX Idle, khong lam gi */
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
