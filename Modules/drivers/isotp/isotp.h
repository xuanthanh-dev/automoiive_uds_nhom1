/**
 * @file isotp.h
 * @brief Minimal passive ISO-TP segmentation and reassembly interface.
 *
 * @details The module only builds and parses SF, FF, CF and basic FC frames.
 *          It never accesses CanIf, reads time, schedules transmission or
 *          processes UDS. Those responsibilities belong to main.c.
 */

#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

#define ISOTP_MAX_PAYLOAD_LENGTH        (64U)
#define ISOTP_CAN_FRAME_LENGTH          (8U)
#define ISOTP_SINGLE_FRAME_PAYLOAD      (7U)
#define ISOTP_FIRST_FRAME_PAYLOAD       (6U)
#define ISOTP_CONSECUTIVE_FRAME_PAYLOAD (7U)
#define ISOTP_MAX_STANDARD_CAN_ID       (0x07FFU)
#define ISOTP_BASIC_BLOCK_SIZE          (0U)
#define ISOTP_BASIC_ST_MIN_MS           (10U)
#define ISOTP_PADDING_VALUE             (0x00U)

/** Return codes owned by the ISO-TP module. */
typedef enum
{
    ISOTP_E_OK = 0,
    ISOTP_E_NULL_PTR,
    ISOTP_E_INVALID_LENGTH,
    ISOTP_E_INVALID_CAN_ID,
    ISOTP_E_INVALID_DLC,
    ISOTP_E_INVALID_CONFIG,
    ISOTP_E_NOT_INITIALIZED,
    ISOTP_E_BUSY,
    ISOTP_E_INVALID_STATE,
    ISOTP_E_INVALID_FRAME_TYPE,
    ISOTP_E_INVALID_FRAME_FORMAT,
    ISOTP_E_UNEXPECTED_FRAME,
    ISOTP_E_SEQUENCE_MISMATCH,
    ISOTP_E_BUFFER_OVERFLOW,
    ISOTP_E_NO_FRAME_AVAILABLE,
    ISOTP_E_NO_PAYLOAD_AVAILABLE,
    ISOTP_E_SMALL_BUFFER,
    ISOTP_E_UNSUPPORTED_FLOW_CONTROL
} IsoTp_ReturnType;

/** Segmentation state. */
typedef enum
{
    ISOTP_TX_STATE_UNINITIALIZED = 0,
    ISOTP_TX_STATE_IDLE,
    ISOTP_TX_STATE_SINGLE_FRAME_READY,
    ISOTP_TX_STATE_FIRST_FRAME_READY,
    ISOTP_TX_STATE_WAIT_FLOW_CONTROL,
    ISOTP_TX_STATE_CONSECUTIVE_FRAME_READY
} IsoTp_TxStateType;

/** Reassembly state. */
typedef enum
{
    ISOTP_RX_STATE_UNINITIALIZED = 0,
    ISOTP_RX_STATE_IDLE,
    ISOTP_RX_STATE_WAIT_CONSECUTIVE_FRAME,
    ISOTP_RX_STATE_PAYLOAD_READY
} IsoTp_RxStateType;

/** Event returned to main.c after one frame is parsed. */
typedef enum
{
    ISOTP_RX_EVENT_NONE = 0,
    ISOTP_RX_EVENT_PAYLOAD_COMPLETE,
    ISOTP_RX_EVENT_FLOW_CONTROL_REQUIRED,
    ISOTP_RX_EVENT_FLOW_CONTROL_RECEIVED,
    ISOTP_RX_EVENT_CONSECUTIVE_FRAME_RECEIVED
} IsoTp_RxEventType;

/** Direction selected by IsoTp_Reset(). */
typedef enum
{
    ISOTP_RESET_TRANSMIT = 0,
    ISOTP_RESET_RECEIVE,
    ISOTP_RESET_BOTH
} IsoTp_ResetDirectionType;

/** Classical CAN frame exchanged with main.c. */
typedef struct
{
    uint16_t identifier;
    uint8_t dataLength;
    uint8_t data[ISOTP_CAN_FRAME_LENGTH];
} IsoTp_CanFrameType;

/** Connection addressing. */
typedef struct
{
    uint16_t transmitCanIdentifier;
    uint16_t receiveCanIdentifier;
} IsoTp_ConfigType;

