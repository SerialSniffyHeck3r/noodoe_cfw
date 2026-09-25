# RTC lifecycle and alarm failures

The fixture executes actual `BSP_Clock.c` compiled for Cortex-M4 with the installed
STM32 HAL/CMSIS headers. HAL calls, RTC flag write side effects, and the RTC clock
bit-band alias are boundary models. The normal production register-macro path is
also compiled separately with `-Wall -Wextra -Werror`.

Both `-O0` and `-Os` pass **83 assertions**. `output/results.json` binds results
to source, header, fixture and runner hashes. No Cube build, programmer, physical
RTC, clock adjustment, alarm firing, or backup-domain reset is performed.

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -X utf8 tools/tests/clock_host/run.py
```

Checks include:

- Attach preserves calendar and backup words; bad retained clock state,12-hour
  mode, or initial read failure cannot publish ready. No implicit calendar set.
- Every GetTime attempt is followed by GetDate, including an injected failure.
  Partial reads invalidate the sample; a nested operation returns BUSY without
  interleaving another Time/Date pair. Masked contexts are rejected.
- Valid leap dates succeed; impossible fields are rejected before either write.
  Date-set failure prevents time-set, and time-set failure reports the partial
  date change without pretending rollback happened.
- Shared alarm IRQ is disabled before old Alarm A deactivation and pending
  cleanup; successful installation precedes IRQ enable. Failed cancellation
  returns BUSY/TIMEOUT, and both entry IRQ states are restored on failures/cancel.
- Existing enabled Alarm B flags/registers/shared pending state are preserved.
  B arriving during shared pending cleanup is re-pended at NVIC. Tests also
  preserve the existing shared-vector priority while B interrupts are enabled.

These are production control-flow tests with injected HAL outcomes. They do not
prove physical LSE accuracy, register synchronization timing, actual interrupt
delivery, or real alarm cancellation under a hardware failure. Calendar set uses
two HAL transactions and is not hardware-atomic; failures remain explicit.
