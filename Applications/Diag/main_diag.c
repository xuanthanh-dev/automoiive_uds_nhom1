#include "main.h"
#include "Can_if.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * DIAG CAN CONFIGURATION
 * ============================================================ */

#define DIAG_CAN_ID_REQUEST          0x7E0U
#define DIAG_CAN_ID_RESPONSE         0x7E8U
#define DIAG_CAN_ID_ENGINE_STATUS    0x100U

#define DIAG_RESPONSE_TIMEOUT_MS     3000U

#define DIAG_TEST_COUNT              9U

/* ============================================================
 * ENGINE STATUS
 *
 * CAN ID = 0x100
 *
 * Byte 0..1 : Vehicle Speed
 * Byte 2..3 : RPM
 * Byte 4    : Temperature
 * Byte 5    : Battery voltage / 10
 * Byte 6..7 : Reserved
 * ============================================================ */

#define ENGINE_STATUS_DLC            8U

static uint8_t engineStatusData[ENGINE_STATUS_DLC];
static uint8_t engineStatusDlc;

static volatile uint8_t engineStatusAvailable;

/* ============================================================
 * DIAG TEST DESCRIPTION
 * ============================================================ */

typedef struct
{
    const char *id;
    const char *name;

    uint8_t request[3];
    uint8_t requestLength;

} Diag_TestType;

/* ============================================================
 * TEST LIST
 * ============================================================ */

static const Diag_TestType diagTests[DIAG_TEST_COUNT] =
{
    {
        "ST-002",
        "Read VIN",
        {0x22U, 0xF1U, 0x90U},
        3U
    },

    {
        "ST-003",
        "Read SW version",
        {0x22U, 0xF1U, 0x87U},
        3U
    },

    {
        "ST-004",
        "Read vehicle speed",
        {0x22U, 0x01U, 0x01U},
        3U
    },

    {
        "ST-005",
        "Read RPM",
        {0x22U, 0x01U, 0x02U},
        3U
    },

    {
        "ST-006",
        "Unknown DID",
        {0x22U, 0xFFU, 0xFFU},
        3U
    },

    {
        "ST-007",
        "Extended session",
        {0x10U, 0x03U, 0x00U},
        2U
    },

    {
        "ST-008",
        "ECU soft reset",
        {0x11U, 0x03U, 0x00U},
        2U
    },

    {
        "ST-009",
        "TesterPresent",
        {0x3EU, 0x00U, 0x00U},
        2U
    },

    {
        "ST-010/011",
        "Read active DTC",
        {0x19U, 0x02U, 0xFFU},
        3U
    }
};

/* ============================================================
 * HAL HANDLES
 * ============================================================ */

CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

/* ============================================================
 * DIAG ISO-TP RX BUFFER
 * ============================================================ */

static uint8_t diagResponse[64];

static uint16_t diagResponseLength;
static uint16_t diagResponseExpectedLength;

static uint8_t diagExpectedSequence;
static uint8_t diagReceivingConsecutiveFrames;

/* ============================================================
 * DIAG TEST STATE
 * ============================================================ */

static uint8_t diagSelectedTest;
static uint8_t diagRunAll;
static uint8_t diagWaitingResponse;

static uint32_t diagRequestTimeMs;
static uint32_t diagNextActionMs;

/* ============================================================
 * UART COMMAND BUFFER
 *
 * UART interrupt nhận từng byte.
 *
 * Ví dụ người dùng nhập:
 *
 *      10<ENTER>
 *
 * Buffer sẽ chứa:
 *
 *      "10"
 *
 * Sau khi nhận ENTER:
 *
 *      diagUartCommandReady = 1
 *
 * Main loop mới xử lý command.
 * ============================================================ */

#define DIAG_UART_COMMAND_SIZE       8U

static volatile uint8_t diagUartRxByte;

static volatile char diagUartCommand[
    DIAG_UART_COMMAND_SIZE
];

static volatile uint8_t diagUartCommandIndex;
static volatile uint8_t diagUartCommandReady;

/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */

static void SystemClock_Config(void);
static void MX_USART1_UART_Init(void);

static void Diag_PrintMenu(void);

static void Diag_PrintHex(
    const uint8_t *data,
    uint16_t length
);

