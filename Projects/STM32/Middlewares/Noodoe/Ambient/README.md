# Ambient service

`AmbientService.h` is the upper API. The storage worker calls `Init()` once and
`Process(now_ms)` regularly. Init only queues the default400kHz probe; it does
not access the bus. Do not also call BSP_Ambient directly from another task.

`RequestProbe(80000|100000|400000,&id)` and `RequestEnabled(0|1,&id)` immediately copy
into one slot. ACCEPTED means queued. Read `completed_id/completed_result` from
`GetSnapshot()` to distinguish completion from admission. The slot stays BUSY
through the entire HAL operation. Snapshot reads never initiate bus activity;
use `driver.valid`, `stale`, and `sample_age_ms` with millilux. Raw SWD snapshots
also contain age/stale evaluated at publication time; GetSnapshot refreshes age
at read time. Disable requests
remember intent through a later probe; probe normally enables continuous
measurement,then disables it again if that intent remains off.

A failed probe/configuration or failed transport during polling schedules
recovery after1/4/16seconds,then stops. Explicit requests renew that budget.
Automatic retries update live device state but do not rewrite the completion
result of the original operation. An invalid optical sample is reported without
resetting an otherwise responsive bus. There is no automatic power cycle,
GPIO pull/pulse,storage operation or restart.

The upper API is asynchronous. The BSP remains synchronous in this intermediate
implementation:20ms HAL transfer timeout and the installed HAL's separate25ms
initial BUSY timeout,plus tick granularity and scheduling delay. A successful
probe uses three sensor transfers (four when disabling afterwards). These calls
run only in the low-priority storage worker and can delay that worker's other
services. I2C IRQ-driven transfer/cancellation is not claimed. IOC still selects
400kHz; explicit80/100kHz probing is a reversible runtime diagnostic on I2C3 only.
80kHz avoids the88-100kHz repeated-START setup limitation described in ST
ES0206 §2.10.4; this does not identify that erratum as the cause of observed ARLO.
HAL's generated MSP retains PH7/PC9 AF4 open-drain/no-pull configuration.

The original BSP diagnostic48-byte prefix is preserved. Appended phase,
HAL status,HAL ErrorCode,SR1,elapsed milliseconds,bus frequency,and confirmed
enable state separate transport evidence from generic failure. `SR1=0x200`
means arbitration loss; `0x400` is acknowledgement failure. Polling HAL may
record TIMEOUT while hardware still records ARLO. No diagnostic SR2 read is
performed. SR1 after HAL deinitialization may be zero because its clock is off;
the failed transaction's phase/status/register observation is the useful record.

`g_ambient_mailbox` exposes explicit SWD diagnostics. Its exact124-byte layout,
allowed commands,and commit ordering are documented in the header. The host
writes command/argument then request_seq last,waits for matching response_seq,
and verifies that response_seq is unchanged around its full read. Results and
driver evidence remain frozen across automatic retries until the next response.
Only one host request may be outstanding. Unknown commands/arguments are
rejected. This mailbox is neither a raw-register interface nor an arbitrary
memory-access interface.

`tools/tests/ambient_host/run.py` exercises actual BSP and service C on emulated
Cortex-M4 with the installed HAL/CMSIS headers. O0/Os143 service assertions cover every
probe failure phase,ARLO versus AF,clock choices,write-failure semantics,validity,
queue reentry,finite retries,snapshot staleness,and mailbox completion ordering.
These results establish software behavior,not successful sensor communication.
Actual stock and CFW both previously reported ARLO; live80/100/400kHz results are
separate hardware evidence owned by the integration procedure.

## Explicit ID-only wire diagnostic

`AmbientService_RequestIDDiagnostic(&id)` queues one measurement.
`AmbientService_GetIDDiagnosticSnapshot(&out)` returns1 only for a stable
completed record; it returns0 without changing output while none/running.
These upper calls never touch GPIO/I2C. SWD uses the existing124-byte mailbox
command3/argument0 and separate `g_bsp_ambient_bitbang` record (version3:212bytes;
older version2:208bytes, version1:184bytes).
Match its operation_id/request_seq and identical even sequence before/after
reading. `BSP_AmbientBitbang.h` defines every field and result value. The normal
HAL driver snapshot is previous HAL evidence, not this diagnostic's result.

