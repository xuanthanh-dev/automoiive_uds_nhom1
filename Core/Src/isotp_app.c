/**
 * @file    isotp_app.c
 * @brief   Lop keo toi thieu tich hop ISO-TP (tuan 3-4, chua co UDS).
 * @details Nhan ban tin qua ISO-TP, in ra UART, gui nguoc lai (echo).
 *
 * @note    NGUYEN TAC 1 - Khong in log trong ngat.
 *          Ba ham duoi day CO THE duoc goi tu ngat CAN:
 *              IsoTpApp_OnCanRx
 *              IsoTpApp_CanSendWrapper   (khi ISO-TP gui Flow Control)
 *              IsoTpApp_OnMessageReceived (khi ghep xong ban tin)
 *          Chung CHI cat du lieu vao hang doi. Viec in do IsoTpApp_MainFunction
 *          lam, vi ham do chay trong vong lap chinh chu khong trong ngat.
 *
 * @note    NGUYEN TAC 2 - Khong phu thuoc phan cung.
 *          File nay khong goi ham nao cua HAL. Moc thoi gian do CanIf dua
 *          sang, viec gui frame do CanIf lam, viec in do uart_log lam.
 *          Nho vay kiem thu duoc tren may tinh ma khong can gia lap HAL.
 */

#include "isotp_app.h"
#include "isotp.h"
#include "canif.h"     /* CAN_IF_Transmit, dang ky ham nhan */
#include "uart_log.h"   /* uartlog */
#include <stdio.h>      /* sprintf */

/*----------------------------------------------------------------------------
 * Cau hinh
 *--------------------------------------------------------------------------*/
#define ISOTP_APP_ECHO_ENABLE   (0)    /* 1 = gui nguoc ban tin nhan duoc  */
#define ISOTP_APP_LOG_FRAMES    (1)    /* 1 = in tung frame gui di          */
#define ISOTP_APP_LOG_SIZE      (200)  /* Kich thuoc bo dem chuoi log       */
#define ISOTP_APP_TXLOG_DEPTH   (12u)  /* So frame cho log toi da           */
#define ISOTP_APP_MSG_MAX       (64u)  /* Ban tin dai nhat                  */

/*----------------------------------------------------------------------------
 * Hang doi log frame gui di
 * Ngat GHI vao, MainFunction DOC ra. Danh dau volatile vi hai ben khac nhau.
 *--------------------------------------------------------------------------*/
typedef struct
{
    uint8_t data[8];
    uint8_t dlc;
} IsoTpApp_TxLogType;

static volatile IsoTpApp_TxLogType isoTpAppTxLog[ISOTP_APP_TXLOG_DEPTH];
static volatile uint8_t            isoTpAppTxLogHead = 0u;  /* MainFunction doc */
static volatile uint8_t            isoTpAppTxLogTail = 0u;  /* ngat ghi          */

/*----------------------------------------------------------------------------
 * Cho log ban tin da ghep
 *--------------------------------------------------------------------------*/
static uint8_t           isoTpAppRxLog[ISOTP_APP_MSG_MAX];
static volatile uint16_t isoTpAppRxLogLength  = 0u;
static volatile uint8_t  isoTpAppRxLogPending = 0u;

/*----------------------------------------------------------------------------
 * Cho gui echo
 *--------------------------------------------------------------------------*/
static uint8_t           isoTpAppEchoBuffer[ISOTP_APP_MSG_MAX];
static volatile uint16_t isoTpAppEchoLength  = 0u;
static volatile uint8_t  isoTpAppEchoPending = 0u;
static volatile uint8_t  isoTpAppEchoTooLong = 0u;

/*----------------------------------------------------------------------------
 * Trang thai module
 *--------------------------------------------------------------------------*/
static uint8_t           isoTpAppInitialised   = 0u;
static uint32_t          isoTpAppReceivedCount = 0u;
static volatile uint8_t  isoTpAppSendFailed    = 0u;
static char              isoTpAppLogBuffer[ISOTP_APP_LOG_SIZE];

