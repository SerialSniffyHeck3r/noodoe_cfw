# Bluetooth actual-C host regression

Run `run.py` with the configured Python runtime. It compiles production BSP,
service, port, patch selection and key storage C for Cortex-M4 at `-O0` and
`-Os`, then executes those binaries under Unicorn. Output hashes/results are
written to `output/results.json`. No ST-LINK or Bluetooth hardware is opened.

The UART/DMA fixture models committed receive bytes, NDTR, TC, error flags,
HAL callback handoff and abort failures. It is not an analog timing model or
a complete STM32 peripheral simulator. Tests prove the exercised software
contracts under those modeled events, not controller readiness on the board.

Current551 assertions per optimization comprise314 existing main checks,
15+15 abort cases,8 inherited-DMA checks,189 raw transaction checks and10 raw
abort/quarantine checks. Raw cases include exact single Reset payload, varying
command credits, stale prior error followed by success, invalid/no response,
nonzero Reset status, send failure, bounded timeout, final-byte error boundary,
DMA buffer ownership on failed abort, H4 exclusion, queue conflicts and
seq-stable snapshots. MMIO hooks reject pending-DR reads and writes to the
diagnostic record outside its odd sequence. Only USART1 may be reset, after
both owned streams are disabled. Model-specific expected reset counts are
checked per scenario.

The three modified production modules are additionally compiled with
`-Wall -Wextra -Werror`; this is separate from the full CubeIDE build and
on-device validation, which remain the root agent's responsibility.
