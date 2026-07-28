/**
 * @file    isotp.c
 * @brief   Trien khai tang ISO-TP (ISO 15765-2) tren CAN.
 * @details Issue: #19 (Single Frame), #21 (Multi-frame FF/CF/FC)
 *          Requirement: SWR-ISO-TP-001, SWR-ISO-TP-002, SYS-003
 */

#include "isotp.h"

/*----------------------------------------------------------------------------
 * Trang thai cua may trang thai truyen (TX)
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_TX_IDLE          = 0u,  /* Khong truyen                       */
    ISOTP_TX_WAIT_FC       = 1u,  /* Da gui FF, dang cho Flow Control   */
    ISOTP_TX_SENDING_CF    = 2u   /* Dang gui cac Consecutive Frame     */
} IsoTp_TxStateType;

/*----------------------------------------------------------------------------
 * Trang thai cua may trang thai nhan (RX)
 *--------------------------------------------------------------------------*/
typedef enum
{
    ISOTP_RX_IDLE          = 0u,  /* Khong nhan                         */
    ISOTP_RX_RECEIVING_CF  = 1u   /* Da nhan FF, dang cho Consecutive   */
} IsoTp_RxStateType;

/*----------------------------------------------------------------------------
 * Bien module (static - chi dung trong file nay)
 *--------------------------------------------------------------------------*/

/* Ham gui CAN frame xuong tang duoi, dang ky luc Init */
static IsoTp_CanSendType    isoTpCanSend;

/* Ham bao len khi nhan xong ban tin day du */
static IsoTp_RxCallbackType isoTpRxCallback;

/* --- Bien phia truyen (TX) --- */
static IsoTp_TxStateType    isoTpTxState;
static uint8_t              isoTpTxBuffer[ISOTP_MAX_MESSAGE_SIZE];
static uint16_t             isoTpTxLength;         /* Tong so byte can gui   */
static uint16_t             isoTpTxIndex;          /* So byte da gui         */
static uint8_t              isoTpTxSequenceNumber; /* SN cua CF tiep theo    */
static uint32_t             isoTpTxTimer;          /* Moc thoi gian cho FC   */

/* --- Bien phia nhan (RX) --- */
static IsoTp_RxStateType    isoTpRxState;
static uint8_t              isoTpRxBuffer[ISOTP_MAX_MESSAGE_SIZE];
static uint16_t             isoTpRxLength;         /* Tong so byte se nhan   */
static uint16_t             isoTpRxIndex;          /* So byte da nhan        */
static uint8_t              isoTpRxSequenceNumber; /* SN mong doi tiep theo  */
static uint32_t             isoTpRxTimer;          /* Moc thoi gian cho CF   */

/*----------------------------------------------------------------------------
 * Ham phu tro (private)
 *--------------------------------------------------------------------------*/

/**
 * @brief  Sao chep mot so byte tu nguon sang dich.
 * @details Tu viet vong lap de tranh phu thuoc <string.h> tren firmware.
 */
static void IsoTp_CopyBytes(uint8_t       *destination,
                            const uint8_t *source,
                            uint16_t       length)
{
    uint16_t index;

    for (index = 0u; index < length; index++)
    {
        destination[index] = source[index];
    }
}

/**
 * @brief  Gui mot Single Frame (payload <= 7 byte).
 * @details Byte 0: 0x0<len>. Byte 1..len: du lieu. Cac byte con lai dem 0x00.
 */
static void IsoTp_SendSingleFrame(const uint8_t *message, uint16_t length)
{
    uint8_t frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t index;

    /* PCI: loai Single Frame + do dai (nam trong 4 bit thap) */
    frame[0] = (uint8_t)(ISOTP_PCI_SINGLE_FRAME | (length & ISOTP_PCI_VALUE_MASK));

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

    isoTpCanSend(frame, ISOTP_CAN_FRAME_SIZE);
}

/**
 * @brief  Gui mot First Frame (bat dau ban tin > 7 byte).
 * @details Byte 0-1: 0x1<len_high> <len_low>. Byte 2..7: 6 byte du lieu dau.
 */
