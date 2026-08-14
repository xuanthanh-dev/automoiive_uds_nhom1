/**
 * @file    isotp_app.c
 * @brief   Lop keo giua ISO-TP va tang giao tiep CAN.
 *
 * @note    NGUYEN TAC MOT - ngat lam cang it viec cang tot.
 *          CAN_IF_OnFrameReceived chay trong ngat. No chi chep khung vao
 *          hang doi roi thoat. Moi viec xu ly, ke ca khung tra loi ma ISO-TP
 *          co the phai gui, deu dien ra sau do trong ham chu ky. Khong tach
 *          nhu vay thi mot khung nhan duoc co the keo theo viec gui khung
 *          ngay trong ngat, va mot dong log viet o do se chan du lau de mat
 *          cac khung phia sau.
 *
 * @note    NGUYEN TAC HAI - khong sua tang giao tiep CAN.
 *          Chieu gui dung CAN_IF_Transmit san co. Chieu nhan dung
 *          CAN_IF_OnFrameReceived - ham yeu khai bao trong tang giao tiep
 *          CAN, file nay dinh nghia lai de lay quyen. Nho vay tang giao tiep
 *          CAN khong bao gio can biet tang nao dang nam tren no.
 *
 * @note    Hai hang doi duoi day do ngat ghi va vong lap chinh doc. Chi so
 *          dau va duoi deu la mot byte, moi chi so chi mot ben ghi, nen tren
 *          kien truc nay khong can khoa. Neu chi so vuot qua mot byte thi
 *          phai them doan chan ngat.
 */

#include "isotp_app.h"
#include "isotp.h"
#include "Can_if.h"     /* CAN_IF_Transmit, CAN_StatusTypeDef */
#include "uart_log.h"
#include <stdio.h>      /* snprintf */

/*----------------------------------------------------------------------------
 * Cau hinh luc bien dich
 *--------------------------------------------------------------------------*/

/*
 * Gui nguoc lai moi ban tin nhan duoc, de chung minh ca hai chieu deu chay.
 *
 * QUAN TRONG: chi duoc bat o MOT board. Neu ca hai cung bat, ban tin se doi
 * qua doi lai khong dut. Board chu dong gui phai dat gia tri nay bang khong,
 * khai bao trong muc bien tien xu ly cua project.
 */
#ifndef ISOTP_APP_ECHO_ENABLE
#define ISOTP_APP_ECHO_ENABLE   (1)
#endif

/** In tung khung gui di. Dat bang khong cho ban chay nhanh va it log. */
#ifndef ISOTP_APP_LOG_FRAMES
#define ISOTP_APP_LOG_FRAMES    (1)
#endif

/** Kich thuoc bo dem ghep mot dong log. */
#define ISOTP_APP_LOG_SIZE      (200)

/** So khung gui di duoc phep cho in. */
#define ISOTP_APP_TXLOG_DEPTH   (12u)

/** So khung nhan duoc phep cho xu ly. */
#define ISOTP_APP_RXQUEUE_DEPTH (16u)

/** Ban tin dai nhat lop nay xu ly. */
#define ISOTP_APP_MSG_MAX       (64u)

/*----------------------------------------------------------------------------
 * Hang doi nhan - ngat ghi vao, ham chu ky lay ra
 *--------------------------------------------------------------------------*/
typedef struct {
	uint8_t data[ISOTP_CAN_FRAME_SIZE];
	uint8_t dlc;
	uint32_t timestampMs; /**< Khung den luc nao, giu de tinh han cho */
} IsoTpApp_RxEntryType;

static volatile IsoTpApp_RxEntryType isoTpAppRxQueue[ISOTP_APP_RXQUEUE_DEPTH];
static volatile uint8_t isoTpAppRxHead = 0u; /* vong lap chinh doc */
static volatile uint8_t isoTpAppRxTail = 0u; /* ngat ghi */
static volatile uint32_t isoTpAppRxDropped = 0u;

/*----------------------------------------------------------------------------
 * Hang doi log khung gui - dien o cho gui, lay ra o cho in
 *--------------------------------------------------------------------------*/