static void Diag_HandleCommand(
    const char *command
);

static void Diag_StartTest(
    uint8_t testIndex
);

static void Diag_ProcessCan(
    uint32_t now
);

static void Diag_HandleResponseFrame(
    const uint8_t *frame,
    uint8_t dlc
);

static void Diag_CompleteResponse(void);

static void Diag_CheckTimeout(
    uint32_t now
);

static uint8_t Diag_IsResponseExpected(
    uint8_t testIndex,
    const uint8_t *response,
    uint16_t length
);

static void Diag_SendFlowControl(void);

static uint8_t Diag_SendCan(
    uint16_t canId,
    const uint8_t *data,
    uint8_t dlc
);

/* ============================================================
 * ENGINE STATUS FUNCTIONS
 * ============================================================ */

static void Diag_StoreEngineStatus(
    const uint8_t *data,
    uint8_t dlc
);

static void Diag_ReadEngineStatus(void);

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_USART1_UART_Init();

    /* ========================================================
     * Initialize CAN interface
     * ======================================================== */

    CAN_IF_Init();


    /* ========================================================
     * Initialize DIAG state
     * ======================================================== */

    diagSelectedTest = 0U;

    diagRunAll = 0U;

    diagWaitingResponse = 0U;

    diagResponseLength = 0U;

    diagResponseExpectedLength = 0U;

    diagExpectedSequence = 0U;

    diagReceivingConsecutiveFrames = 0U;

    engineStatusDlc = 0U;

    engineStatusAvailable = 0U;

    diagUartCommandIndex = 0U;

    diagUartCommandReady = 0U;

    memset(
        engineStatusData,
        0,
        sizeof(engineStatusData)
    );

    memset(
        diagResponse,
        0,
        sizeof(diagResponse)
    );

    /* ========================================================
     * Start UART receive interrupt
     *
     * Nhận từng byte.
     * ======================================================== */

    if (HAL_UART_Receive_IT(
            &huart1,
            (uint8_t *)&diagUartRxByte,
            1U) != HAL_OK)
    {
        Error_Handler();
    }

    /* ========================================================
     * Startup message
     * ======================================================== */

    printf("\r\n");
    printf("========================================\r\n");
    printf("          DIAG TESTER READY\r\n");
    printf("========================================\r\n");

    printf(
        "CAN REQUEST  : 0x%03X\r\n",
        DIAG_CAN_ID_REQUEST
    );

    printf(
        "CAN RESPONSE : 0x%03X\r\n",
        DIAG_CAN_ID_RESPONSE
    );

    printf(
        "ENGINE STATUS: 0x%03X\r\n",
        DIAG_CAN_ID_ENGINE_STATUS
    );

    printf("UART         : 115200 8-N-1\r\n");

    Diag_PrintMenu();

    /* ========================================================
     * MAIN LOOP
     * ======================================================== */

    while (1)
    {
        uint32_t now;

        now = HAL_GetTick();

        /* ====================================================
         * UART COMMAND
         *
         * Chỉ xử lý khi đã nhận ENTER.
         * ==================================================== */

        if (diagUartCommandReady != 0U)
        {
            char command[DIAG_UART_COMMAND_SIZE];

            /*
             * Copy command ra khỏi volatile buffer.
             */

            __disable_irq();

            strncpy(
                command,
                (const char *)diagUartCommand,
                DIAG_UART_COMMAND_SIZE
            );

            command[
                DIAG_UART_COMMAND_SIZE - 1U
            ] = '\0';

            diagUartCommandReady = 0U;

            __enable_irq();

            /*
             * Xử lý command ở main context.
             */

            Diag_HandleCommand(command);
        }

        /* ====================================================
         * CAN RX
         *
         * CAN ID 0x100 chỉ được lưu vào buffer.
         *
         * KHÔNG tự động printf Engine Status.
         * ==================================================== */

        Diag_ProcessCan(now);

        /* ====================================================
         * DIAG TIMEOUT
         * ==================================================== */

        Diag_CheckTimeout(now);

        /* ====================================================
         * RUN ALL TESTS
         * ==================================================== */

        if ((diagRunAll != 0U) &&
            (diagWaitingResponse == 0U) &&
            ((int32_t)(now - diagNextActionMs) >= 0))
        {
            if (diagSelectedTest < DIAG_TEST_COUNT)
            {
                Diag_StartTest(
                    diagSelectedTest
                );
            }
            else
            {
                diagRunAll = 0U;

                printf("\r\n");
                printf("========================================\r\n");
                printf("          ALL TESTS FINISHED\r\n");
                printf("========================================\r\n");

                Diag_PrintMenu();
            }
        }
    }
}