static void IsoTp_SendFirstFrame(void)
{
    uint8_t frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t index;

    /* PCI 12 bit do dai: 4 bit cao o byte 0, 8 bit thap o byte 1 */
    frame[0] = (uint8_t)(ISOTP_PCI_FIRST_FRAME |
                         ((isoTpTxLength >> 8u) & ISOTP_PCI_VALUE_MASK));
    frame[1] = (uint8_t)(isoTpTxLength & 0xFFu);

    for (index = 0u; index < ISOTP_FF_FIRST_PAYLOAD; index++)
    {
        frame[index + 2u] = isoTpTxBuffer[index];
    }

    isoTpTxIndex = ISOTP_FF_FIRST_PAYLOAD;
    isoTpCanSend(frame, ISOTP_CAN_FRAME_SIZE);
}

/**
 * @brief  Gui mot Consecutive Frame.
 * @details Byte 0: 0x2<SN>. Byte 1..7: toi da 7 byte du lieu tiep theo.
 */
static void IsoTp_SendConsecutiveFrame(void)
{
    uint8_t  frame[ISOTP_CAN_FRAME_SIZE];
    uint8_t  index;
    uint16_t remaining;

    frame[0] = (uint8_t)(ISOTP_PCI_CONSECUTIVE_FRAME |
                         (isoTpTxSequenceNumber & ISOTP_PCI_VALUE_MASK));

    remaining = isoTpTxLength - isoTpTxIndex;

    for (index = 0u; index < ISOTP_CF_MAX_PAYLOAD; index++)
    {
        if (index < (uint8_t)remaining)
        {
            frame[index + 1u] = isoTpTxBuffer[isoTpTxIndex + index];
        }
        else
        {
            frame[index + 1u] = 0x00u;  /* Padding */
        }
    }

    /* Cap nhat so byte da gui */
    if (remaining >= ISOTP_CF_MAX_PAYLOAD)
    {
        isoTpTxIndex += ISOTP_CF_MAX_PAYLOAD;
    }
    else
    {
        isoTpTxIndex += remaining;
    }

    /* SN cuon vong 0..15 */
    isoTpTxSequenceNumber = (uint8_t)((isoTpTxSequenceNumber + 1u) &
                                      ISOTP_PCI_VALUE_MASK);

    isoTpCanSend(frame, ISOTP_CAN_FRAME_SIZE);
}

/*----------------------------------------------------------------------------
 * Xu ly tung loai frame nhan duoc
 *--------------------------------------------------------------------------*/

/**
 * @brief  Xu ly Single Frame nhan duoc.
 * @details Tach payload va bao len tang tren ngay.
 */
static void IsoTp_HandleReceivedSingleFrame(const uint8_t *frame)
{
    uint8_t payloadLength;

    payloadLength = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    if ((payloadLength > 0u) && (payloadLength <= ISOTP_SF_MAX_PAYLOAD))
    {
        IsoTp_CopyBytes(isoTpRxBuffer, &frame[1], payloadLength);
        isoTpRxCallback(isoTpRxBuffer, (uint16_t)payloadLength);
    }
}

/**
 * @brief  Xu ly First Frame nhan duoc.
 * @details Doc tong do dai, luu 6 byte dau, gui Flow Control cho phep gui tiep.
 */
static void IsoTp_HandleReceivedFirstFrame(const uint8_t *frame)
{
    uint8_t flowControlFrame[ISOTP_CAN_FRAME_SIZE];
    uint8_t index;

    /* Ghep 12 bit do dai tu byte 0 va byte 1 */
    isoTpRxLength = (uint16_t)(((uint16_t)(frame[0] & ISOTP_PCI_VALUE_MASK) << 8u) |
                               (uint16_t)frame[1]);

    /* Bao ve tran buffer */
    if (isoTpRxLength > ISOTP_MAX_MESSAGE_SIZE)
    {
        isoTpRxState = ISOTP_RX_IDLE;
        return;
    }

    /* Luu 6 byte du lieu dau tien */
    IsoTp_CopyBytes(isoTpRxBuffer, &frame[2], ISOTP_FF_FIRST_PAYLOAD);
    isoTpRxIndex          = ISOTP_FF_FIRST_PAYLOAD;
    isoTpRxSequenceNumber = 1u;   /* CF dau tien co SN = 1 */
    isoTpRxState          = ISOTP_RX_RECEIVING_CF;

    /* Gui Flow Control: cho phep gui tiep, khong gioi han block, khong tre */
    flowControlFrame[0] = (uint8_t)(ISOTP_PCI_FLOW_CONTROL | ISOTP_FC_CONTINUE_TO_SEND);
    flowControlFrame[1] = 0x00u;  /* Block Size = 0 (gui het khong cho)     */
    flowControlFrame[2] = 0x00u;  /* STmin = 0 (khong tre giua cac CF)      */
    for (index = 3u; index < ISOTP_CAN_FRAME_SIZE; index++)
    {
        flowControlFrame[index] = 0x00u;
    }

    isoTpCanSend(flowControlFrame, ISOTP_CAN_FRAME_SIZE);
}