typedef struct {
	uint8_t data[ISOTP_CAN_FRAME_SIZE];
	uint8_t dlc;
} IsoTpApp_TxLogType;

static volatile IsoTpApp_TxLogType isoTpAppTxLog[ISOTP_APP_TXLOG_DEPTH];
static volatile uint8_t isoTpAppTxLogHead = 0u;
static volatile uint8_t isoTpAppTxLogTail = 0u;

/*----------------------------------------------------------------------------
 * Ban tin da ghep xong dang cho in
 *--------------------------------------------------------------------------*/
static uint8_t isoTpAppRxLog[ISOTP_APP_MSG_MAX];
static volatile uint16_t isoTpAppRxLogLength = 0u;
static volatile uint8_t isoTpAppRxLogPending = 0u;

/*----------------------------------------------------------------------------
 * Ban tin dang cho gui nguoc lai
 *--------------------------------------------------------------------------*/
static uint8_t isoTpAppEchoBuffer[ISOTP_APP_MSG_MAX];
static volatile uint16_t isoTpAppEchoLength = 0u;
static volatile uint8_t isoTpAppEchoPending = 0u;
static volatile uint8_t isoTpAppEchoTooLong = 0u;

/*----------------------------------------------------------------------------
 * Trang thai module
 *--------------------------------------------------------------------------*/
static uint8_t isoTpAppInitialised = 0u;
static uint32_t isoTpAppReceivedCount = 0u;
static volatile uint8_t isoTpAppSendFailed = 0u;
static char isoTpAppLogBuffer[ISOTP_APP_LOG_SIZE];

/*============================================================================
 * Ham phu tro chi goi tu vong lap chinh, nen duoc phep cham
 *==========================================================================*/

/**
 * @brief  Ghep byte vao bo dem log duoi dang thap luc phan.
 * @param  offset  Bat dau ghi tu dau.
 * @param  data    Du lieu can ghi.
 * @param  length  So byte.
 * @return Vi tri trong ke tiep trong bo dem.
 *
 * @note   Dung snprintf va dung lai truoc khi het bo dem, nen khung dai bi cat
 *         bot chu khong tran ra ngoai.
 */
static int IsoTpApp_AppendHex(int offset, const uint8_t *data, uint16_t length) {
	uint16_t index;
	int position;
	int written;
	int remaining;

	position = offset;

	for (index = 0u; index < length; index++) {
		remaining = ISOTP_APP_LOG_SIZE - position;

		if (remaining > 6) {
			written = snprintf(&isoTpAppLogBuffer[position], (size_t) remaining,
					" %02X", (unsigned) data[index]);
			if (written > 0) {
				position += written;
			} else {
				break;
			}
		} else {
			(void) snprintf(&isoTpAppLogBuffer[position], (size_t) remaining,
					"...");
			position = ISOTP_APP_LOG_SIZE - 1;
			break;
		}
	}

	return position;
}

/**
 * @brief  In cac khung da gui ke tu chu ky truoc.
 */
static void IsoTpApp_FlushTxLog(void) {
#if (ISOTP_APP_LOG_FRAMES == 1)
	uint8_t frame[ISOTP_CAN_FRAME_SIZE];
	uint8_t dlc;
	uint8_t index;
	int position;

	while (isoTpAppTxLogHead != isoTpAppTxLogTail) {
		/* Sao ra truoc de ben ghi van tiep tuc dien duoc vao hang doi */
		dlc = isoTpAppTxLog[isoTpAppTxLogHead].dlc;
		for (index = 0u; index < ISOTP_CAN_FRAME_SIZE; index++) {
			frame[index] = isoTpAppTxLog[isoTpAppTxLogHead].data[index];
		}
		isoTpAppTxLogHead = (uint8_t) ((isoTpAppTxLogHead + 1u)
				% ISOTP_APP_TXLOG_DEPTH);

		position = snprintf(isoTpAppLogBuffer, sizeof(isoTpAppLogBuffer),
				"  [TX 0x%03X]", (unsigned) ISOTP_APP_ID_TX);
		position = IsoTpApp_AppendHex(position, frame, (uint16_t) dlc);
		(void) snprintf(&isoTpAppLogBuffer[position],
				sizeof(isoTpAppLogBuffer) - (size_t) position, "\r\n");
		uartlog(isoTpAppLogBuffer);
	}
#else
    /* Da tat log khung luc bien dich */
#endif
}

