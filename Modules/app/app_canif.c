#include "app_canif.h"
#include "canif.h"
#include "usart.h"

#include <stdio.h>

extern UART_HandleTypeDef huart1;

/*=============================
 * Private Function Prototype
 *=============================*/
static void App_PrintMenu(void);
static void App_SendTestFrame(void);
static void App_ReadVIN(void);
static void App_ReadSWVersion(void);
static void App_ECUReset(void);
static void App_TesterPresent(void);

/*=============================
 * Public Functions
 *=============================*/

void App_CanIf_Init(void)
{
    printf("\r\n");
    printf("=====================================\r\n");
    printf("      CANIF DEBUG APPLICATION\r\n");
    printf("=====================================\r\n");

    App_PrintMenu();
}

void App_CanIf_Run(void)
{
    uint8_t cmd;

    if (HAL_UART_Receive(&huart1, &cmd, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        return;
    }

    printf("%c\r\n", cmd);

    switch(cmd)
    {
        case '1':
            App_SendTestFrame();
            break;

        case '2':
            App_ReadVIN();
            break;

        case '3':
            App_ReadSWVersion();
            break;

        case '4':
            App_ECUReset();
            break;

        case '5':
            App_TesterPresent();
            break;

        case 'h':
        case 'H':
            App_PrintMenu();
            break;

        default:
            printf("\r\n[ERROR] Invalid command!\r\n");
            break;
    }

    printf("\r\nSelect Command > ");
}

/*=============================
 * Private Functions
 *=============================*/

static void App_PrintMenu(void)
{
    printf("\r\n");
    printf("=============== MENU ===============\r\n");
    printf("1. Send Test CAN Frame\r\n");
    printf("2. Read VIN\r\n");
    printf("3. Read Software Version\r\n");
    printf("4. ECU Reset\r\n");
    printf("5. Tester Present\r\n");
    printf("H. Show Menu\r\n");
    printf("====================================\r\n");

    printf("Select Command > ");
}

static void App_SendTestFrame(void)
{
    uint8_t data[8] =
    {
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88
    };

    printf("\r\n[TX] Send Test CAN Frame\r\n");

    CanIf_Send(0x123, data, 8);
}

static void App_ReadVIN(void)
{
    uint8_t req[3] =
    {
        0x22,
        0xF1,
        0x90
    };

    printf("\r\n[TX] Read VIN\r\n");

    CanIf_Send(0x700, req, 3);
}

static void App_ReadSWVersion(void)
{
    uint8_t req[3] =
    {
        0x22,
        0xF1,
        0x87
    };

    printf("\r\n[TX] Read Software Version\r\n");

    CanIf_Send(0x700, req, 3);
}

static void App_ECUReset(void)
{
    uint8_t req[2] =
    {
        0x11,
        0x01
    };

    printf("\r\n[TX] ECU Reset\r\n");

    CanIf_Send(0x700, req, 2);
}

static void App_TesterPresent(void)
{
    uint8_t req[2] =
    {
        0x3E,
        0x00
    };

    printf("\r\n[TX] Tester Present\r\n");

    CanIf_Send(0x700, req, 2);
}