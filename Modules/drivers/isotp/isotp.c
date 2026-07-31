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
 */

#include "isotp.h"

/*----------------------------------------------------------------------------
 * Bien module - toan bo trang thai nam trong mot struct duy nhat
 *--------------------------------------------------------------------------*/
static IsoTp_ContextType isoTpContext;

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

        isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE);
        status = ISOTP_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Gui First Frame (bat dau ban tin > 7 byte)
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

    isoTpContext.tx.sentIndex = ISOTP_FF_FIRST_PAYLOAD;
    isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE);
    status = ISOTP_OK;

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

    isoTpContext.tx.sentIndex += bytesToSend;

    /* SN cuon vong 0..15 */
    isoTpContext.tx.sequenceNumber =
        (uint8_t)((isoTpContext.tx.sequenceNumber + 1u) & ISOTP_PCI_VALUE_MASK);

    isoTpContext.canSend(frame, ISOTP_CAN_FRAME_SIZE);
    status = ISOTP_OK;

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
static IsoTp_StatusType IsoTp_HandleReceivedSingleFrame(const uint8_t *frame)
{
    IsoTp_StatusType status;
    uint8_t          payloadLength;

    payloadLength = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    /* Kiem tra do dai hop le: 1..7 */
    if ((payloadLength == 0u) || (payloadLength > ISOTP_SF_MAX_PAYLOAD))
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
 * Xu ly First Frame nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly First Frame: luu 6 byte dau, gui Flow Control.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedFirstFrame(const uint8_t *frame)
{
    IsoTp_StatusType status;
    uint8_t          flowControlFrame[ISOTP_CAN_FRAME_SIZE];
    uint8_t          index;

    /* Ghep 12 bit do dai tu byte 0 va byte 1 */
    isoTpContext.rx.totalLength =
        (uint16_t)(((uint16_t)(frame[0] & ISOTP_PCI_VALUE_MASK) << 8u) |
                   (uint16_t)frame[1]);

    /* Kiem tra do dai vuot buffer */
    if (isoTpContext.rx.totalLength > ISOTP_MAX_MESSAGE_SIZE)
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

            /* Gui Flow Control: cho phep gui tiep */
            flowControlFrame[0] =
                (uint8_t)(ISOTP_PCI_FLOW_CONTROL | ISOTP_FC_CONTINUE_TO_SEND);
            flowControlFrame[1] = 0x00u;  /* Block Size = 0 */
            flowControlFrame[2] = 0x00u;  /* STmin = 0      */
            for (index = 3u; index < ISOTP_CAN_FRAME_SIZE; index++)
            {
                flowControlFrame[index] = 0x00u;
            }

            isoTpContext.canSend(flowControlFrame, ISOTP_CAN_FRAME_SIZE);
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
static IsoTp_StatusType IsoTp_HandleReceivedConsecutiveFrame(const uint8_t *frame)
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
 * @brief  Xu ly Flow Control: neu CTS thi cho phep gui CF.
 * @param  frame  Con tro 8 byte frame.
 * @return ISOTP_OK, hoac ma loi.
 */
static IsoTp_StatusType IsoTp_HandleReceivedFlowControl(const uint8_t *frame)
{
    IsoTp_StatusType status;
    uint8_t          flowStatus;

    /* Chi xu ly khi dang cho Flow Control */
    if (isoTpContext.tx.state != ISOTP_TX_WAIT_FC)
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
            /* Phia nhan yeu cau cho - giu nguyen trang thai */
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

        isoTpContext.lastError     = ISOTP_ERR_NONE;
        isoTpContext.errorCounter  = 0u;
        isoTpContext.isInitialised = 1u;

        status = ISOTP_OK;
    }

    return status;
}

/**
 * @brief  Gui mot ban tin qua ISO-TP. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message, uint16_t length)
{
    IsoTp_StatusType status;

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
    /* Kiem tra dang ban khong */
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

            status = IsoTp_SendFirstFrame();
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
IsoTp_StatusType IsoTp_OnCanFrame(const uint8_t *frame, uint8_t dlc)
{
    IsoTp_StatusType status;
    uint8_t          frameType;

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
            status = IsoTp_HandleReceivedSingleFrame(frame);
        }
        else if (frameType == ISOTP_PCI_FIRST_FRAME)
        {
            status = IsoTp_HandleReceivedFirstFrame(frame);
        }
        else if (frameType == ISOTP_PCI_CONSECUTIVE_FRAME)
        {
            status = IsoTp_HandleReceivedConsecutiveFrame(frame);
        }
        else if (frameType == ISOTP_PCI_FLOW_CONTROL)
        {
            status = IsoTp_HandleReceivedFlowControl(frame);
        }
        else
        {
            /* Loai frame khong thuoc 4 loai hop le - luu loi */
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

        /* Neu dang gui CF thi gui frame tiep theo */
        if (isoTpContext.tx.state == ISOTP_TX_SENDING_CF)
        {
            if (isoTpContext.tx.sentIndex < isoTpContext.tx.totalLength)
            {
                status = IsoTp_SendConsecutiveFrame();
            }
            else
            {
                /* Khong con byte de gui */
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

        /* Kiem tra timeout cho Consecutive Frame (N_Cr) */
        if (isoTpContext.rx.state == ISOTP_RX_RECEIVING_CF)
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
