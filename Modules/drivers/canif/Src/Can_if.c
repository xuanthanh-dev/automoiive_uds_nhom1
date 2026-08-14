#include "Can_if.h"
extern CAN_HandleTypeDef hcan;
/* Private Variables cho CAN */
static CAN_TxHeaderTypeDef TxHeader;
static uint32_t TxMailbox;
static CAN_RxHeaderTypeDef RxHeader;
static uint8_t RxData[8];
static CAN_FilterTypeDef FilterConfig;

/**
 * @brief CAN Initialization Function
 * @param None
 * @retval None
 */

void CAN_IF_Init(void) {
	/* USER CODE BEGIN CAN_Init 0 */

	/* USER CODE END CAN_Init 0 */

	/* USER CODE BEGIN CAN_Init 1 */

	/* USER CODE END CAN_Init 1 */
	hcan.Instance = CAN1;
	hcan.Init.Prescaler = 18;
	hcan.Init.Mode = CAN_MODE_NORMAL;
	hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
	hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
	hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
	hcan.Init.TimeTriggeredMode = DISABLE;
	hcan.Init.AutoBusOff = ENABLE;
	hcan.Init.AutoWakeUp = DISABLE;
	hcan.Init.AutoRetransmission = ENABLE;
	hcan.Init.ReceiveFifoLocked = DISABLE;
	hcan.Init.TransmitFifoPriority = DISABLE;
	HAL_CAN_Init(&hcan);

	/* USER CODE BEGIN CAN_Init 2 */
	FilterConfig.FilterActivation = CAN_FILTER_ENABLE;
	FilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO1; /* Sửa tên enum chuẩn HAL: CAN_FILTER_FIFO1 */
	FilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	FilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	FilterConfig.FilterIdHigh = (0x000 << 5);
	FilterConfig.FilterIdLow = 0;
	FilterConfig.FilterMaskIdHigh = (0x000 << 5);
	FilterConfig.FilterMaskIdLow = 0;
	FilterConfig.FilterBank = 0;
	HAL_CAN_ConfigFilter(&hcan, &FilterConfig);
	/* USER CODE END CAN_Init 2 */
	HAL_CAN_Start(&hcan);
	HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
}

/**
 * @brief Hàm truyền dữ liệu CAN
 */
CAN_StatusTypeDef CAN_IF_Transmit(uint16_t stdId, uint8_t *pData, uint8_t len) {
	CAN_StatusTypeDef errorCode = OK;
	HAL_StatusTypeDef halStatus = HAL_OK;

	if (stdId > 0x7FFU) {
		errorCode = ERROR_INVALID_ID;
	}
	if (pData == NULL) {
		errorCode = ERROR_NULL_POINTER;
	}
	if (len > 8) {
		errorCode = ERROR_INVALID_LENGTH;
	}

	if (OK == errorCode) {
		TxHeader.StdId = stdId;
		TxHeader.IDE = CAN_ID_STD;
		TxHeader.RTR = CAN_RTR_DATA;
		TxHeader.DLC = len;

		halStatus = HAL_CAN_AddTxMessage(&hcan, &TxHeader, pData, &TxMailbox);
		if (halStatus != HAL_OK) {
			errorCode = ERROR_TRANSMIT;
		}
	}

	return errorCode;
}
/**
 * @brief Hàm xử lý lỗi khi truyền dữ liệu CAN
 */
uint32_t CAN_IF_HandleTxError(void) {
	uint32_t error = HAL_CAN_GetError(&hcan);
	return error;
}


/**
 * @brief Lay frame CAN da nhan
 */
CAN_StatusTypeDef CAN_IF_GetReceivedFrame(uint32_t *stdId, uint8_t *data,uint8_t *dlc) {
	CAN_StatusTypeDef errorCode = OK;
	if (stdId == NULL) {
		errorCode = ERROR_NULL_POINTER;
	}

	if (data == NULL) {
		errorCode = ERROR_NULL_POINTER;
	}

	if (dlc == NULL) {
		errorCode = ERROR_NULL_POINTER;
	}

	if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO1) == 0U) {
		errorCode = ERROR_INVALID_LENGTH;
	}


	*stdId = RxHeader.StdId;
	*dlc = (uint8_t) RxHeader.DLC;

	for (uint8_t i = 0U; i < 8U; i++) {
		data[i] = RxData[i];
	}

	return errorCode;
}
/**
 * @brief Hàm xử lý ngắt nhận dữ liệu CAN
 */
CAN_StatusTypeDef CAN_IF_ProcessRxInterrupt(void)
{
	CAN_StatusTypeDef errorCode = OK;
	HAL_StatusTypeDef halStatus = HAL_OK;
	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	if (HAL_CAN_GetRxMessage(&hcan,
	CAN_RX_FIFO1, &RxHeader, RxData) != HAL_OK) {
		errorCode = ERROR_TRANSMIT;
	}
	return errorCode;
}
/**
 * @brief Callback nhận ngắt CAN FIFO 1
 */

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	if (hcan->Instance == CAN1) {
		CAN_IF_ProcessRxInterrupt();
	}
}
