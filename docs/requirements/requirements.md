# STM32 CAN UDS Diagnostic ECU Project
# V-Model Requirements & Test Plan

> Project: **Development of a Real Automotive Diagnostic ECU on STM32 using CAN and UDS**

---

## 1. Document Purpose

This document defines the initial requirements and verification plan for a student project that implements a diagnostic ECU on STM32 using CAN and UDS.

The goal is not to create a production-grade automotive ECU. The goal is to teach students the engineering mindset:

```text
Requirement → Design → Code → Test → Evidence → Release
```

Each requirement should be traceable to at least one test case and one implementation area.

---

## 2. Project Scope

### 2.1 In Scope

The project shall include:

- STM32-based firmware written in Embedded C.
- CAN communication through a CAN controller and external CAN transceiver.
- EngineStatus cyclic CAN message.
- UDS diagnostic server over CAN.
- Basic ISO-TP support for UDS payload transport.
- DID manager for diagnostic data.
- DTC manager for simulated faults.
- Python or USB-CAN based tester.
- GitHub-based workflow: issue, branch, PR, review, merge, release.
- Test evidence: CAN traces, UART logs, screenshots, tester logs.

### 2.2 Out of Scope

The MVP does not require:

- Real vehicle connection.
- Real OBD-II integration.
- Full AUTOSAR stack.
- Product-grade ISO-TP timing conformance.
- Production bootloader.
- Production security access algorithm.
- Persistent NVM DTC storage.
- Functional Safety or Cybersecurity certification.

---

## 3. V-Model Mapping

```text
System Requirements
        ↓
Software Requirements
        ↓
Software Architecture Design
        ↓
Detailed Design
        ↓
Coding
        ↓
Unit Testing
        ↑
Integration Testing
        ↑
System Testing
        ↑
Acceptance Testing
```

| V-Model Area | Project Artifact |
|---|---|
| System Requirement | SYS-* requirements |
| Software Requirement | SWR-* requirements |
| Architecture Design | software_architecture.md |
| Detailed Design | can_matrix.md, uds_design.md |
| Coding | firmware source code, Python tester |
| Unit Test | unit_test_spec.md |
| Integration Test | integration_test_spec.md |
| System Test | system_test_spec.md |
| Acceptance Test | acceptance_test_spec.md |
| Traceability | traceability_matrix.md |

---

# 4. System Requirements

## SYS-001 - ECU shall transmit EngineStatus cyclically

### Requirement

The ECU shall transmit the `EngineStatus` CAN message cyclically on the CAN bus.

### Acceptance Criteria

- CAN ID is `0x100`.
- DLC is `8`.
- Cycle time is `1000 ms ± 10%`.
- Message contains VehicleSpeed, EngineRPM, EngineTemp and BatteryVoltage.

### Verification Method

System Test `ST-001` and Acceptance Test `AT-001`.

---

## SYS-002 - ECU shall support UDS diagnostic communication

### Requirement

The ECU shall act as a UDS diagnostic server and respond to diagnostic requests from a tester over CAN.

### Acceptance Criteria

The ECU supports at least:

| SID | Service |
|---|---|
| `0x10` | DiagnosticSessionControl |
| `0x11` | ECUReset |
| `0x19` | ReadDTCInformation |
| `0x22` | ReadDataByIdentifier |
| `0x3E` | TesterPresent |

### Verification Method

System Tests `ST-002` to `ST-009` and Acceptance Test `AT-002`.

---

## SYS-003 - ECU shall provide VIN information

### Requirement

The ECU shall return VIN information when DID `0xF190` is requested.

### Request

```text
22 F1 90
```

### Expected Positive Response

```text
62 F1 90 <VIN data>
```

### Acceptance Criteria

- Response starts with `62 F1 90`.
- VIN is encoded as ASCII data.
- Response is transported correctly over ISO-TP if longer than 7 bytes.

### Verification Method

Unit Test `UT-DID-001`, Integration Test `IT-002`, System Test `ST-002`.

---

## SYS-004 - ECU shall provide software version information

### Requirement

The ECU shall return software version when DID `0xF187` is requested.

### Request

```text
22 F1 87
```

### Expected Response

```text
62 F1 87 <Software Version>
```

### Verification Method

Unit Test `UT-DID-001`, Integration Test `IT-003`, System Test `ST-003`.

---

## SYS-005 - ECU shall provide dynamic vehicle data through DID

### Requirement

The ECU shall provide simulated vehicle data through UDS ReadDataByIdentifier.

### Supported DIDs

| DID | Data |
|---|---|
| `0x0101` | VehicleSpeed |
| `0x0102` | EngineRPM |
| `0x0103` | EngineTemp |
| `0x0104` | BatteryVoltage |

### Verification Method

Unit Test `UT-DID-001`, Integration Test `IT-003`, System Tests `ST-004` and `ST-005`.

---

## SYS-006 - ECU shall manage diagnostic trouble codes

### Requirement

The ECU shall store and report diagnostic trouble codes when simulated fault conditions occur.

### Supported Demo DTCs

