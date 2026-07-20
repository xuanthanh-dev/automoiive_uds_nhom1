# System Test Specification

| Test ID | Objective | Expected Result |
|---|---|---|
| ST-001 | EngineStatus cyclic message | ID `0x100`, DLC 8, period 1000ms ±10% |
| ST-002 | Read VIN | `62 F1 90 <VIN>` |
| ST-003 | Read SW Version | `62 F1 87 <SW version>` |
| ST-004 | Read VehicleSpeed | `62 01 01 <speed>` |
| ST-005 | Read RPM | `62 01 02 <rpm>` |
| ST-006 | Unknown DID | `7F 22 31` |
| ST-007 | Extended session | `50 03` |
| ST-008 | ECU reset | `51 03` |
| ST-009 | TesterPresent | `7E 00` |
| ST-010 | OverTemp DTC | DTC `0x010001` returned |
| ST-011 | LowBattery DTC | DTC `0x010002` returned |