/*============================================================================
 * Ham phu tro - chi chay trong MainFunction, duoc phep cham
 *==========================================================================*/

/**
 * @brief  Ghep cac byte thanh chuoi hex vao bo dem log.
 * @param  offset  Vi tri bat dau ghi.
 * @param  data    Du lieu can in.
 * @param  length  So byte.
 * @return Vi tri tiep theo trong bo dem.
 */
static int IsoTpApp_AppendHex(int offset, const uint8_t *data, uint16_t length)
{
    uint16_t index;
    int      position;

    position = offset;

    for (index = 0u; index < length; index++)
    {
        if (position < (ISOTP_APP_LOG_SIZE - 5))
        {
            position += sprintf(&isoTpAppLogBuffer[position], " %02X",
                                (unsigned)data[index]);
        }
        else
        {
            position += sprintf(&isoTpAppLogBuffer[position], "...");
            break;
        }
    }

    return position;
}

/**
 * @brief  Xa hang doi log frame gui di ra UART.
 * @note   Chi goi tu MainFunction. Khong bao gio goi tu ngat.
 */
static void IsoTpApp_FlushTxLog(void)
{
#if (ISOTP_APP_LOG_FRAMES == 1)
    uint8_t frame[8];
    uint8_t dlc;
    uint8_t index;
    int     position;

    while (isoTpAppTxLogHead != isoTpAppTxLogTail)
    {
        /* Sao ra bien cuc bo truoc, de ngat co the ghi tiep vao hang doi */
        dlc = isoTpAppTxLog[isoTpAppTxLogHead].dlc;
        for (index = 0u; index < 8u; index++)
        {
            frame[index] = isoTpAppTxLog[isoTpAppTxLogHead].data[index];
        }
        isoTpAppTxLogHead =
            (uint8_t)((isoTpAppTxLogHead + 1u) % ISOTP_APP_TXLOG_DEPTH);

        position = sprintf(isoTpAppLogBuffer, "  [TX 0x%03X]",
                           (unsigned)ISOTP_APP_ID_TX);
        position = IsoTpApp_AppendHex(position, frame, (uint16_t)dlc);
        (void)sprintf(&isoTpAppLogBuffer[position], "\r\n");
        uartlog(isoTpAppLogBuffer);
    }
#else
    /* Tat log frame */
#endif
}

/**
 * @brief  In ban tin da ghep va cac thong bao khac ra UART.
 * @note   Chi goi tu MainFunction.
 */
static void IsoTpApp_FlushMessageLog(void)
{
    int position;

    if (isoTpAppRxLogPending != 0u)
    {
        position = sprintf(isoTpAppLogBuffer, "\r\n>>> GHEP XONG %u byte:",
                           (unsigned)isoTpAppRxLogLength);
        position = IsoTpApp_AppendHex(position, isoTpAppRxLog,
                                      isoTpAppRxLogLength);
        (void)sprintf(&isoTpAppLogBuffer[position], "\r\n");
        uartlog(isoTpAppLogBuffer);

        isoTpAppRxLogPending = 0u;
    }
    else
    {
        /* Khong co ban tin moi */
    }

    if (isoTpAppEchoTooLong != 0u)
    {
        uartlog(">>> Ban tin qua dai, khong echo\r\n");
        isoTpAppEchoTooLong = 0u;
    }
    else
    {
        /* Khong co canh bao */
    }

    if (isoTpAppSendFailed != 0u)
    {
        uartlog("  [LOI] Gui CAN that bai\r\n");
        isoTpAppSendFailed = 0u;
    }
    else
    {
        /* Khong co loi gui */
    }
}

/*============================================================================
 * Ham callback - CO THE chay trong ngat, tuyet doi khong in log
 *==========================================================================*/

/**
 * @brief  Gui frame xuong CanIf kem CAN ID cua minh.
 * @details ISO-TP goi ham nay. Khi ISO-TP dang xu ly Flow Control thi
 *          ham nay chay TRONG NGAT, nen chi duoc lam viec nhanh.
 * @param  frame  Con tro 8 byte cua CAN frame.
 * @param  dlc    So byte hop le.
 * @return 0 neu gui thanh cong, 1 neu that bai.
 */
