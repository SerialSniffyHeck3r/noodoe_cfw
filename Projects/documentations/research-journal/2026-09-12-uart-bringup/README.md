# UART bench bring-up — 2026-09-12

## Current installed state: automatic stock-compatible UART CFW

Latest APP `image/app.bin`: **396928bytes**, SHA256
`51f4d63537148f30565fdc43e48552ba15ca732256a47317fe91dcf4decf96ab`.
Install `../bringup-runs/2026-09-12-214054-603-Release` passed exact readback,
lower64KiB preservation, original BL reset chain, HAL/RTOS progress and graphics.
Release spare61824bytes; Debug APP458044bytes, spare708bytes. Both Cube builds
pass, with the existing RWX LOAD-segment warning. Core task remains LCDTest().

`compat-02/result.json` is **PASS** on the real UART5 ISR/ring/parser/service/TX
DMA and COM11 USB bridge. This supersedes the earlier receive-only state below.

| Test | Actual evidence |
|---|---|
| Host→MCU bytes |9144 sent,9144 received in settled interval|
| Valid RX |722 XOR-valid frames;692 accepted for stock link policy|
| UART/overflow |0 new UART errors,0 overflows; cumulative earlier errors remain2|
| Corruption |exactly1 intentional XOR rejection|
| Reply cadence |24 recorded CMD21 frames produced6 A1 index0 replies|
| Receive gap |unknown/empty frames ignored for link; request at1.219s after last good frame|
| Fragmentation |4 split frames recovered, one A1; ODO36475, synthetic speed73, fuel1|
| Light |all10 indices received on PC; AUTO returned to failed-sensor initial0|
| TX completion |175 additional TC frames:169 A1,5 request,1 stop; DMA failures0|
| Stop/restart |CMD01=00, TE off/RX alive, then CMD01=04 and A1 resume|
| Silence |link down/stale, last ODO retained|
| Display |actual EVE capture below, persistent UART line, result page2 telemetry|
| Performance |continuous/final windows29.9fps, CPU61.7/64.4%; missed frame slots0|

The PC source is paced around100ms; A1 spacing here406–407ms reflects those
actual host intervals, rather than a hardcoded400ms transmitter. Stock donor
reference `stock-reference-01` independently produced the same A1 index0 and
CMD01 request bytes, with a four-frame cadence and400+800ms retry policy. Its
failed ALS UART cache at0x20023308 was actually0. Only stock APP was temporarily
installed for that reference; BL/lower data were verified preserved.

`compat-01` is a **test-harness failure**, preserved: six correct A1 frames were
received, but a preceding synchronization request shared the coarse Windows
timestamp and was included in the test interval. `compat-02` slices the RX list
after that request; no firmware change or reflash was needed for this fix.

![Actual EVE UART page](compat-02/capture/display.png)

The center arc is still the existing labeled DEMO SPEED test. The ODO/SPD/F1
row is the actual parsed COM11 input. Screenshot is EVE raster, not a photograph
of the LCD glass. Synthetic speed73 is a test value, not actual vehicle motion.

Source is split into `BSP_Dash`, `Dash_Protocol`, `DashService`, and the existing
`Vehicle_Service` parser. See the project `Services/Vehicle/README.md` for API,
ownership, failure and cached-sensor contracts. Host ARM C tests passed at O0
and Os: UART service93, BSP IRQ86 and protocol-services12395 assertions each.
Four CDT-policy tests also passed; file-specific optimizations are preserved
by sync_project/services_build, without vendor or non-USER Core edits.

COM11 is closed after the test. UART service remains enabled, AUTO light policy
restored; without a peer the screen shows WAIT/RETRY and stale vehicle data.
Physical dashboard hookup/acceptance and real sensor-driven brightness remain
separate checks. The failing donor ALS has not been repaired; no lux or real
dashboard brightness success follows from the diagnostic0..9 override.

## Historical wiring and receive-only investigation

The user connected an Uno with its ATmega328P removed, leaving the USB UART
bridge, and asked to identify the unknown TX/RX orientation. They explicitly
confirmed that only Uno and Noodoe UART are connected; dashboard UART is
disconnected. No claim about a live vehicle connection follows from this test.

## Identified host port

- COM11: Arduino Uno USB VID2341/PID0043, serial[redacted USB-UART serial].
- COM3: Intel Active Management Technology SOL; not the Uno.
- Baseline is the unchanged full480 CFW APP SHA256
  `67e08698c5174678844ccedcd616ba6278148e11f9772799f6f366f27bae9305`.
  `wiring-01` matched the complete live APP before using diagnostic symbols.
- UART5 CR1=0x2024: enabled RX/interrupt, TE disabled. PC12 GPIO mode is input,
  PD2 alternate input. `g_bsp_dash` ready1, tx_enabled0. No MCU register writes,
  CPU halt/reset, flash, options or NOR operations were performed.