/**
 * @brief  In ban tin da ghep xong va cac canh bao dang cho.
 */
static void IsoTpApp_FlushMessageLog(void) {
	int position;

	if (isoTpAppRxLogPending != 0u) {
		position = snprintf(isoTpAppLogBuffer, sizeof(isoTpAppLogBuffer),
				"\r\n>>> GHEP XONG %u byte:", (unsigned) isoTpAppRxLogLength);
		position = IsoTpApp_AppendHex(position, isoTpAppRxLog,
				isoTpAppRxLogLength);
		(void) snprintf(&isoTpAppLogBuffer[position],
				sizeof(isoTpAppLogBuffer) - (size_t) position, "\r\n");
		uartlog(isoTpAppLogBuffer);

		isoTpAppRxLogPending = 0u;
	} else {
		/* Khong co gi moi de bao */
	}

	if (isoTpAppEchoTooLong != 0u) {
		uartlog(">>> Ban tin qua dai, khong gui nguoc lai\r\n");
		isoTpAppEchoTooLong = 0u;
	} else {
		/* Khong co canh bao nao */
	}

	if (isoTpAppSendFailed != 0u) {
		uartlog("  [LOI] Gui CAN that bai\r\n");
		isoTpAppSendFailed = 0u;
	} else {
		/* Khong co loi gui nao */
	}

	if (isoTpAppRxDropped != 0u) {
		(void) snprintf(isoTpAppLogBuffer, sizeof(isoTpAppLogBuffer),
				"  [CANH BAO] Mat %lu khung, hang doi da day\r\n",
				(unsigned long) isoTpAppRxDropped);
		uartlog(isoTpAppLogBuffer);
		isoTpAppRxDropped = 0u;
	} else {
		/* Khong mat khung nao */
	}
}

/**
 * @brief  Dua lan luot cac khung trong hang doi len ISO-TP.
 *
 * @note   Chay trong vong lap chinh, nen ISO-TP tra loi bang khung dieu khien
 *         luong tu day la an toan.
 */
static void IsoTpApp_DrainRxQueue(void) {
	uint8_t frame[ISOTP_CAN_FRAME_SIZE];
	uint8_t dlc;
	uint32_t timestampMs;
	uint8_t index;

	while (isoTpAppRxHead != isoTpAppRxTail) {
		dlc = isoTpAppRxQueue[isoTpAppRxHead].dlc;
		timestampMs = isoTpAppRxQueue[isoTpAppRxHead].timestampMs;
		for (index = 0u; index < ISOTP_CAN_FRAME_SIZE; index++) {
			frame[index] = isoTpAppRxQueue[isoTpAppRxHead].data[index];
		}
		isoTpAppRxHead = (uint8_t) ((isoTpAppRxHead + 1u)
				% ISOTP_APP_RXQUEUE_DEPTH);

		(void) IsoTp_OnCanFrame(frame, dlc, timestampMs);
	}
}

/*============================================================================
 * Cac ham trao cho ISO-TP goi lai
 *==========================================================================*/

/**
 * @brief  Dat mot khung len bus kem ma dinh danh cua lop nay.
 * @param  frame  Tam byte can gui.
 * @param  dlc    So byte hop le.
 * @return Bang khong khi khung duoc nhan, bang mot khi bi tu choi.
 */
