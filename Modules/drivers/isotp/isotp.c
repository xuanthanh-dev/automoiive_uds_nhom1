/**
 * @file isotp.c
 * @brief Minimal passive ISO-TP segmentation and reassembly.
 */

#include "isotp.h"

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

/**
 * @brief Initializes a padded Classical CAN frame.
 * @param frame Destination frame.
 * @param identifier Standard CAN identifier.
 * @return ISOTP_E_OK or an input error.
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

/** @brief Builds and consumes the next pending data frame. */
IsoTp_ReturnType IsoTp_GetNextFrame(IsoTp_ContextType *context,
                                    IsoTp_CanFrameType *frame)
{
    IsoTp_ReturnType result;
    uint16_t remainingLength;
    uint16_t payloadLength;

    if ((context == 0) || (frame == 0))
    {
        result = ISOTP_E_NULL_PTR;
    }
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
                context->transmit.transmittedLength = (uint16_t)(
                    context->transmit.transmittedLength + payloadLength);
                context->transmit.nextSequenceNumber = (uint8_t)(
                    (context->transmit.nextSequenceNumber + 1U) &
                    ISOTP_PCI_VALUE_MASK);
                if (context->transmit.transmittedLength >=
                    context->transmit.totalLength)
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

        if (frameType == ISOTP_PCI_SINGLE)
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