The worker temporarily disables I2C3 PE and changes only PH7/PC9 MODE to
open-drain output with existing no-pull configuration. It reads fixed address
0x45 manufacturer0x7E/device0x7F as MSB-first16-bit words. Register-pointer
writes are part of these reads; no sensor configuration/data write, scan,
bus-clear train, supply control or MFi access exists. Both original pin fields
and I2C timing/PE are checked before/after. Other GPIO pin changes are preserved.
On restoration mismatch PE remains off and the HAL handle remains locked;
the service stops automatic work and rejects enable/diagnostic requests as
BUSY until an explicit normal probe succeeds. The diagnostic is never retried
automatically and otherwise retains normal speed/enable/retry policy.

Nominal20kHz uses25us DWT half periods with interrupts enabled, following the
[OPT3001 datasheet's](https://www.ti.com/lit/ds/symlink/opt3001.pdf) minimum10kHz
and28ms SCL-low reset limits. GPIO readback observes real ACKs and HIGH/LOW;
it is not inferred from ODR. A gap>10ms or total duration>25ms aborts and marks
timing_uncertain. DWT/tick observations record max_gap_us/max_low_us, including
a35ms preemption test. Firmware cannot release a wire while it is not scheduled;
max_low_us>=28000 explicitly permits sensor timeout. timing_uncertain=0 is not
an oscilloscope measurement of20kHz or a proof of electrical signal integrity.
The one-shot synchronous worker may pause that worker for the transaction.

The additional162 actual-C assertions per O0/Os use a separate edge-decoding
I2C endpoint model. They cover the exact transmitted byte allowlist, both IDs,
six independent ACK failures, SDA conflict, idle/clock stuck states, settling,
each STOP failure, long LOW/preemption, stopped DWT, reentry, unrelated GPIO
preservation and failed-restore PE/lock handling. These are software tests;
actual ID/ACK recovery requires the separately recorded hardware experiment.

Version2 appends the first address bit's line samples and DWT-relative
microsecond timestamps: before SCL rises, just after SCL reads HIGH, and at
the end of the25us HIGH observation after a detected conflict. A zero timestamp
means that observation was not reached. These samples narrow the digital
transition behavior, not analog voltage/ringing or the failing component.
An early LOW followed by late HIGH stays SDA_CONFLICT: accepting that bit
would ignore a possible STOP condition while SCL is HIGH. The diagnostic
still sends no further byte after that conflict and restores its configuration.

The first live ID diagnostic observed SDA LOW on address bit0 after25us setup.
It also exposed a separate restore-check bug: RM0090 §27.6.1 specifies that
PE=0 automatically clears ACK/POS (also START/PEC/ALERT). Treating CR1 as plain
RAM in the original fixture missed this. The corrected driver compares the
documented PE-off state, restores PE before saved ACK/POS, and still verifies
the complete saved CR1. PEC/ALERT requests are rejected before takeover, not
replayed. Tests model these hardware side effects and saved ACK/POS0/1, while
retaining foreign CR1/CR2/CCR mismatch quarantine. This fixes the false restore
failure; it does not explain or repair the independently observed SDA LOW.

Version3 also offers `AmbientService_RequestIDDiagnosticWithPullup(&id)`.
This explicit diagnostic (SWD cmd3/argument1) temporarily enables only PC9's
internal weak pull-up while reading the same fixed IDs. PH7 stays NOPULL.
The original PC9 pull bits and all other owned configuration are restored on
every takeover exit; other pins' pull fields are preserved. Initial prechecks
still require the original AF4/OD/NOPULL setup. A foreign pull/configuration
mismatch remains a quarantine failure even if the owned bits can be restored.
Normal probes, sampling and automatic retries never inherit this pull-up.

The appended `pullup_mode` is the requested variant, including precheck
rejections; `pin_changes` and `restore_result` distinguish actual takeover
and restoration. Success in this additional-bias experiment proves only that
the ID transaction worked under that condition, not normal lux sampling,
the original board's pull-up integrity, or compliance with analog I2C timing.
Tests cover mode1-to-mode0 isolation, responsive and still-LOW wires under
pull-up, clock/NACK/timing failures, foreign pull changes and restoration.