static uint8_t IsoTpApp_CanSendWrapper(const uint8_t *frame, uint8_t dlc) {
	uint8_t result;
	uint8_t index;
	uint8_t nextTail;

	if (frame == 0) {
		result = 1u;
	}
	/* Tang giao tiep CAN nhan con tro khong const du chi doc, nen ep kieu
	 o day la an toan. */
	else if (CAN_IF_Transmit(ISOTP_APP_ID_TX, (uint8_t*) frame, dlc) != OK) {
		isoTpAppSendFailed = 1u;
		result = 1u;
	} else {
		nextTail = (uint8_t) ((isoTpAppTxLogTail + 1u) % ISOTP_APP_TXLOG_DEPTH);

		if (nextTail != isoTpAppTxLogHead) {
			for (index = 0u; index < ISOTP_CAN_FRAME_SIZE; index++) {
				isoTpAppTxLog[isoTpAppTxLogTail].data[index] =
						(index < dlc) ? frame[index] : 0u;
			}
			isoTpAppTxLog[isoTpAppTxLogTail].dlc = dlc;
			isoTpAppTxLogTail = nextTail;
		} else {
			/* Hang doi log day. Bo mot dong log thi vo hai, bo chinh khung thi
			 khong, nen viec gui da duoc thuc hien o tren. */
		}

		result = 0u;
	}

	return result;
}

/**
 * @brief  Nhan ban tin ma ISO-TP vua ghep xong.
 * @param  message  Ban tin day du.
 * @param  length   So byte cua no.
 *
 * @note   Chi ghi lai ban tin. Gui nguoc ngay tai day se goi long vao ISO-TP
 *         trong luc no con dang xu ly ban tin nay, lam hong trang thai, nen
 *         viec gui duoc hoan lai cho vong lap chinh.
 */
static void IsoTpApp_OnMessageReceived(const uint8_t *message, uint16_t length) {
	uint16_t index;

	if (message == 0) {
		/* Nothing to record */
	} else if (length > ISOTP_APP_MSG_MAX) {
		isoTpAppEchoTooLong = 1u;
	} else {
		isoTpAppReceivedCount++;

		for (index = 0u; index < length; index++) {
			isoTpAppRxLog[index] = message[index];
		}
		isoTpAppRxLogLength = length;
		isoTpAppRxLogPending = 1u;

#if (ISOTP_APP_ECHO_ENABLE == 1)
		for (index = 0u; index < length; index++) {
			isoTpAppEchoBuffer[index] = message[index];
		}
		isoTpAppEchoLength = length;
		isoTpAppEchoPending = 1u;
#endif
	}
}

/*============================================================================
 * Diem vao tu ngat
 *==========================================================================*/

/**
 * @brief  Nhan mot khung tu tang giao tiep CAN.
 * @param  stdId  Ma dinh danh cua khung.
 * @param  data   Du lieu khung.
 * @param  dlc    So byte hop le.
 *
 * @note   CHAY TRONG NGAT. No loc theo ma dinh danh va chep khung vao hang doi,
 *         khong lam gi hon. Moi viec khac cho vong lap chinh. Dinh nghia ham
 *         nay se lay quyen tu ban mac dinh danh dau yeu trong tang giao tiep
 *         CAN.
 */
void CAN_IF_OnFrameReceived(uint32_t stdId, const uint8_t *data, uint8_t dlc) {
	uint8_t nextTail;
	uint8_t index;

	if (isoTpAppInitialised == 0u) {
		/* Chua san sang */
	} else if (data == 0) {
		/* Khong co gi de lay */
	} else if (stdId != ISOTP_APP_ID_RX) {
		/* Khung cua module khac */
	} else {
		nextTail = (uint8_t) ((isoTpAppRxTail + 1u) % ISOTP_APP_RXQUEUE_DEPTH);

		if (nextTail != isoTpAppRxHead) {
			for (index = 0u; index < ISOTP_CAN_FRAME_SIZE; index++) {
				isoTpAppRxQueue[isoTpAppRxTail].data[index] =
						(index < dlc) ? data[index] : 0u;
			}
			isoTpAppRxQueue[isoTpAppRxTail].dlc = dlc;
			isoTpAppRxQueue[isoTpAppRxTail].timestampMs = HAL_GetTick();
			isoTpAppRxTail = nextTail;
		} else {
			/* Hang doi day: vong lap chinh khong theo kip. Dem so khung mat de
			 con bao ra, thay vi de troi qua khong ai biet. */
			isoTpAppRxDropped++;
		}
	}
}

