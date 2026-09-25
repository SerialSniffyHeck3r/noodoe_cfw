# Bounded stock APP / CFW comparison

Initial preparation used immutable firmware and saved observations. The root
integration task subsequently executed the authorized APP-only stock comparison
on2026-09-12, after the independent full NOR A/B backup. See
`Reversing/analysis/2026-09-12-integrated-bringup/STOCK_COMPARISON_RESULT.md`
and `stock-probe-04/events.tsv` for actual hardware evidence.

Stock ALS returned0x23 (HAL_TIMEOUT), with I2C3 SR1.ARLO=1. Stock HCI Reset
returned-14; host CTS remained HIGH, TX DMA had3of4bytes pending and RX had not
progressed. CFW was restored and running afterward; internal lower64KiB, UID
and option bytes were verified unchanged. These were MCU warm resets under
unchanged bench wiring, not full module power cycles. Stock can write NOR and
the full NOR contents have not been compared again after that execution.

## Exact image and restoration boundary

All addresses below apply to the actual original full backup A:

- `Reversing/VERY_IMPORTANT_ORIGINAL_NOODOE_BOOTLOARDER/noodoe_full_flash_dump_A.bin`
- 524288 bytes, SHA-256
  `38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
- Its APP slice at offset `0x10000`, length458748, is byte-for-byte identical
  to V516 OTA SHA-256
  `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`.

Use the original full backup's complete 448KiB APP slice for any temporary
restoration, including its last four bytes. The downloaded OTA is four bytes
shorter. The complete slice's SHA-256 is
`162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf`;
the final four bytes are `FF FF FF FF`.
The restore boundary is `0x08010000..0x0807FFFF`, sectors4..7.
Keep the current complete internal-flash backup, current CFW APP, option-byte
record, and verified independent full128MiB NOR A/B reads before that action.
Check the existing BL metadata does not request a pending installation; do
not silently replace it to make this experiment boot. BL/sectors0/1/3 and
option bytes remain unchanged; this comparison is not an OTA commit and does
not authorize changing sector2 either.

After stock execution, preserve any changed NOR contents for comparison before
restoring them. Re-read internal flash to distinguish stock-induced changes
from the intentional APP replacement. Restore the saved CFW APP and verify
the same reserved-region hashes. Any persistent differences are an explicit
result to review, not evidence that the stock boot was harmless.

## The first decisive experiment

The CFW currently cannot complete the first four-byte HCI Reset because host
CTS is HIGH. The smallest useful stock test is whether that same board can
receive a **valid successful Reset Command Complete** at bootstrap115200.
Finishing pairing, SPP or the stock patch is unnecessary to answer this.

1. On a stock boot without an earlier long halt, use an instruction hardware
   breakpoint at `0x08055B88`, immediately after the synchronous Reset call.
   Also arrange an ALS-result checkpoint at `0x0803255C`; the task scheduling
   order of these two checkpoints is not assumed.
2. At `0x08055B88`, save PC, R0, R4, SP, LR, GPIOA/C/H/I, USART1 controls,
   both DMA stream controls and the transport RAM listed below. Signed R0<0
   is a failed command operation; R0=0 is its successful return, after which
   the event status must still be checked.
3. On R0=0, continue only to `0x08055B92`. Here R0 is the returned event
   pointer. Save exactly six bytes from that RAM address. Expected layout:
   `0E 04 <command-credit> 03 0C <status>`. Status zero proves that the
   controller received and answered Reset. At `0x08055B94`, R2 contains this
   status byte, before it is stored through R4.
4. If the first breakpoint does not arrive, record the elapsed wait, halt once
   to locate PC, and inspect the live transport before calling the result a
   controller failure. Stock's command event wait uses5000 RTOS ticks at
   `0x08055146..0x0805514C`; there can be additional scheduling or send waits
   and retries. An arbitrary five-second host deadline is not a proof.

Reading the returned event is essential: an open transport, task heartbeat,
Reset submission or patch-entry breakpoint alone does not prove controller
communication. The valid stock Reset response with a failing CFW on the same
physical connection would rule out an unconditional dead UART/radio and direct
the next investigation toward CFW sequencing/clock/state differences.

### CLI breakpoint setup for that later experiment

The installed CLI is
`C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin/arm-none-eabi-gdb.exe`.
Its local `help hbreak`, `help thbreak` and `help dump binary memory` were
checked offline; no target connection was opened. The following are GDB
commands to use **after** the integration operator has separately restored
the stock APP, attached its selected GDB server and stopped before APP startup.
Connection port, reset/halt policy and watchdog handling remain with that
existing device workflow; they are not guessed here. Do not load CFW symbols
as though they described the stock APP.

```gdb
set pagination off
thbreak *0x0803255c
thbreak *0x08055b88
info breakpoints
```

Use hardware breakpoints (`thbreak`/`hbreak`), not software `break` instructions
that might cause a server to patch flash. The commands above only arm the two
checkpoints; the operator performs its controlled reset/resume separately.
At either stop, obtain the registers and identify the checkpoint:

```gdb
info registers pc r0 r1 r2 r4 r5 r6 sp lr
x/1ub 0x200232a0
x/1ub 0x200232f5
x/1wx 0x40011000
x/4wx 0x40011008
x/6wx 0x40026488
x/6wx 0x400264b8
```

At the ALS return, record signed R0 and the active byte, then continue to the
remaining HCI checkpoint. At the Reset return, signed R0<0 is a failure to
record before deciding any further resume. **Only if R0=0**, use:

```gdb
thbreak *0x08055b92
continue
info registers pc r0 r2 r4 sp
x/6bx $r0
```

Verify the PC and event layout before treating the bytes as a Reset response.
For a local binary artifact, run GDB from the new evidence directory and use
`dump binary memory reset-event.bin $r0 ($r0 + 6)` at this exact checkpoint.
This reads SRAM, not USART DR. Keep these scripts separate from the APP install
workflow: there is no `load`, flash erase, option-byte change or automatic
unbounded continuation in the proposed breakpoint setup.

## Transport setup checkpoints

Use these on a separate diagnostic pass if the first experiment fails or to
capture the exact register differences. Earlier halts alter timing and the
external controller continues running; do not compare a heavily stepped boot
as if it were an uninterrupted startup.

| Instruction address | What is true immediately before this instruction |
|---|---|
| `0x0802FD26` | Stock main's board setup call at`0x0802FD22` has returned; this is before its later task creation, not BT-ready. |
| `0x0803E82E` | PA8 was driven LOW by call at`0x0803E82A`. |
| `0x0803E840` | PI1 was driven HIGH by call at`0x0803E83C`; UART setup follows. |
| `0x0803E946` | RX DMA is armed, UART enabled, and the first10ms delay returned; PA8 is still LOW. |
| `0x0803E958` | PA8 HIGH call at`0x0803E954` returned; the150ms delay follows. |
| `0x0803E95E` | That150ms delay has returned, RX was armed before reset release. R6=1 is the transport-open success value; it is copied to R0 at`0x0803E96E`. |

USART1's HAL initializer briefly receives3686400 before the manual baud setter
uses the transport configuration at`0x0803E896`. The earlier stack hook
`0x080505FE` saves the requested eventual rate into`0x20023144`, then changes
the transport configuration to115200. Therefore the final BRR at the open
checkpoint, not the initial HAL handle baud member alone, is authoritative.

Stock common GPIO explicitly sets PC1 HIGH (`0x08037066`), PA8 LOW
(`0x08037002`) and PI1 LOW (`0x08036DBE`). The BT open sequence then changes
PA8/PI1 as above. No second PC1 pulse or MCU pin supplying the controller's
slow clock has been established. The original BL also sets PC1 HIGH, as the
offline `tools/boot-gpio-intent.json` proves. These facts do not justify an
unknown GPIO pulse as a diagnostic shortcut.

## ReadVersion and patch checkpoints

The stock order is **Reset completion → TI baud command → host baud change →
service pack → Read Local Version**. The caller at`0x08050AA6` invokes the
Reset wrapper, whose post-reset hook performs the baud/patch work; only on
success does`0x08050AD6` call ReadVersion. Therefore stock's normal
ReadVersion result is post-patch and must not be labelled an unpatched ROM
identity equivalent to CFW's pre-patch probe.

| Address | Meaning and values to preserve |
|---|---|
| `0x0805064C` | TI baud-command helper returned in R0; zero is success. Requested eventual baud is at`0x20023144`. |
| `0x08050682` | Host baud ioctl returned in R0; zero is success. |
| `0x080506A8` | About to call stock script sender: R0=stack handle, R1=`0x22F6`, R2=`0x0805C53C`. Reaching it proves earlier conditions, not completed patch. |
| `0x080506AC` | Script sender returned boolean in R0. Nonzero is its reported success; it is not an independent audit of every vendor response. |
| `0x080506F0` | Final post-reset hook result in R5; byte`0x200232F5` was written at`0x080506EA` on the normal attempted path. One means the hook reports completion, zero means incomplete/failure. This flag is also cleared by open/close hooks. |
| `0x08055C00` | ReadVersion synchronous operation returned R0; negative is failure. |
| `0x08055C0A` | ReadVersion event pointer is R0. Save14 bytes before they are released. |
| `0x08050ADA` | ReadVersion wrapper returned R0; zero means command status zero. |

ReadVersion event buffer has no H4 type byte:

| Byte offset | Width | Meaning |
|---|---|---|
| 0 | 1 | Event0x0E |
| 1 | 1 | Parameter length0x0C |
| 2 | 1 | Command credits |
| 3 | 2 | Opcode0x1001, little-endian |
| 5 | 1 | Controller status |
| 6 | 1 | HCI version |
| 7 | 2 | HCI revision, little-endian |
| 9 | 1 | LMP version |
| 10 | 2 | Manufacturer, little-endian; TI is13 in this stock caller's check |
| 12 | 2 | LMP subversion, little-endian, after the stock service pack |

The patch helper's return logic is not equivalent to verifying every
Command Complete status, so prefer the successful Reset and subsequent valid
ReadVersion response as the concrete communication evidence.

## ALS result checkpoints

Stock uses I2C3, PH7/PC9 AF4 open-drain,7-bit address0x45
(HAL8-bit argument0x8A),400kHz, register addresses of one byte and register
values of two bytes in big-endian wire order.

| Address | Meaning immediately before this instruction |
|---|---|
| `0x0803255C` | Entire ALS initialization wrapper returned R0. Zero means platform initialization, configuration and both ID checks succeeded. This checkpoint is reached even when an earlier config read failed. |
| `0x0803F936` | Sensor initializer returned R0. It reads and writes configuration register1 before the ID calls, so an address NAK can prevent ID breakpoints. |
| `0x0803F948` | ID-check function returned R0. Zero means both expected IDs matched. |
| `0x0804E30C` | First successful ID read has been loaded into R0; expected register0x7E value is0x5449. |
| `0x0804E32E` | Second successful ID read is in R0; expected register0x7F value is0x3001. |
| `0x080438CA` | HAL I2C memory-read returned R0. R5 retains the requested sensor register, R6 the two-byte output pointer, and R4 the address argument. Zero is successful HAL return; retain the output only on success. |

The byte at`0x200232A0` is set to1 after successful initialization at
`0x0803F968`. Read it alongside the wrapper return; a later zero alone is not
an error-code log. The original I2C handle is at`0x20022640`; its state getter
`0x0804D628` reads byte+0x3D. Save the whole0x50-byte RAM handle for later
analysis rather than applying offsets from the CFW's newer HAL typedef.

For a failing register read, stock's lower wrapper maps a nonzero HAL return
to return+0x20. A HAL-level failure does not itself identify address NAK versus
timeout; the saved peripheral status and lower-level history distinguish them.

## Register/RAM capture set and comparison limits

At a stopped checkpoint preserve core registers and these non-destructive
observations; do not run a debugger's automatic whole-peripheral register view.

- GPIOA/C/H/I configuration+IDR+ODR+AFR: offsets0x00..0x14 and0x20..0x24.
  PA8=reset, PI1=enable, PA9/10=TX/RX, PA11=host CTS, PA12=host RTS,
  PC1=stock HIGH output. BSRR is not needed.
- USART1 SR at`0x40011000`, and BRR/CR1/CR2/CR3 at`0x40011008..0x40011014`.
  **Omit DR at0x40011004:** reading it consumes receive data and the SR/DR
  sequence can clear status.
- DMA2 stream5 at`0x40026488` and stream7 at`0x400264B8`, six words each:
  CR/NDTR/PAR/M0AR/M1AR/FCR. Record DMA status flags without writing clear flags.
- RCC CR/PLLCFGR/CFGR, clock enables, BDCR and CSR. LSE-ready is an MCU fact,
  not proof of a Bluetooth clock waveform or controller supply voltage.
- Stock UART handle RAM`0x2002285C`,0x40 bytes. Hardware Instance is at+0;
  +0x30 and+0x34 point to the TX/RX DMA handles used by this transport.
- Stock transport RAM`0x2001B0E8`,0x1028 bytes, plus open flag word
  `0x20023164`, requested baud word`0x20023144`, and bytes`0x200232F4/F5`.
  RX circular storage begins at`0x2001B105` (+0x1D), spans0xC00 bytes;
  +0x14/+0x16 are RX indices and +0x1C is the receive-block active flag.
  The initial DMA block is0x100 bytes. Infer actual progress from DMA and
  changed data together; indices alone can remain unchanged before block/idle
  handling.
- I2C3 CR1/CR2/OAR1/CCR/TRISE and the stock handle RAM. Omit DR; avoid blindly
  reading SR1 followed by SR2 during a live address phase, because that can
  clear ADDR. At the completed ALS-wrapper checkpoint one SR1 sample plus
  the return code is enough for the initial failure comparison.

Saved CFW startup reference (`analysis/2026-09-12-integrated-bringup/bt-startup`):
USART BRR0x2D9, CR1=0x200C, CR2=0, CR3=0x3C1, SR=0; DMA TX NDTR=3 of4;
RX NDTR=1; PA8/PI1 HIGH, PA11 CTS HIGH, PA12 RTS LOW. The deliberate diagnostic
CTSE bypass later completed28 TX bytes and zero RX bytes. That bypass is not
the production policy. ALS's address0x8A transaction failed: the recorded
SR1=0x200 is ARLO, not AF/NACK. A generic HAL_ERROR does not establish NACK.

| Stock observation | Defensible next conclusion |
|---|---|
| Successful Reset while CFW still blocks at CTS | Radio/UART can operate on this board; compare the staged stock/CFW register and reset history. Focus before patch, not SPP. |
| Reset succeeds but stock baud/patch fails | Bootstrap transport works. Investigate the specific later command/rate/service-pack stage separately. |
| Stock also has CTS HIGH and no Reset response | The failure is not specific to BTstack. Compare actual stock/CFW rail/reset register states; then physical supply/clock/level-shifter measurements are warranted. It still does not prove a damaged radio. |
| Stock ALS IDs pass, CFW transaction fails | Sensor/bus can work. Compare the specific error, configuration, timing, pull-up/rail conditions and initialization ownership. |
| Stock ALS also fails, BT also fails | Two failures under original code strengthen a board/power/interconnect hypothesis, but do not identify a shared rail or justify guessing its control pin. |
| No stock checkpoint reached | First establish stock APP entry/task progress and preserved metadata compatibility. This is not a peripheral failure result. |

Use the same externally supplied power and wiring. An MCU system reset is not
a confirmed removal of power from the radio, RTC or sensor. If a true cold
power cycle cannot be performed, record that limitation; do not label the
comparison a cold-boot test. Short, reproducible checkpoint runs and a final
CFW replay on unchanged wiring give stronger evidence than repeated pin
guessing or arbitrary register changes.

## Reproduction evidence

`tools/stock-checkpoints.json` records exact raw instruction anchors and image
hashes. `tools/stock-checkpoints.asm.txt` contains disassembly of the bounded
functions above from the verified image, regenerated with the repository's
`analysis/2026-09-10-sleep-entry/mcu/inspect_mcu.py` helper. These are static
analysis artifacts, not a device execution log.