/* ============================================================
 * PRINT MENU
 * ============================================================ */

static void Diag_PrintMenu(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("             DIAG TEST MENU\r\n");
    printf("========================================\r\n");

    printf("1  : ST-002  Read VIN\r\n");
    printf("2  : ST-003  Read SW version\r\n");
    printf("3  : ST-004  Read vehicle speed\r\n");
    printf("4  : ST-005  Read RPM\r\n");
    printf("5  : ST-006  Unknown DID\r\n");
    printf("6  : ST-007  Extended session\r\n");
    printf("7  : ST-008  ECU soft reset\r\n");
    printf("8  : ST-009  TesterPresent\r\n");
    printf("9  : ST-010  Read active DTC\r\n");

    printf("----------------------------------------\r\n");

    printf("10 : Read Engine Status\r\n");

    printf("----------------------------------------\r\n");

    printf("A  : Run ALL UDS tests\r\n");

    printf("========================================\r\n");
    printf("Select: ");
}

/* ============================================================
 * UART RX CALLBACK
 *
 * ISR CHỈ nhận byte.
 *
 * Không xử lý DIAG trong ISR.
 * Không printf trong ISR.
 * ============================================================ */

void HAL_UART_RxCpltCallback(
    UART_HandleTypeDef *huart
)
{
    if (huart->Instance == USART1)
    {
        uint8_t received;

        received = diagUartRxByte;

        /* ====================================================
         * ENTER
         * ==================================================== */

        if ((received == '\r') ||
            (received == '\n'))
        {
            if (diagUartCommandIndex > 0U)
            {
                diagUartCommand[
                    diagUartCommandIndex
                ] = '\0';

                diagUartCommandReady = 1U;

                /*
                 * Không reset command index ở đây
                 * ngay lập tức vì main sẽ đọc buffer.
                 *
                 * Sau khi main xử lý, reset ở đây:
                 */

                diagUartCommandIndex = 0U;
            }
        }

        /* ====================================================
         * BACKSPACE
         * ==================================================== */

        else if (received == '\b')
        {
            if (diagUartCommandIndex > 0U)
            {
                diagUartCommandIndex--;
            }
        }

        /* ====================================================
         * NORMAL CHARACTER
         * ==================================================== */

        else
        {
            if (diagUartCommandIndex <
                (DIAG_UART_COMMAND_SIZE - 1U))
            {
                diagUartCommand[
                    diagUartCommandIndex
                ] = (char)received;

                diagUartCommandIndex++;
            }
        }

        /* ====================================================
         * Restart UART interrupt
         * ==================================================== */

        HAL_UART_Receive_IT(
            &huart1,
            (uint8_t *)&diagUartRxByte,
            1U
        );
    }
}

/* ============================================================
 * HANDLE UART COMMAND
 * ============================================================ */

