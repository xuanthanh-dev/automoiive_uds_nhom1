# Unit Test Specification

| Test ID | Module | Objective | Expected Result |
|---|---|---|---|
| UT-CAN-001 | CanIf | Send valid PDU | `E_OK` |
| UT-CAN-002 | CanIf | Reject DLC > 8 | `E_NOT_OK` |
| UT-DID-001 | DID | Read VIN | VIN returned |
| UT-DID-002 | DID | Read unknown DID | `E_NOT_OK` |
| UT-DTC-001 | DTC | Set active DTC | DTC appears active |
| UT-DTC-002 | DTC | Clear DTC | DTC inactive |
| UT-UDS-001 | UDS | Read VIN | `62 F1 90 <VIN>` |
| UT-UDS-002 | UDS | Unsupported service | `7F 99 11` |
| UT-ISO-TP-001 | ISO-TP | Single Frame encode | `03 22 F1 90 00 00 00 00` |
