# IGN-only vehicle recovery entry

User constraint: the installed vehicle retains permanent12V. The key changes
IGN, not the power rail. IGN wake must not be treated as reset/cold startup.

## Current implementation audit

- `App_Logic/Runtime/src/NoodoeRuntime.c:122` polls the recovery chord during
  normal runtime. It does not require a startup window or functioning Bluetooth.
- `App_Logic/Recovery/src/App_Recovery.c:220` queues the runtime chord request.
  `:213` waits for the delay and storage drain, writes a checked retained
  waiting intent, then calls the actual CMSIS `NVIC_SystemReset()`.
- `NoodoeRuntime.c:218` checks recovery before its deep-sleep early return;
  its owned quiesce lease participates in `busy`, prevents STOP readiness, and
  lets already-active storage transactions finish. It does not borrow a host
  backup lease. Storage waits are bounded even before the request is observed.
- I/O's POWER_DEEP branch intentionally skips chord polling. `BSP_Power` EXTI
  and `PowerService_IgnitionIRQ()` wake owners on IGN; the raw ON level also
  rejects STOP. The stable input is published until UI returns policy to RUN.
  Therefore the vehicle procedure is **IGN ON → running DOWN+ENTER chord →
  software reset → explicit recovery confirmation**, with permanent12V kept on.
- Normal CPU software reset preserves the checked `.noinit.app_recovery` intent
  needed by the early hook. Removing permanent power is neither needed nor part
  of this workflow.

## Scope and boundaries

No production C, IOC, linker or installed firmware changed for this clarification.
The existing route already supports the permanent-power requirement. Hardware
was not contacted in this follow-up. `AGENTS.md` and Bootstrap documentation now
make IGN/wake/reset/power removal distinct.

The ARM regression adds real runtime GPIO chord processing 24 hours after
startup, short-chord rejection, DOWN-only rejection, the mandatory writer-drain
and delay, physical intent surviving Bluetooth-disconnect notification, and an
actual CMSIS reset interception with reason WAIT rather than CONFIRMED. GPIO
input/output bytes remain unchanged across the software-reset request. It runs
with simulated raw IGN ON and OFF; **this does not exercise RTOS POWER_DEEP
scheduling or prove physical IGN wake**. The input can be OFF while the runtime
task is still polling in a non-deep policy.

Both O0 and Os passed: original17 checks plus the control/fault/identity checks
and both new runtime-chord cases. Each build intercepted four real CMSIS reset
writes (remote confirmation, fault, IGN-ON runtime chord, IGN-OFF runtime chord).
Results are in `results.json`, console output in `app-test.log`, and source hashes
in `source-sha256.json`. A passed emulation is not a physical button test on
either board.

The runtime hook cannot rescue an APP that cannot execute it or reach startup.
Key cycling does not independently reset such a hung CPU. A previously committed
resident update is also processed before the APP recovery hook on reset; this
entry is not an override of an already-pending installation. Neither limitation
is solved by keeping12V connected, and neither is claimed as guaranteed rollback.