static void Diag_HandleCommand(
    const char *command
)
{
    uint8_t testIndex;

    if (command == NULL)
    {
        return;
    }

    /* ========================================================
     * Ignore empty command
     * ======================================================== */

    if (command[0] == '\0')
    {
        return;
    }

    printf("\r\n");

    printf(
        "[DIAG] Command: %s\r\n",
        command
    );

    /* ========================================================
     * ENGINE STATUS
     *
     * Đây là điểm quan trọng:
     *
     * "10" KHÔNG gửi CAN request.
     *
     * Nó chỉ gọi:
     *
     *     Diag_ReadEngineStatus()
     *
     * ======================================================== */

    if (strcmp(command, "10") == 0)
    {
        Diag_ReadEngineStatus();

        Diag_PrintMenu();

        return;
    }

    /* ========================================================
     * RUN ALL
     * ======================================================== */

    if ((strcmp(command, "A") == 0) ||
        (strcmp(command, "a") == 0))
    {
        if (diagWaitingResponse != 0U)
        {
            printf(
                "[DIAG] Busy - waiting ECU response...\r\n"
            );

            return;
        }

        printf(
            "[DIAG] Run ALL UDS tests\r\n"
        );

        diagRunAll = 1U;

        diagSelectedTest = 0U;

        diagNextActionMs = HAL_GetTick();

        return;
    }

    /* ========================================================
     * Do not accept another UDS test while waiting
     * ======================================================== */

    if (diagWaitingResponse != 0U)
    {
        printf(
            "[DIAG] Busy - waiting ECU response...\r\n"
        );

        return;
    }

    /* ========================================================
     * TEST 1 -> 9
     * ======================================================== */

    if ((strlen(command) == 1U) &&
        (command[0] >= '1') &&
        (command[0] <= '9'))
    {
        testIndex =
            (uint8_t)(command[0] - '1');

        if (testIndex < DIAG_TEST_COUNT)
        {
            diagRunAll = 0U;

            Diag_StartTest(
                testIndex
            );
        }

        return;
    }

    /* ========================================================
     * Invalid command
     * ======================================================== */

    printf(
        "[DIAG] Unknown command: %s\r\n",
        command
    );

    Diag_PrintMenu();
}

/* ============================================================
 * START DIAG TEST
 * ============================================================ */

static void Diag_StartTest(
    uint8_t testIndex
)
{
    uint8_t frame[8] = {0};

    uint8_t index;

    if (testIndex >= DIAG_TEST_COUNT)
    {
        return;
    }

    /* ========================================================
     * Save current test
     * ======================================================== */

    diagSelectedTest = testIndex;

    /* ========================================================
     * ISO-TP Single Frame
     *
     * Byte 0 = payload length
     *
     * Byte 1.. = UDS request
     * ======================================================== */

    frame[0] =
        diagTests[testIndex].requestLength;

    for (index = 0U;
         index < diagTests[testIndex].requestLength;
         index++)
    {
        frame[index + 1U] =
            diagTests[testIndex].request[index];
    }

    printf("\r\n");

    printf(
        "[DIAG][%s] %s\r\n",
        diagTests[testIndex].id,
        diagTests[testIndex].name
    );

    printf("[DIAG] UDS TX: ");

    Diag_PrintHex(
        diagTests[testIndex].request,
        diagTests[testIndex].requestLength
    );

    printf("\r\n");

    printf(
        "[DIAG] CAN TX 0x%03X: ",
        DIAG_CAN_ID_REQUEST
    );

    Diag_PrintHex(
        frame,
        8U
    );

    printf("\r\n");

    /* ========================================================
     * Send CAN request
     * ======================================================== */

    if (Diag_SendCan(
            DIAG_CAN_ID_REQUEST,
            frame,
            8U) != 0U)
    {
        diagWaitingResponse = 1U;

        diagReceivingConsecutiveFrames = 0U;

        diagResponseLength = 0U;

        diagResponseExpectedLength = 0U;

        diagExpectedSequence = 0U;

        diagRequestTimeMs = HAL_GetTick();
    }
    else
    {
        printf(
            "[DIAG][%s] FAIL - CAN transmit error\r\n",
            diagTests[testIndex].id
        );

        if (diagRunAll != 0U)
        {
            diagSelectedTest++;

            diagNextActionMs =
                HAL_GetTick() + 150U;
        }
        else
        {
            Diag_PrintMenu();
        }
    }
}

/* ============================================================
 * PROCESS CAN RX
 *
 * CAN ID 0x7E8:
 *     -> ISO-TP UDS response
 *
 * CAN ID 0x100:
 *     -> Store Engine Status only
 *
 * IMPORTANT:
 *
 * 0x100 không printf ở đây.
 * ============================================================ */