/*============================================================================
 * Giao dien cong khai
 *==========================================================================*/

IsoTpApp_StatusType IsoTpApp_Init(void) {
	IsoTpApp_StatusType status;
	IsoTp_StatusType isoTpStatus;

	isoTpStatus = IsoTp_Init(IsoTpApp_CanSendWrapper,
			IsoTpApp_OnMessageReceived);

	if (isoTpStatus != ISOTP_OK) {
		status = ISOTP_APP_ERROR_STATE;
	} else {
		isoTpAppReceivedCount = 0u;
		isoTpAppRxHead = 0u;
		isoTpAppRxTail = 0u;
		isoTpAppRxDropped = 0u;
		isoTpAppTxLogHead = 0u;
		isoTpAppTxLogTail = 0u;
		isoTpAppRxLogLength = 0u;
		isoTpAppRxLogPending = 0u;
		isoTpAppEchoLength = 0u;
		isoTpAppEchoPending = 0u;
		isoTpAppEchoTooLong = 0u;
		isoTpAppSendFailed = 0u;
		isoTpAppInitialised = 1u;

		(void) snprintf(isoTpAppLogBuffer, sizeof(isoTpAppLogBuffer),
				"\r\n=== ISO-TP san sang ===\r\n"
						"Nhan ID 0x%03X, gui ID 0x%03X\r\n\r\n",
				(unsigned) ISOTP_APP_ID_RX, (unsigned) ISOTP_APP_ID_TX);
		uartlog(isoTpAppLogBuffer);

		status = ISOTP_APP_OK;
	}

	return status;
}

IsoTpApp_StatusType IsoTpApp_MainFunction(uint32_t currentTimeMs) {
	IsoTpApp_StatusType status;
	uint16_t echoLength;
	uint32_t stdId;
	uint8_t data[8];
	uint8_t dlc;

	while (CAN_IF_GetReceivedFrame(&stdId, data, &dlc) == OK) {
		CAN_IF_OnFrameReceived(stdId, data, dlc);
	}
	if (isoTpAppInitialised == 0u) {
		status = ISOTP_APP_ERROR_STATE;
	} else {
		/* Cac khung ngat da bat duoc se duoc xu ly o day, noi ma ISO-TP tra
		 loi bang khung dieu khien luong la an toan. */
		IsoTpApp_DrainRxQueue();

		(void) IsoTp_MainFunction(currentTimeMs);

		if (isoTpAppEchoPending != 0u) {
			isoTpAppEchoPending = 0u;
			echoLength = isoTpAppEchoLength;
			(void) IsoTp_Send(isoTpAppEchoBuffer, echoLength, currentTimeMs);
		} else {
			/* Khong co gi de gui nguoc lai */
		}

		IsoTpApp_FlushMessageLog();
		IsoTpApp_FlushTxLog();

		status = ISOTP_APP_OK;
	}

	return status;
}

IsoTpApp_StatusType IsoTpApp_SendMessage(const uint8_t *message,
		uint16_t length, uint32_t currentTimeMs) {
	IsoTpApp_StatusType status;
	IsoTp_StatusType isoTpStatus;

	if (isoTpAppInitialised == 0u) {
		status = ISOTP_APP_ERROR_STATE;
	} else if (message == 0) {
		status = ISOTP_APP_ERROR_NULL;
	} else {
		(void) snprintf(isoTpAppLogBuffer, sizeof(isoTpAppLogBuffer),
				"\r\n<<< GUI %u byte qua ISO-TP\r\n", (unsigned) length);
		uartlog(isoTpAppLogBuffer);

		isoTpStatus = IsoTp_Send(message, length, currentTimeMs);

		if (isoTpStatus != ISOTP_OK) {
			uartlog("<<< LOI: khong gui duoc\r\n");
			status = ISOTP_APP_ERROR_STATE;
		} else {
			status = ISOTP_APP_OK;
		}
	}

	return status;
}

uint32_t IsoTpApp_GetReceivedCount(void) {
	return isoTpAppReceivedCount;
}
