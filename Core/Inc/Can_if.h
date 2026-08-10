#ifndef INC_CAN_IF_H_
#define INC_CAN_IF_H_

#include "main.h"
#include <stdio.h>

void CAN_IF_Init(void);
HAL_StatusTypeDef CAN_IF_Transmit(uint16_t stdId, uint8_t *pData, uint8_t len);
void CAN_IF_HandleTxError(void);
void CAN_IF_ProcessReceivedFrame(void);
void CAN_IF_ProcessRxInterrupt(void);
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan);
#endif /* INC_CAN_IF_H_ */
