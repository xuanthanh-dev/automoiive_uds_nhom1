# Integration Test Specification

| Test ID | Chain | Input | Expected Result |
|---|---|---|---|
| IT-001 | App → CanIf | Run scheduler 1000ms | CAN ID `0x100` sent |
| IT-002 | CAN RX → ISO-TP → UDS | `03 22 F1 90 ...` | UDS receives `22 F1 90` |
| IT-003 | UDS → DID | `22 F1 87` | SW version returned |
| IT-004 | UDS → DTC | Fault + `19 02 FF` | DTC returned |
| IT-005 | UDS session | `10 03` | `50 03` |
| IT-006 | TesterPresent | `3E 00` | `7E 00` |
