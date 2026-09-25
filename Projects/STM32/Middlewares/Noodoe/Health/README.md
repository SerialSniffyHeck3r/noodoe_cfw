# Watchdog and task progress

`Health_Service` owns runtime health policy; `BSP_Watchdog` is the only project
writer of the IWDG key register. Drivers, interrupt handlers, fault loops and
the sleep port do not refresh it. An unavailable Bluetooth controller is a
degraded service, not a failed CPU: its owner must still finish housekeeping.

## Phases and bounds

| Phase | Proof and deadline |
|---|---|
| BOOT | Completed checkpoints at most 2s apart, absolute 30s budget. First task reports may wait for initialization. |
| RUN | Supervisor every100ms checks IO/BT within1s, Storage within2s, Graphics within2.5s. OFF raises IO/BT to2.5s. |
| WAIT | Local recovery UI continues bounded checkpoints; user waiting has no total limit. |
| RECOVERY | Checkpoints at most2s apart, absolute maximum10min per operation. |
| FLASH | One DWT-clocked5s lease; only RAM routines may refresh while flash is busy. Nested/renewed leases are refused. |
| FAILED | Failure is latched. Late task progress cannot resume refreshing. |

Task progress is published only after a complete owner iteration. No new UART
packet, rendered frame, or connected Bluetooth peer is required. Owners are
registered once; repeated registration cannot reset a missed deadline. After
60s of healthy RUN checks, `HealthService_StableBoot()` fires once. Product
overrides this hook to request durable boot confirmation; this module performs
no storage writes.

The independent gate starts IWDG before C initialization with `/128` and reload
4095. Its small hardware-key routine is executed from its flash load address
before `.data`/`.RamFunc` copy, then from SRAM afterward. The linker must provide
`_sidata = LOADADDR(.data)` and `_sdata = ADDR(.data)` with `.RamFunc*` inside
`.data`. No initialized global, HAL clock or interrupt is assumed by the early
entry. Runtime initialization publishes diagnostics before a bounded hardware
start, so an LSI/configuration failure is recorded instead of appearing healthy.

STM32F429 datasheet17–47kHz LSI bounds give approximately11.16–30.84s hardware
timeout. Sleep calculations additionally use a conservative60kHz bound. Deep
STOP is capped500ms and requires a current supervisor sleep grant; the sleep
port neither feeds IWDG nor extends task deadlines. RTC elapsed time is added
back before resumed task checks. IO samples the recovery gesture even in deep
standby and wakes at most250ms apart. Retained Bluetooth uses peripheral-clocked
Sleep, not STOP.

The five-second flash lease exceeds the documented two-second maximum128KiB
sector erase at32-bit programming width. It is an absolute bound: busy polling
cannot keep a broken controller alive indefinitely. The metadata writer owns
its own lease; callers must not nest another one around `UpdateMetadata_Commit`.
RUN can enter FLASH only after a healthy check within250ms. A successfully
completed lease grants one checkpoint allowance for its measured duration;
it never changes the original phase deadline. The runtime rebases owner
deadlines once on completion, with no invented future HAL timestamps and no
clearing of failed owners. Every owner must subsequently make progress within
its ordinary deadline. Failed or expired flash work receives no such grace.

## Verification and limits

`tools/tests/watchdog_host/run.py` executes the actual Cortex-M4 BSP/policy at
O0/Os with MMIO models. It injects missed owners, frozen progress, timestamp
wrap, ISR calls, absent LSI readiness, long flash work and expired sleep grants.
It tests early startup against uninitialized RAM and audits the single raw
IWDG key writer. Metadata regression additionally verifies SRAM relocations;
power regression confirms the sleep path does not refresh IWDG.

These tests prove code policy and bounded paths, not real LSI frequency,
physical reset timing, current consumption, or wireless behavior. Device reset
and recovery trials remain separate evidence.