static void Diag_ProcessCan(
    uint32_t now
)
{
    uint32_t canId;

    uint8_t frame[8];

    uint8_t dlc;

    (void)now;

    if (CAN_IF_GetReceivedFrame(
            &canId,
            frame,
            &dlc) != OK)
    {
        return;
    }

    /* ========================================================
     * UDS RESPONSE
     * ======================================================== */

    if (canId == DIAG_CAN_ID_RESPONSE)
    {
        printf(
            "[DIAG] CAN RX 0x%03lX: ",
            (unsigned long)canId
        );

        Diag_PrintHex(
            frame,
            dlc
        );

        printf("\r\n");

        Diag_HandleResponseFrame(
            frame,
            dlc
        );

        return;
    }

    /* ========================================================
     * ENGINE STATUS
     *
     * KHÔNG printf.
     *
     * Chỉ lưu dữ liệu.
     * ======================================================== */

    if ((canId == DIAG_CAN_ID_ENGINE_STATUS) &&
        (dlc == ENGINE_STATUS_DLC))
    {
        Diag_StoreEngineStatus(
            frame,
            dlc
        );

        return;
    }
}

/* ============================================================
 * STORE ENGINE STATUS
 *
 * Đây là nơi dữ liệu CAN 0x100 được lưu.
 *
 * ECU có thể gửi liên tục.
 *
 * Ví dụ:
 *
 * ECU -> 0x100 -> data
 * ECU -> 0x100 -> data
 * ECU -> 0x100 -> data
 *
 * Chỉ giá trị mới nhất được giữ trong buffer.
 * ============================================================ */

static void Diag_StoreEngineStatus(
    const uint8_t *data,
    uint8_t dlc
)
{
    if (data == NULL)
    {
        return;
    }

    if (dlc != ENGINE_STATUS_DLC)
    {
        return;
    }

    memcpy(
        engineStatusData,
        data,
        ENGINE_STATUS_DLC
    );

    engineStatusDlc = dlc;

    engineStatusAvailable = 1U;
}

/* ============================================================
 * READ ENGINE STATUS
 *
 * Hàm này CHỈ được gọi khi người dùng nhập:
 *
 *      10
 *
 * Không gửi CAN.
 *
 * Không yêu cầu ECU.
 *
 * Chỉ đọc dữ liệu hiện tại trong buffer.
 * ============================================================ */

static void Diag_ReadEngineStatus(void)
{
    uint16_t speed;

    uint16_t rpm;

    uint8_t temperature;

    uint8_t battery;

    uint8_t localData[ENGINE_STATUS_DLC];

    /* ========================================================
     * Check data available
     * ======================================================== */

    if (engineStatusAvailable == 0U)
    {
        printf("\r\n");

        printf(
            "[DIAG][ST-001] No Engine Status data available\r\n"
        );

        printf(
            "[DIAG] Waiting for CAN ID 0x%03X\r\n",
            DIAG_CAN_ID_ENGINE_STATUS
        );

        return;
    }

    /* ========================================================
     * Copy buffer
     *
     * Tránh dữ liệu bị thay đổi trong lúc đang đọc.
     * ======================================================== */

    __disable_irq();

    memcpy(
        localData,
        engineStatusData,
        ENGINE_STATUS_DLC
    );

    __enable_irq();

    /* ========================================================
     * Decode
     *
     * Byte 0..1 = Speed
     * Byte 2..3 = RPM
     * Byte 4    = Temperature
     * Byte 5    = Battery / 10
     * ======================================================== */

    speed =
        (uint16_t)(
            ((uint16_t)localData[0] << 8U) |
            localData[1]
        );

    rpm =
        (uint16_t)(
            ((uint16_t)localData[2] << 8U) |
            localData[3]
        );

    temperature =
        localData[4];

    battery =
        localData[5];

    /* ========================================================
     * Print result
     * ======================================================== */

    printf("\r\n");

    printf(
        "========================================\r\n"
    );

    printf(
        "          ENGINE STATUS\r\n"
    );

    printf(
        "========================================\r\n"
    );

    printf(
        "CAN ID      : 0x%03X\r\n",
        DIAG_CAN_ID_ENGINE_STATUS
    );

    printf(
        "DLC         : %u\r\n",
        engineStatusDlc
    );

    printf(
        "RAW DATA    : "
    );

    Diag_PrintHex(
        localData,
        ENGINE_STATUS_DLC
    );

    printf("\r\n");

    printf(
        "Speed       : %u km/h\r\n",
        speed
    );

    printf(
        "RPM         : %u\r\n",
        rpm
    );

    printf(
        "Temperature : %u C\r\n",
        temperature
    );

    printf(
        "Battery     : %u.%u V\r\n",
        battery / 10U,
        battery % 10U
    );

    printf(
        "========================================\r\n"
    );
}