| Fault | DTC |
|---|---|
| Engine Over Temperature | `0x010001` |
| Low Battery | `0x010002` |

### Acceptance Criteria

- Injected Over Temperature fault is reported as active DTC `0x010001`.
- Injected Low Battery fault is reported as active DTC `0x010002`.
- DTCs are readable through UDS service `0x19`.

### Verification Method

Unit Tests `UT-DTC-001`, `UT-DTC-002`, Integration Test `IT-004`, System Tests `ST-010`, `ST-011`.

---

## SYS-007 - ECU shall return negative responses for invalid requests

### Requirement

The ECU shall return a valid UDS negative response for unsupported services or invalid data identifiers.

### Expected Format

```text
7F <Request SID> <NRC>
```

### Examples

```text
Request: 99
Response: 7F 99 11
```

```text
Request: 22 FF FF
Response: 7F 22 31
```

### Verification Method

Unit Tests `UT-UDS-002`, `UT-UDS-003`, System Test `ST-006`.

---

## SYS-008 - ECU shall be demonstrable on real hardware

### Requirement

The final project shall be demonstrable using STM32 board, CAN transceiver and a diagnostic tester.

### Acceptance Criteria

- ECU runs on STM32 hardware.
- CAN frame can be captured by USB-CAN or monitor node.
- UDS request and response can be shown in trace evidence.
- Final demo includes source code, wiring, test cases, test result and report.

### Verification Method

Acceptance Tests `AT-001`, `AT-002`, `AT-003`.

---

# 5. Software Requirements

## SWR-CAN-001 - CAN abstraction shall provide initialization API

```c
void CanIf_Init(void);
```

The API initializes the CAN abstraction layer internal state.

---

## SWR-CAN-002 - CAN abstraction shall provide transmit API

```c
Std_ReturnType CanIf_Send(const CanIf_PduType* pdu);
```

Acceptance criteria:

- Return `E_OK` for valid PDU.
- Return `E_NOT_OK` for `NULL` PDU.
- Return `E_NOT_OK` when DLC > 8.

---

## SWR-CAN-003 - CAN abstraction shall provide RX indication callback

```c
void CanIf_RxIndication(const CanIf_PduType* pdu);
```

Acceptance criteria:

- Received PDU is stored or forwarded safely.
- Registered callback is notified.
- UDS request ID `0x200` can be routed to ISO-TP.

---

## SWR-APP-001 - Application shall maintain engine state

The application layer shall maintain:

- VehicleSpeed.
- EngineRPM.
- EngineTemp.
- BatteryVoltage.

---

## SWR-APP-002 - Application shall pack EngineStatus CAN message

EngineStatus signal layout:

| Byte | Signal |
|---|---|
| 0-1 | VehicleSpeed raw |
| 2-3 | EngineRPM raw |
| 4 | EngineTemp raw |
| 5 | BatteryVoltage raw |
| 6-7 | Reserved |

---

## SWR-DID-001 - DID Manager shall support static DIDs

| DID | Data |
|---|---|
| `0xF190` | VIN |
| `0xF187` | Software Version |

---

## SWR-DID-002 - DID Manager shall support dynamic DIDs

| DID | Data Source |
|---|---|
| `0x0101` | VehicleSpeed |
| `0x0102` | EngineRPM |
| `0x0103` | EngineTemp |
| `0x0104` | BatteryVoltage |

---

## SWR-DTC-001 - DTC Manager shall store active DTCs

The DTC Manager shall support:

- Set DTC active.
- Clear DTC inactive.
- Return active DTC list.

---

## SWR-UDS-001 - UDS Dispatcher shall route requests by SID

The UDS dispatcher shall route supported services:

- `0x10`
- `0x11`
- `0x19`
- `0x22`
- `0x3E`

---

## SWR-UDS-002 - Positive response SID shall equal request SID + 0x40

| Request SID | Positive Response SID |
|---|---|
| `0x10` | `0x50` |
| `0x11` | `0x51` |
| `0x19` | `0x59` |
| `0x22` | `0x62` |
| `0x3E` | `0x7E` |

---

## SWR-UDS-003 - Negative response shall follow UDS format

```text
7F <Request SID> <NRC>
```

Baseline NRCs:

| NRC | Meaning |
|---|---|
| `0x11` | Service Not Supported |
| `0x12` | Sub-function Not Supported |
| `0x13` | Incorrect Message Length |
| `0x22` | Conditions Not Correct |
| `0x31` | Request Out Of Range |
| `0x33` | Security Access Denied |

---

## SWR-ISO-TP-001 - ISO-TP shall support Single Frame

The ISO-TP layer shall support UDS payload up to 7 bytes using Single Frame.



## SWR-ISO-TP-002 - ISO-TP shall support basic multi-frame response

The ISO-TP layer shall support segmented response for payload larger than 7 bytes, such as VIN response.

---

# 6. Definition of Done

A requirement or task is done when:

- Code is implemented.
- Code builds successfully.
- Unit or integration test exists.
- Test is executed.
- Evidence is captured.
- Pull Request is reviewed.
- Traceability matrix is updated.
