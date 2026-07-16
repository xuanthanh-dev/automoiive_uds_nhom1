# Requirement Traceability Matrix

| Requirement ID | Summary | Design | Source Module | Unit Test | Integration Test | System Test | Acceptance Test | Evidence |
|---|---|---|---|---|---|---|---|---|
| SYS-001 | EngineStatus cyclic transmission | CAN Matrix, App Scheduler | app_engine.c, can_if.c | UT-CAN-001 | IT-001 | ST-001 | AT-001 | CAN trace |
| SYS-002 | UDS diagnostic communication | UDS Design | uds.c, isotp.c, can_if.c | UT-UDS-001 | IT-002 | ST-002..ST-009 | AT-002 | Tester log |
| SYS-003 | Read VIN | DID Table | did.c, uds.c | UT-DID-001 | IT-002 | ST-002 | AT-002 | Tester log |
| SYS-004 | Read SW Version | DID Table | did.c, uds.c | UT-DID-001 | IT-003 | ST-003 | AT-002 | Tester log |
| SYS-005 | Read dynamic data | DID Table, App State | app_engine.c, did.c | UT-DID-001 | IT-003 | ST-004, ST-005 | AT-002 | Tester log |
| SYS-006 | DTC management | DTC Design | dtc.c, uds.c | UT-DTC-001 | IT-004 | ST-010, ST-011 | AT-003 | DTC log |
| SYS-007 | Negative response | UDS NRC Handling | uds.c | UT-UDS-002 | IT-002 | ST-006 | AT-002 | Tester log |
| SYS-008 | Real hardware demo | Hardware Architecture | all modules | N/A | N/A | ST-001..ST-011 | AT-001..AT-004 | Demo video |