/**
 * @brief  Xu ly Consecutive Frame nhan duoc.
 * @details Kiem tra SN, ghep du lieu. Khi du do dai, bao len tang tren.
 */
static void IsoTp_HandleReceivedConsecutiveFrame(const uint8_t *frame)
{
    uint8_t  receivedSequenceNumber;
    uint16_t remaining;
    uint16_t bytesToCopy;

    /* Bo qua neu khong o trang thai dang nhan CF */
    if (isoTpRxState != ISOTP_RX_RECEIVING_CF)
    {
        return;
    }

    receivedSequenceNumber = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    /* Kiem tra SN co dung thu tu mong doi khong */
    if (receivedSequenceNumber != isoTpRxSequenceNumber)
    {
        isoTpRxState = ISOTP_RX_IDLE;  /* Sai thu tu - huy phien */
        return;
    }

    remaining = isoTpRxLength - isoTpRxIndex;

    if (remaining >= ISOTP_CF_MAX_PAYLOAD)
    {
        bytesToCopy = ISOTP_CF_MAX_PAYLOAD;
    }
    else
    {
        bytesToCopy = remaining;
    }

    IsoTp_CopyBytes(&isoTpRxBuffer[isoTpRxIndex], &frame[1], bytesToCopy);
    isoTpRxIndex += bytesToCopy;

    /* SN mong doi tiep theo, cuon vong 0..15 */
    isoTpRxSequenceNumber = (uint8_t)((isoTpRxSequenceNumber + 1u) &
                                      ISOTP_PCI_VALUE_MASK);

    /* Da nhan du toan bo ban tin */
    if (isoTpRxIndex >= isoTpRxLength)
    {
        isoTpRxState = ISOTP_RX_IDLE;
        isoTpRxCallback(isoTpRxBuffer, isoTpRxLength);
    }
}

/**
 * @brief  Xu ly Flow Control nhan duoc (phia truyen).
 * @details Khi nhan CTS, chuyen sang gui cac Consecutive Frame.
 */
static void IsoTp_HandleReceivedFlowControl(const uint8_t *frame)
{
    uint8_t flowStatus;

    /* Chi xu ly neu dang cho Flow Control */
    if (isoTpTxState != ISOTP_TX_WAIT_FC)
    {
        return;
    }

    flowStatus = (uint8_t)(frame[0] & ISOTP_PCI_VALUE_MASK);

    if (flowStatus == ISOTP_FC_CONTINUE_TO_SEND)
    {
        isoTpTxState = ISOTP_TX_SENDING_CF;
        /* Cac CF se duoc gui trong IsoTp_MainFunction */
    }
    else
    {
        /* WAIT hoac OVERFLOW - huy phien truyen */
        isoTpTxState = ISOTP_TX_IDLE;
    }
}

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao tang ISO-TP. Xem mo ta trong isotp.h.
 */
void IsoTp_Init(IsoTp_CanSendType    canSendFunction,
                IsoTp_RxCallbackType rxCallback)
{
    isoTpCanSend    = canSendFunction;
    isoTpRxCallback = rxCallback;

    isoTpTxState          = ISOTP_TX_IDLE;
    isoTpTxLength         = 0u;
    isoTpTxIndex          = 0u;
    isoTpTxSequenceNumber = 0u;
    isoTpTxTimer          = 0u;

    isoTpRxState          = ISOTP_RX_IDLE;
    isoTpRxLength         = 0u;
    isoTpRxIndex          = 0u;
    isoTpRxSequenceNumber = 0u;
    isoTpRxTimer          = 0u;
}