/* ============================================================
 * HANDLE ISO-TP RESPONSE
 * ============================================================ */

static void Diag_HandleResponseFrame(
    const uint8_t *frame,
    uint8_t dlc
)
{
    uint8_t frameType;

    uint16_t copyLength;

    uint16_t remaining;

    uint8_t sequence;

    if ((diagWaitingResponse == 0U) ||
        (frame == NULL) ||
        (dlc == 0U))
    {
        return;
    }

    frameType =
        (uint8_t)(frame[0] & 0xF0U);

    /* ========================================================
     * SINGLE FRAME
     * ======================================================== */

    if (frameType == 0x00U)
    {
        copyLength =
            (uint16_t)(frame[0] & 0x0FU);

        if ((copyLength == 0U) ||
            (copyLength > 7U) ||
            (copyLength >
             ((uint16_t)dlc - 1U)))
        {
            printf(
                "[DIAG] Invalid ISO-TP Single Frame\r\n"
            );

            return;
        }

        for (remaining = 0U;
             remaining < copyLength;
             remaining++)
        {
            diagResponse[remaining] =
                frame[remaining + 1U];
        }

        diagResponseLength =
            copyLength;

        Diag_CompleteResponse();

        return;
    }

    /* ========================================================
     * FIRST FRAME
     * ======================================================== */

    if ((frameType == 0x10U) &&
        (dlc == 8U))
    {
        diagResponseExpectedLength =
            (uint16_t)(
                ((uint16_t)(frame[0] & 0x0FU) << 8U) |
                frame[1]
            );

        if ((diagResponseExpectedLength <= 7U) ||
            (diagResponseExpectedLength >
             sizeof(diagResponse)))
        {
            printf(
                "[DIAG] Invalid ISO-TP First Frame\r\n"
            );

            return;
        }

        /* ====================================================
         * First Frame contains 6 payload bytes
         * ==================================================== */

        for (remaining = 0U;
             remaining < 6U;
             remaining++)
        {
            diagResponse[remaining] =
                frame[remaining + 2U];
        }

        diagResponseLength = 6U;

        diagExpectedSequence = 1U;

        diagReceivingConsecutiveFrames = 1U;

        /* ====================================================
         * Send Flow Control
         * ==================================================== */

        Diag_SendFlowControl();

        return;
    }

    /* ========================================================
     * CONSECUTIVE FRAME
     * ======================================================== */

    if ((frameType == 0x20U) &&
        (diagReceivingConsecutiveFrames != 0U))
    {
        sequence =
            (uint8_t)(frame[0] & 0x0FU);

        /* ====================================================
         * Check sequence
         * ==================================================== */

        if (sequence != diagExpectedSequence)
        {
            printf(
                "[DIAG] ISO-TP sequence error "
                "expected=%u received=%u\r\n",
                diagExpectedSequence,
                sequence
            );

            diagReceivingConsecutiveFrames = 0U;

            return;
        }

        /* ====================================================
         * Calculate remaining
         * ==================================================== */

        remaining =
            (uint16_t)(
                diagResponseExpectedLength -
                diagResponseLength
            );

        copyLength =
            (remaining > 7U) ?
            7U :
            remaining;

        if (copyLength >
            ((uint16_t)dlc - 1U))
        {
            printf(
                "[DIAG] ISO-TP CF too short\r\n"
            );

            return;
        }

        /* ====================================================
         * Copy payload
         * ==================================================== */

        for (remaining = 0U;
             remaining < copyLength;
             remaining++)
        {
            diagResponse[
                diagResponseLength + remaining
            ] = frame[remaining + 1U];
        }

        diagResponseLength =
            (uint16_t)(
                diagResponseLength +
                copyLength
            );

        /* ====================================================
         * Next sequence number
         * ==================================================== */

        diagExpectedSequence =
            (uint8_t)(
                (diagExpectedSequence + 1U) &
                0x0FU
            );

        /* ====================================================
         * Complete message
         * ==================================================== */

        if (diagResponseLength >=
            diagResponseExpectedLength)
        {
            diagResponseLength =
                diagResponseExpectedLength;

            diagReceivingConsecutiveFrames = 0U;

            Diag_CompleteResponse();
        }

        return;
    }

    printf(
        "[DIAG] Unknown ISO-TP frame type: 0x%02X\r\n",
        frame[0]
    );
}

