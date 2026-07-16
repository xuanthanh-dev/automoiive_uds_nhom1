# Acceptance Test Specification

## AT-001 - Hardware CAN Demo

Acceptance criteria:

- STM32 ECU runs on hardware.
- CAN transceiver is connected correctly.
- EngineStatus `0x100` is visible in CAN trace.
- Evidence is captured.

## AT-002 - Full UDS Diagnostic Flow

Demo sequence:

```text
10 03
22 F1 90
22 F1 87
22 01 01
19 02 FF
3E 00
11 03
```

Acceptance criteria:

- Supported services respond correctly.
- Negative response is demonstrated.
- Tester log is captured.

## AT-003 - Fault Injection Demo

Acceptance criteria:

- DTC appears after fault injection.
- DTC response format is valid.
- Evidence is captured.

## AT-004 - GitHub Process Demo

Acceptance criteria:

- Issues, branches, PRs, reviews and release tag are shown.
- Test evidence is linked to PR or report.
