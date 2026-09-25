# UART5 recovery tests

`run.py` compiles the actual `Drivers/BSP/src/BSP_Dash.c` with the installed
STM32F429 HAL/CMSIS headers, then executes it in Unicorn Cortex-M4. HAL boundary
functions are controlled mocks; no physical UART, DMA, programmer, or Cube build
is used. Both `-O0` and `-Os` pass **76 assertions**. Source hashes and results
are recorded in `output/results.json`; compile logs are kept beside them.

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -X utf8 tools/tests/dash_irq_host/run.py
```

The fixture covers two independent classes of errors:

- RX recovery claims its pending flag before rearming reception. Error IRQs or
  a nested RX completion during `HAL_UART_Receive_IT` cannot lose a new recovery
  request. The TX DMA recovery latch survives a later unrelated RX error.
- A TX abort must return `HAL_OK` and leave its bound DMA stream disabled before
  the private transmit buffer becomes reusable. The fixture injects timeout
  with EN still set, timeout after EN clears, and apparent success with EN set.
  It also models the installed HAL's behavior after DMAT is already clear:
  `HAL_UART_AbortTransmit` may return success without attempting another DMA stop.

Accepted reinitialization also clears old readiness and TX permission before
calling HAL. Both initialization and RX-rearm failures remain unready; successful
reinitialization returns to receive-only operation.

An uncertain TX stop deliberately latches a fault until MCU reset. The tests
verify that the original private bytes remain unchanged, late TC callbacks and
later errors cannot release the slot, `Send`/`EnableTx`/runtime `Init` cannot
bypass the latch, and an eventual EN clear does not silently reauthorize TX.
RX recovery and byte delivery continue. The fixture resets static state only
when constructing a new simulated MCU-startup case.

This verifies production control flow against injected HAL outcomes. It does
not establish actual DMA electrical behavior, physical fault recovery, or a
hardware-tested UART peer connection. Fault locking is a safe retained-buffer
policy; it is not a claim that a stuck DMA stream was repaired without reset.