/* ============================================================
 * RESPONSE COMPLETE
 * ============================================================ */

static void Diag_CompleteResponse(void)
{
    uint8_t passed;

    printf("[DIAG] UDS RX: ");

    Diag_PrintHex(
        diagResponse,
        diagResponseLength
    );

    printf("\r\n");

    passed =
        Diag_IsResponseExpected(
            diagSelectedTest,
            diagResponse,
            diagResponseLength
        );

    printf(
        "[DIAG][%s] %s\r\n",
        diagTests[diagSelectedTest].id,
        (passed != 0U) ?
        "PASS" :
        "FAIL"
    );

    /* ========================================================
     * ST-010 / ST-011
     * ======================================================== */

    if (diagSelectedTest == 8U)
    {
        printf(
            "[DIAG][ST-010] "
            "OverTemp DTC 0x010001: %s\r\n",
            (passed != 0U) ?
            "PASS" :
            "FAIL"
        );

        printf(
            "[DIAG][ST-011] "
            "LowBattery DTC 0x010002: %s\r\n",
            (passed != 0U) ?
            "PASS" :
            "FAIL"
        );
    }

    /* ========================================================
     * Current request completed
     * ======================================================== */

    diagWaitingResponse = 0U;

    diagReceivingConsecutiveFrames = 0U;

    diagResponseLength = 0U;

    diagResponseExpectedLength = 0U;

    /* ========================================================
     * Run next test
     * ======================================================== */

    if (diagRunAll != 0U)
    {
        diagSelectedTest++;

        diagNextActionMs =
            HAL_GetTick() + 150U;
    }
    else
    {
        Diag_PrintMenu();
    }
}

/* ============================================================
 * CHECK EXPECTED UDS RESPONSE
 * ============================================================ */