static uint8_t IsoTpApp_CanSendWrapper(const uint8_t *frame, uint8_t dlc)
{
    uint8_t result;
    uint8_t index;
    uint8_t nextTail;

    if (frame == 0)
    {
        result = 1u;
    }
    else if (CAN_IF_Transmit(ISOTP_APP_ID_TX, frame, dlc) != CANIF_OK)
    {
        /* Chi dat co, MainFunction se in thong bao */
        isoTpAppSendFailed = 1u;
        result = 1u;
    }
    else
    {
        /* Cat frame vao hang doi de MainFunction in sau */
        nextTail = (uint8_t)((isoTpAppTxLogTail + 1u) % ISOTP_APP_TXLOG_DEPTH);

        if (nextTail != isoTpAppTxLogHead)
        {
            for (index = 0u; index < 8u; index++)
            {
                isoTpAppTxLog[isoTpAppTxLogTail].data[index] =
                    (index < dlc) ? frame[index] : 0u;
            }
            isoTpAppTxLog[isoTpAppTxLogTail].dlc = dlc;
            isoTpAppTxLogTail = nextTail;
        }
        else
        {
            /* Hang doi day - bo qua log, KHONG anh huong viec gui */
        }

        result = 0u;
    }

    return result;
}

/**
 * @brief  Nhan ban tin da ghep day du tu ISO-TP.
 * @details CO THE chay trong ngat. Chi cat du lieu, khong in, khong gui.
 * @param  message  Du lieu da ghep.
 * @param  length   So byte.
 */
static void IsoTpApp_OnMessageReceived(const uint8_t *message, uint16_t length)
{
    uint16_t index;

    if (message == 0)
    {
        /* Khong co du lieu */
    }
    else if (length > ISOTP_APP_MSG_MAX)
    {
        isoTpAppEchoTooLong = 1u;
    }
    else
    {
        isoTpAppReceivedCount++;

        /* Cat de MainFunction in */
        for (index = 0u; index < length; index++)
        {
            isoTpAppRxLog[index] = message[index];
        }
        isoTpAppRxLogLength  = length;
        isoTpAppRxLogPending = 1u;

#if (ISOTP_APP_ECHO_ENABLE == 1)
        /* Cat de MainFunction gui nguoc lai.
           KHONG goi IsoTp_Send o day: ISO-TP dang xu ly do, goi long vao
           chinh no se lam hong trang thai. */
        for (index = 0u; index < length; index++)
        {
            isoTpAppEchoBuffer[index] = message[index];
        }
        isoTpAppEchoLength  = length;
        isoTpAppEchoPending = 1u;
#endif
    }
}

/**
 * @brief  Nhan frame tu CanIf. Xem mo ta trong isotp_app.h.
 * @details CHAY TRONG NGAT. Chi loc ID va day len ISO-TP.
 */
void IsoTpApp_OnCanRx(uint32_t       id,
                      const uint8_t *data,
                      uint8_t        dlc,
                      uint32_t       timestampMs)
{
    if (isoTpAppInitialised == 0u)
    {
        /* Chua khoi tao - bo qua */
    }
    else if (data == 0)
    {
        /* Du lieu rong - bo qua */
    }
    else if (id != ISOTP_APP_ID_RX)
    {
        /* Khong phai ID cua minh - bo qua */
    }
    else
    {
        /* Moc thoi gian do CanIf dua sang, khong tu goi ham phan cung */
        (void)IsoTp_OnCanFrame(data, dlc, timestampMs);
    }
}

/*============================================================================
 * Giao dien cong khai
 *==========================================================================*/

/**
 * @brief  Khoi tao. Xem mo ta trong isotp_app.h.
 */