## Initial wiring is reversed on the PC transmit lead

`wiring-01` sent64 bytes of0x55 at1152008N1 and observed MCU RX delta0, no
errors. Passive PC input was empty; current CFW intentionally does not transmit
automatically. CDC BREAK did not change either pin, which is inconclusive for
this Uno bridge firmware and is not treated as a wiring test failure.

`slow-tx-01` then sent bounded0x00 data at300baud so the PC TX level could be
observed by live SWD. UART5's own baud/configuration remained unchanged:

| Observation | PC12 (Noodoe TX connector signal) | PD2 (Noodoe RX) |
|---|---:|---:|
| Before transmission |1|0|
| Four samples during slow PC transmission |0|0|
| After transmission |1|0|

Thus PC TX reaches Noodoe's **TX** pin rather than RX. The user was asked to
swap the two signal leads while leaving GND connected. MCU TX remains disabled
so this finding did not involve two active MCU/USB transmitters being driven
against each other. Port was returned to115200 and closed after the probe.
The reverse receive wire has not yet been independently demonstrated.

## Corrected wiring and physical RX/parser: PASS

The user confirmed swapping the two leads. `wiring-02` saw66 received bytes
and2 UART errors against a64-byte test; that attempt did not prove clean
transport. The baseline had been taken before opening/reconfiguring COM11,
so the extra activity cannot be uniquely assigned to the test payload.

`wiring-03` took its baseline after port-open settling, then sent64 bytes:
**MCU RX delta64, UART error delta0**. PC TX now reaches Noodoe PD2 RX.

`replay-01` used the exact archived ELF/APP to locate the running C parser and
the published `vehicle_snapshot` copied by `NoodoeRuntime_GetVehicle`:

- Recorded fuel0 and fuel1 frames: ten each, ODO36475km and0/1 observed bars.
- Explicit synthetic speed73 variant: ten frames, correct speed/ODO/fuel.
- One corrupt-checksum frame followed by a valid fuel1 frame: corrupt rejected,
  valid frame recovered. This intentional XOR error is not a UART framing error.
- After stopping input: stale1, valid_fields0, previous ODO retained.
- Totals: **416 host bytes =416 MCU bytes; zero UART errors;31 valid frames;
  exactly1 expected checksum rejection.** No register/memory writes or reset
  were used for this test. Serial frames changed volatile telemetry only.

The actual dashboard remains disconnected. These tests establish the physical
USB-UART→MCU→ISR/ring→C parser→published snapshot path, not live dashboard
electrical behavior or the meaning of unconfirmed payload fields. SWD snapshots
are non-atomic live reads; stable payload cases are observed after stopping TX.

## Reverse physical TX: PASS; firmware DMA/automatic replies still pending

`return-03` temporarily enabled only UART5 TE and PC12 alternate mode through
their peripheral bit-band aliases. Five UART DR bytes were independently
received on COM11, **55 A6 3C 00 FF**, exactly matching transmission. TX idle/TC
was checked; both temporary bits were restored, PC12 returned to input,
`tx_enabled` remained0, and CPU was running. No flash, reset or option changes.

`return-01` and `return-02` preserve earlier tool failures: Cube's default
write/readback check is unsuitable for UART DR, and a preceding `-nv` did not
disable that check. The successful local CLI syntax was
`-w32 0x40005004 <byte> -nv`; correctness was checked against the independent
serial receiver rather than a read of DR (which is the RX register). Both
failed attempts also restored the temporary TX bits.

This register probe proves the second wire, UART TX output and PC receiver.
It does **not** execute `BSP_Dash_Send`/TX DMA or an automatic A1/CMD01 service.
Those remain separate bring-up work, as does actual dashboard acceptance.
Current firmware is unchanged full480 Release, default receive-only; COM11 is
closed and available for the next experiment.

The final cumulative UART diagnostics report556 RX bytes,7 errors and5 RX
restarts across all port openings, slow-data tests and temporary TX probes.
The zero-error result above applies to the settled416-byte replay interval,
not the whole session. Port-open/line-transition transients have not been
separately characterized; do not label these cumulative errors as payload
loss or claim their electrical cause has been proved.

## Stock ambient reply audit

Existing V516 reverse engineering and historical capture identify Noodoe→Dash
`F5 A1 02 01 <0..9 index> <whole-frame XOR>`, e.g. `F5 A1 02 01 07 50`.
The index is not raw lux or percent. Current CFW has no automatic A1/CMD01
transmission. A reply cadence tied to received frames is supported by the V516
analysis but must not be equated to a fixed400ms periodic command solely from
two separately recorded serial directions. Follow-up donor-full evidence belongs
in [`stock-audit/README.md`](stock-audit/README.md) and does not itself prove
physical dashboard acceptance. That audit found the donor's entire458748-byte
APP identical to V516 and decoded the actual lower-flash threshold table, so
the V516 static analysis applies to this donor's stock APP.