static uint8_t Diag_IsResponseExpected(
    uint8_t testIndex,
    const uint8_t *response,
    uint16_t length
)
{
    if ((response == NULL) ||
        (length == 0U))
    {
        return 0U;
    }

    /* ========================================================
     * ST-002 VIN
     * ======================================================== */

    if (testIndex == 0U)
    {
        if ((length >= 3U) &&
            (response[0] == 0x62U) &&
            (response[1] == 0xF1U) &&
            (response[2] == 0x90U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-003 SW VERSION
     * ======================================================== */

    if (testIndex == 1U)
    {
        if ((length >= 3U) &&
            (response[0] == 0x62U) &&
            (response[1] == 0xF1U) &&
            (response[2] == 0x87U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-004 VEHICLE SPEED
     * ======================================================== */

    if (testIndex == 2U)
    {
        if ((length == 5U) &&
            (response[0] == 0x62U) &&
            (response[1] == 0x01U) &&
            (response[2] == 0x01U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-005 RPM
     * ======================================================== */

    if (testIndex == 3U)
    {
        if ((length == 5U) &&
            (response[0] == 0x62U) &&
            (response[1] == 0x01U) &&
            (response[2] == 0x02U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-006 UNKNOWN DID
     * ======================================================== */

    if (testIndex == 4U)
    {
        if ((length == 3U) &&
            (response[0] == 0x7FU) &&
            (response[1] == 0x22U) &&
            (response[2] == 0x31U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-007 EXTENDED SESSION
     * ======================================================== */

    if (testIndex == 5U)
    {
        if ((length == 2U) &&
            (response[0] == 0x50U) &&
            (response[1] == 0x03U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-008 ECU RESET
     * ======================================================== */

    if (testIndex == 6U)
    {
        if ((length == 2U) &&
            (response[0] == 0x51U) &&
            (response[1] == 0x03U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-009 TESTER PRESENT
     * ======================================================== */

    if (testIndex == 7U)
    {
        if ((length == 2U) &&
            (response[0] == 0x7EU) &&
            (response[1] == 0x00U))
        {
            return 1U;
        }

        return 0U;
    }

    /* ========================================================
     * ST-010 / ST-011 DTC
     *
     * Expected:
     *
     * 59 02 09
     * 01 00 01 status
     * 01 00 02 status
     * ======================================================== */

    if (testIndex == 8U)
    {
        if (length != 11U)
        {
            return 0U;
        }

        if ((response[0] != 0x59U) ||
            (response[1] != 0x02U) ||
            (response[2] != 0x09U))
        {
            return 0U;
        }

        /* DTC 010001 */

        if ((response[3] != 0x01U) ||
            (response[4] != 0x00U) ||
            (response[5] != 0x01U))
        {
            return 0U;
        }

        /* DTC 010002 */

        if ((response[7] != 0x01U) ||
            (response[8] != 0x00U) ||
            (response[9] != 0x02U))
        {
            return 0U;
        }

        return 1U;
    }

    return 0U;
}

/* ============================================================
 * SEND ISO-TP FLOW CONTROL
 * ============================================================ */

static void Diag_SendFlowControl(void)
{
    uint8_t frame[8] =
    {
        0x30U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        0x00U
    };

    printf(
        "[DIAG] CAN TX FlowControl 0x%03X: ",
        DIAG_CAN_ID_REQUEST
    );

    Diag_PrintHex(
        frame,
        8U
    );

    printf("\r\n");

    if (Diag_SendCan(
            DIAG_CAN_ID_REQUEST,
            frame,
            8U) == 0U)
    {
        printf(
            "[DIAG] FlowControl transmit error\r\n"
        );
    }
}

/* ============================================================
 * DIAG TIMEOUT
 * ============================================================ */

static void Diag_CheckTimeout(
    uint32_t now
)
{
    if ((diagWaitingResponse != 0U) &&
        ((now - diagRequestTimeMs) >
         DIAG_RESPONSE_TIMEOUT_MS))
    {
        printf(
            "[DIAG][%s] FAIL - timeout "
            "waiting ECU response\r\n",
            diagTests[diagSelectedTest].id
        );

        diagWaitingResponse = 0U;

        diagReceivingConsecutiveFrames = 0U;

        diagResponseLength = 0U;

        diagResponseExpectedLength = 0U;

        if (diagRunAll != 0U)
        {
            diagSelectedTest++;

            diagNextActionMs =
                now + 150U;
        }
        else
        {
            Diag_PrintMenu();
        }
    }
}

/* ============================================================
 * CAN SEND WRAPPER
 * ============================================================ */

static uint8_t Diag_SendCan(
    uint16_t canId,
    const uint8_t *data,
    uint8_t dlc
)
{
    if ((data == NULL) ||
        (dlc > 8U))
    {
        return 0U;
    }

    if (CAN_IF_Transmit(
            canId,
            (uint8_t *)data,
            dlc) == OK)
    {
        return 1U;
    }

    return 0U;
}

/* ============================================================
 * PRINT HEX
 * ============================================================ */

static void Diag_PrintHex(
    const uint8_t *data,
    uint16_t length
)
{
    uint16_t index;

    if (data == NULL)
    {
        return;
    }

    for (index = 0U;
         index < length;
         index++)
    {
        printf(
            "%02X ",
            data[index]
        );
    }
}

/* ============================================================
 * UART INIT
 * ============================================================ */

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;

    huart1.Init.BaudRate = 115200U;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * SYSTEM CLOCK
 * ============================================================ */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};

    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    osc.HSEState =
        RCC_HSE_ON;

    osc.HSEPredivValue =
        RCC_HSE_PREDIV_DIV1;

    osc.HSIState =
        RCC_HSI_ON;

    osc.PLL.PLLState =
        RCC_PLL_ON;

    osc.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    osc.PLL.PLLMUL =
        RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Error_Handler();
    }

    clk.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    clk.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    clk.APB1CLKDivider =
        RCC_HCLK_DIV2;

    clk.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &clk,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================
 * PRINTF -> UART
 * ============================================================ */

int __io_putchar(int ch)
{
    uint8_t character;

    character = (uint8_t)ch;

    (void)HAL_UART_Transmit(
        &huart1,
        &character,
        1U,
        HAL_MAX_DELAY
    );

    return ch;
}

/* ============================================================
 * UART LOG
 * ============================================================ */

void uartlog(char *message)
{
    if (message == NULL)
    {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)message,
        (uint16_t)strlen(message),
        HAL_MAX_DELAY
    );
}

/* ============================================================
 * ERROR HANDLER
 * ============================================================ */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
