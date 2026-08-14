| Test ID       | Scenario       | Input                     | Expected Result   |
| ------------- | -------------- | ------------------------- | ----------------- |
| SYS-CANIF-001 | CANIF Init     | CAN configuration         | `0x00`            |
| SYS-CANIF-002 | TX 8 bytes     | ID `0x123`, DLC 8         | Frame TX đúng     |
| SYS-CANIF-003 | TX DLC 0       | DLC 0                     | Frame TX đúng     |
| SYS-CANIF-004 | Invalid DLC    | DLC 9                     | `0x03`            |
| SYS-CANIF-005 | ID Boundary    | `0x000`, `0x7FF`, `0x800` | ID được validate  |
| SYS-CANIF-006 | RX Frame       | ID `0x123`, DLC 8         | RX đúng           |
| SYS-CANIF-007 | RX Interrupt   | Valid CAN frame           | Callback được gọi |
| SYS-CANIF-008 | TX-RX E2E      | TX frame                  | RX = TX           |
| SYS-CANIF-009 | Data Pattern   | `00/FF/AA/55/...`         | Data giống nhau   |
| SYS-CANIF-010 | Multiple Frame | 3 CAN frames              | Nhận đủ 3         |
| SYS-CANIF-011 | RX Timeout     | Không có frame            | `0x08`            |
| SYS-CANIF-012 | CAN Error      | Bus error                 | Error code        |