IsoTpApp_StatusType IsoTpApp_Init(void)
{
    IsoTpApp_StatusType status;
    IsoTp_StatusType    isoTpStatus;

    isoTpStatus = IsoTp_Init(IsoTpApp_CanSendWrapper,
                             IsoTpApp_OnMessageReceived);

    if (isoTpStatus != ISOTP_OK)
    {
        status = ISOTP_APP_ERROR_STATE;
    }
    else if (CAN_IF_RegisterRxCallback(IsoTpApp_OnCanRx) != CANIF_OK)
    {
        status = ISOTP_APP_ERROR_STATE;
    }
    else
    {
        isoTpAppReceivedCount = 0u;
        isoTpAppTxLogHead     = 0u;
        isoTpAppTxLogTail     = 0u;
        isoTpAppRxLogLength   = 0u;
        isoTpAppRxLogPending  = 0u;
        isoTpAppEchoLength    = 0u;
        isoTpAppEchoPending   = 0u;
        isoTpAppEchoTooLong   = 0u;
        isoTpAppSendFailed    = 0u;
        isoTpAppInitialised   = 1u;

        (void)sprintf(isoTpAppLogBuffer,
                      "\r\n=== ISO-TP san sang ===\r\n"
                      "Nhan ID 0x%03X, gui ID 0x%03X\r\n\r\n",
                      (unsigned)ISOTP_APP_ID_RX, (unsigned)ISOTP_APP_ID_TX);
        uartlog(isoTpAppLogBuffer);

        status = ISOTP_APP_OK;
    }

    return status;
}

/**
 * @brief  Ham chu ky. Xem mo ta trong isotp_app.h.
 * @details Chay trong vong lap chinh, KHONG trong ngat, nen duoc phep in log.
 */
IsoTpApp_StatusType IsoTpApp_MainFunction(uint32_t currentTimeMs)
{
    IsoTpApp_StatusType status;
    uint16_t            echoLength;

    if (isoTpAppInitialised == 0u)
    {
        status = ISOTP_APP_ERROR_STATE;
    }
    else
    {
        /* ISO-TP gui frame noi tiep con lai, kiem tra het gio */
        (void)IsoTp_MainFunction(currentTimeMs);

        /* Gui echo neu dang cho */
        if (isoTpAppEchoPending != 0u)
        {
            isoTpAppEchoPending = 0u;
            echoLength = isoTpAppEchoLength;
            (void)IsoTp_Send(isoTpAppEchoBuffer, echoLength, currentTimeMs);
        }
        else
        {
            /* Khong co gi de echo */
        }

        /* In log da cat tu ngat - lam sau cung */
        IsoTpApp_FlushMessageLog();
        IsoTpApp_FlushTxLog();

        status = ISOTP_APP_OK;
    }

    return status;
}

/**
 * @brief  Chu dong gui ban tin. Xem mo ta trong isotp_app.h.
 */
IsoTpApp_StatusType IsoTpApp_SendMessage(const uint8_t *message,
                                         uint16_t       length,
                                         uint32_t       currentTimeMs)
{
    IsoTpApp_StatusType status;
    IsoTp_StatusType    isoTpStatus;

    if (isoTpAppInitialised == 0u)
    {
        status = ISOTP_APP_ERROR_STATE;
    }
    else if (message == 0)
    {
        status = ISOTP_APP_ERROR_NULL;
    }
    else
    {
        (void)sprintf(isoTpAppLogBuffer, "\r\n<<< GUI %u byte qua ISO-TP\r\n",
                      (unsigned)length);
        uartlog(isoTpAppLogBuffer);

        isoTpStatus = IsoTp_Send(message, length, currentTimeMs);

        if (isoTpStatus != ISOTP_OK)
        {
            uartlog("<<< LOI: khong gui duoc\r\n");
            status = ISOTP_APP_ERROR_STATE;
        }
        else
        {
            status = ISOTP_APP_OK;
        }
    }

    return status;
}

/**
 * @brief  So ban tin da nhan. Xem mo ta trong isotp_app.h.
 */
uint32_t IsoTpApp_GetReceivedCount(void)
{
    return isoTpAppReceivedCount;
}
