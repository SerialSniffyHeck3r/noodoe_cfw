# Actual UART test — 2026-09-16

## Outcome

**Blocked at the physical UART link; not a passing receive/UI/IGN test.**

The PC identified the previously verified Arduino Uno USB bridge as COM11,
USB VID:PID 2341:0043, serial [redacted USB-UART serial]. COM3 is Intel AMT and was
not opened. Only the existing isolated Uno–Noodoe harness was used.

At 115200 8N1, the existing dashboard_uart.py worker transmitted the recorded
CMD21/fuel1 frame with synthetic speeds 0 → 30 → 73 → 0 km/h (8 seconds per
stage), ODO 36,475 km, and observed fuel level one bar. Length/XOR and fields
were independently decoded by noodoe_protocol.vehicle_uart. Unknown status
fields were left unchanged. No RTC, OIL, IGN, or estimated range was invented.

ST-LINK was initially absent, then became available. Live, non-halting reads
of the installed candidate ELF diagnostics established:

- Physical IGN valid=1, ON=1; Product UI ready=1, error=0.
- MCU RX bytes remained **1** and valid RX frames remained **0** across two
  observations separated by about 164 seconds of MCU uptime.
- UART error count remained **1**, checksum errors and overflows remained **0**.
  That single historical UART error is not attributed to this injection.
- UART TX completions increased 5640 → 5776, but PC RX stayed **0 bytes**.
- Vehicle and UI speed/ODO validity remained zero; accumulated ride distance
  remained zero. No real receive, speed display, or Ride Summary success is claimed.
- The MCU UART is enabled for TX/RX with RXNE interrupt: CR1=0x202C, CR2=0,
  CR3=1, BRR=0x16C. PC12 and PD2 are AF8; UART5 IRQ is enabled. Reading did
  not touch the data register, halt/reset the MCU, or change UART configuration.

The automated consumer-verification scenario correctly failed on its first
zero-speed stage instead of replacing UART data with a RAM override. UART
signal wiring/direction/common ground remains to be confirmed by the user.
The worker was stopped cleanly, transmitting its final zero-speed frame.

## Evidence and rerun

- `transmit-only-result.json`: host-only 0/30/73/0 transmission evidence.
- `uart.json`: bounded TX/RX byte history; `status.json`: final worker state.
- `scenario-plan.json`: exact validated frames.
- `scenario-result.json`: receive assertion failure.
- `step-0-0/result.json`, `zero-recheck/result.json`: actual MCU snapshots.
- `register-check/result.json`: live GPIO/UART configuration reads.
- `run_scenario.py`: real UART speed-file control and MCU consumer assertions.
- `observe.py`: read-only diagnostic decoder, using the installed
  `../2026-09-16-ign-deferred-summary/candidate.elf`.

Use a fresh evidence directory on rerun; existing snapshot files are preserved.
Firmware FLASH/NOR/options and application source were not modified.
