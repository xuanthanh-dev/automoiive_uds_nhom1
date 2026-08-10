/**
 * @file    CAN_if.c
 * @brief   Trien khai ECU Abstraction Layer cho CAN.
 *
 * @note    Nguyen tac single responsibility:
 *          - CHI gui/nhan frame, BAO ket qua bang ma tra ve.
 *          - KHONG in log (viec cua tang tren).
 *          - KHONG bat den LED (viec cua tang ung dung).
 *          - KHONG goi Error_Handler (tang tren quyet dinh).
 *          - KHONG biet tang tren la ai (dung callback dang ky).
 */

#include "canif.h"
#include "main.h"       /* Kieu HAL CAN - can cho phan cung */

/*----------------------------------------------------------------------------
 * Bien module
 *--------------------------------------------------------------------------*/
extern CAN_HandleTypeDef hcan;

static CAN_TxHeaderTypeDef  TxHeader;
static uint32_t             TxMailbox;
static CAN_RxHeaderTypeDef  RxHeader;
static uint8_t              RxData[8];
static CAN_FilterTypeDef    FilterConfig;

static CanIf_RxCallbackType canIfRxCallback  = 0;
static uint8_t              canIfInitialised = 0u;

/*----------------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------------*/

/**
 * @brief  Khoi tao phan cung CAN. Xem mo ta trong CAN_if.h.
 */
CanIf_StatusType CAN_IF_Init(void)
{
    CanIf_StatusType status;

    /*
     * KHONG cau hinh lai CAN o day.
     * Toc do, BS1, BS2, prescaler do CubeMX quan ly qua MX_CAN_Init().
     * Muon doi toc do thi sua trong CubeMX, khong sua o file nay.
     * CanIf chi lo phan van hanh: bo loc, khoi chay, bat ngat.
     */
    {
        FilterConfig.FilterActivation     = CAN_FILTER_ENABLE;
        FilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO1;
        FilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;
        FilterConfig.FilterScale          = CAN_FILTERSCALE_32BIT;
        FilterConfig.FilterIdHigh         = (0x000u << 5);
        FilterConfig.FilterIdLow          = 0u;
        FilterConfig.FilterMaskIdHigh     = (0x000u << 5);
        FilterConfig.FilterMaskIdLow      = 0u;
        FilterConfig.FilterBank           = 0u;

        if (HAL_CAN_ConfigFilter(&hcan, &FilterConfig) != HAL_OK)
        {
            status = CANIF_ERROR_INIT;
        }
        else if (HAL_CAN_Start(&hcan) != HAL_OK)
        {
            status = CANIF_ERROR_INIT;
        }
        else if (HAL_CAN_ActivateNotification(&hcan,
                     CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
        {
            status = CANIF_ERROR_INIT;
        }
        else
        {
            canIfInitialised = 1u;
            status = CANIF_OK;
        }
    }

    return status;
}

/**
 * @brief  Gui mot CAN frame. Xem mo ta trong CAN_if.h.
 */
CanIf_StatusType CAN_IF_Transmit(uint16_t stdId, const uint8_t *pData, uint8_t len)
{
    CanIf_StatusType status;

    if (canIfInitialised == 0u)
    {
        status = CANIF_ERROR_STATE;
    }
    else if ((pData == 0) || (len == 0u) || (len > 8u))
    {
        status = CANIF_ERROR_PARAM;
    }
    else
    {
        TxHeader.StdId = stdId;
        TxHeader.IDE   = CAN_ID_STD;
        TxHeader.RTR   = CAN_RTR_DATA;
        TxHeader.DLC   = len;

        if (HAL_CAN_AddTxMessage(&hcan, &TxHeader,
                                 (uint8_t *)pData, &TxMailbox) != HAL_OK)
        {
            /* Bao loi len tang tren, KHONG printf */
            status = CANIF_ERROR_BUSY;
        }
        else
        {
            status = CANIF_OK;
        }
    }

    return status;
}

/**
 * @brief  Dang ky ham nhan frame. Xem mo ta trong CAN_if.h.
 */
CanIf_StatusType CAN_IF_RegisterRxCallback(CanIf_RxCallbackType callback)
{
    CanIf_StatusType status;

    if (callback == 0)
    {
        status = CANIF_ERROR_PARAM;
    }
    else
    {
        canIfRxCallback = callback;
        status = CANIF_OK;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * Ngat nhan frame
 *--------------------------------------------------------------------------*/

/**
 * @brief  Ngat bao co frame trong FIFO1.
 * @details CHI lay frame ra va chuyen len tang tren qua callback.
 *          KHONG in log, KHONG bat den - do la viec cua tang tren.
 */

volatile uint32_t rxCount = 0;
volatile uint32_t rxGetOk = 0;      /* thêm */
volatile uint32_t rxCbCalled = 0;   /* thêm */

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcanHandle)
{
    rxCount++;

    if (HAL_CAN_GetRxMessage(hcanHandle, CAN_RX_FIFO1,
                             &RxHeader, RxData) == HAL_OK)
    {
        rxGetOk++;                  /* thêm */

        if (canIfRxCallback != 0)
        {
            rxCbCalled++;           /* thêm */
            canIfRxCallback(RxHeader.StdId, RxData,
                            (uint8_t)RxHeader.DLC, HAL_GetTick());
        }
    }
}