/**
 * @brief  Gui mot ban tin qua ISO-TP. Xem mo ta trong isotp.h.
 */
IsoTp_StatusType IsoTp_Send(const uint8_t *message, uint16_t length)
{
    IsoTp_StatusType status;

    status = ISOTP_OK;

    if ((message == 0) || (isoTpCanSend == 0))
    {
        status = ISOTP_ERROR_NULL;
    }
    else if (length > ISOTP_MAX_MESSAGE_SIZE)
    {
        status = ISOTP_ERROR_SIZE;
    }
    else if (isoTpTxState != ISOTP_TX_IDLE)
    {
        status = ISOTP_BUSY;
    }
    else if (length <= ISOTP_SF_MAX_PAYLOAD)
    {
        /* Truong hop don gian: mot Single Frame la du */
        IsoTp_SendSingleFrame(message, length);
    }
    else
    {
        /* Ban tin dai: bat dau chuoi multi-frame */
        IsoTp_CopyBytes(isoTpTxBuffer, message, length);
        isoTpTxLength         = length;
        isoTpTxSequenceNumber = 1u;
        isoTpTxState          = ISOTP_TX_WAIT_FC;

        IsoTp_SendFirstFrame();
    }

    return status;
}

/**
 * @brief  Xu ly mot CAN frame nhan duoc. Xem mo ta trong isotp.h.
 */
void IsoTp_OnCanFrame(const uint8_t *frame, uint8_t dlc)
{
    uint8_t frameType;

    if ((frame == 0) || (dlc == 0u))
    {
        return;
    }

    /* Lay 4 bit cao de xac dinh loai frame */
    frameType = (uint8_t)(frame[0] & ISOTP_PCI_TYPE_MASK);

    switch (frameType)
    {
        case ISOTP_PCI_SINGLE_FRAME:
            IsoTp_HandleReceivedSingleFrame(frame);
            break;

        case ISOTP_PCI_FIRST_FRAME:
            IsoTp_HandleReceivedFirstFrame(frame);
            break;

        case ISOTP_PCI_CONSECUTIVE_FRAME:
            IsoTp_HandleReceivedConsecutiveFrame(frame);
            break;

        case ISOTP_PCI_FLOW_CONTROL:
            IsoTp_HandleReceivedFlowControl(frame);
            break;

        default:
            /* Loai frame khong hop le - bo qua */
            break;
    }
}

/**
 * @brief  Ham chu ky xu ly gui CF va kiem tra timeout. Xem mo ta trong isotp.h.
 */
void IsoTp_MainFunction(uint32_t currentTimeMs)
{
    /* Neu dang trong che do gui CF thi gui frame tiep theo */
    if (isoTpTxState == ISOTP_TX_SENDING_CF)
    {
        if (isoTpTxIndex < isoTpTxLength)
        {
            IsoTp_SendConsecutiveFrame();
        }

        /* Da gui het toan bo du lieu */
        if (isoTpTxIndex >= isoTpTxLength)
        {
            isoTpTxState = ISOTP_TX_IDLE;
        }
    }

    /* Kiem tra timeout cho Flow Control (N_Bs) */
    if (isoTpTxState == ISOTP_TX_WAIT_FC)
    {
        if ((currentTimeMs - isoTpTxTimer) > ISOTP_TIMEOUT_N_BS_MS)
        {
            isoTpTxState = ISOTP_TX_IDLE;  /* Huy phien do het gio */
        }
    }

    /* Kiem tra timeout cho Consecutive Frame (N_Cr) */
    if (isoTpRxState == ISOTP_RX_RECEIVING_CF)
    {
        if ((currentTimeMs - isoTpRxTimer) > ISOTP_TIMEOUT_N_CR_MS)
        {
            isoTpRxState = ISOTP_RX_IDLE;  /* Huy phien do het gio */
        }
    }
}