/** Persistent segmentation storage. */
typedef struct
{
    IsoTp_TxStateType state;
    uint8_t buffer[ISOTP_MAX_PAYLOAD_LENGTH];
    uint16_t totalLength;
    uint16_t transmittedLength;
    uint8_t nextSequenceNumber;
    uint8_t separationTimeMs;
} IsoTp_TxContextType;

/** Persistent reassembly storage. */
typedef struct
{
    IsoTp_RxStateType state;
    uint8_t buffer[ISOTP_MAX_PAYLOAD_LENGTH];
    uint16_t totalLength;
    uint16_t receivedLength;
    uint8_t expectedSequenceNumber;
} IsoTp_RxContextType;

/** Complete caller-owned ISO-TP state. */
typedef struct
{
    IsoTp_ConfigType config;
    IsoTp_TxContextType transmit;
    IsoTp_RxContextType receive;
    uint8_t initialized;
} IsoTp_ContextType;

/** Current passive state read by main.c. */
typedef struct
{
    IsoTp_TxStateType transmitState;
    IsoTp_RxStateType receiveState;
    uint8_t separationTimeMs;
} IsoTp_StatusType;

/**
 * @brief Initializes one passive ISO-TP connection.
 * @param context Caller-owned context.
 * @param config Transmit and receive CAN identifiers.
 * @return ISOTP_E_OK or a pointer/configuration error.
 */
IsoTp_ReturnType IsoTp_Init(IsoTp_ContextType *context,
                            const IsoTp_ConfigType *config);

/**
 * @brief Copies one payload and starts passive segmentation.
 * @param context Initialized context.
 * @param payload Payload to split into CAN frames.
 * @param payloadLength Length from 1 to ISOTP_MAX_PAYLOAD_LENGTH.
 * @return ISOTP_E_OK or an input/state error.
 */
IsoTp_ReturnType IsoTp_StartSegmentation(IsoTp_ContextType *context,
                                         const uint8_t *payload,
                                         uint16_t payloadLength);

/**
 * @brief Builds and consumes the next pending SF, FF or CF.
 * @details main.c must retain the returned frame until CanIf accepts it.
 * @param context Initialized context.
 * @param frame Destination generated CAN frame.
 * @return ISOTP_E_OK or a pointer/state error.
 */
IsoTp_ReturnType IsoTp_GetNextFrame(IsoTp_ContextType *context,
                                    IsoTp_CanFrameType *frame);

/**
 * @brief Parses one SF, FF, CF or basic FC frame.
 * @details When an FF is received, flowControlFrame receives the fixed
 *          `30 00 0A` response for main.c to transmit.
 * @param context Initialized context.
 * @param frame Received CAN frame.
 * @param event Destination parsing event.
 * @param flowControlFrame Destination FC frame when required.
 * @return ISOTP_E_OK or a frame/state error.
 */
IsoTp_ReturnType IsoTp_ProcessFrame(IsoTp_ContextType *context,
                                    const IsoTp_CanFrameType *frame,
                                    IsoTp_RxEventType *event,
                                    IsoTp_CanFrameType *flowControlFrame);

/**
 * @brief Copies and consumes one completely reassembled payload.
 * @param context Initialized context.
 * @param payload Destination buffer.
 * @param payloadCapacity Destination capacity.
 * @param payloadLength Destination completed length.
 * @return ISOTP_E_OK or an input/state/capacity error.
 */
IsoTp_ReturnType IsoTp_ReadPayload(IsoTp_ContextType *context,
                                   uint8_t *payload,
                                   uint16_t payloadCapacity,
                                   uint16_t *payloadLength);

/**
 * @brief Reads passive segmentation and reassembly status.
 * @param context Initialized context.
 * @param status Destination status.
 * @return ISOTP_E_OK or a pointer/state error.
 */
IsoTp_ReturnType IsoTp_GetStatus(const IsoTp_ContextType *context,
                                 IsoTp_StatusType *status);

/**
 * @brief Resets one or both passive directions.
 * @param context Initialized context.
 * @param direction Direction to reset.
 * @return ISOTP_E_OK or an input/state error.
 */
IsoTp_ReturnType IsoTp_Reset(IsoTp_ContextType *context,
                             IsoTp_ResetDirectionType direction);

#endif /* ISOTP_H */
