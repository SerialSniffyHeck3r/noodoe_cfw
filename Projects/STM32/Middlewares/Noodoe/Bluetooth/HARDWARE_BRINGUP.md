# Bluetooth hardware bring-up evidence

The first integrated image built and ran on 2026-09-12, but Bluetooth did not
reach HCI identity. Do not describe the service as hardware-qualified yet.
The physical SWD operations were performed by the integration task; this note
decodes the saved files only.

## First startup: controller flow-control remains inactive

Evidence directory, relative to the Reversing root:
`analysis/2026-09-12-integrated-bringup/bt-startup`.

- `bsp.bin`: opened=1, baud=115200, TX busy=1, RX busy=1; zero completed blocks.
  These counters measure completion, so zero does not mean no submission.
- `dma-tx.bin`: DMA2 stream7 enabled, channel4, NDTR=3. The four-byte HCI Reset
  submission has placed its first byte in USART1 DR, with three still pending.
- `usart-sr.bin`: SR=0, including TXE=0 and TC=0. `usart-control.bin` has
  BRR=0x2D9, CR1=0x200C, CR2=0, CR3=0x3C1. These are enabled UART TX/RX,
  115200 baud at PCLK2=84 MHz, RTS/CTS and DMA.
- `gpioa.bin`: PA9..12 are AF7; PA8 is output HIGH. IDR=0xCF10 shows TX HIGH,
  RX HIGH, **host CTS/PA11 HIGH**, and host RTS/PA12 LOW.
- `gpioi-start.bin`, captured after another fresh reset before timeout:
  PI1 is output HIGH with HIGH observed at IDR too (ODR=0x302, IDR=0x3FE).

This establishes a pending UART transmission stopped by CTS. The controller
does not advertise readiness to receive. It is not evidence of an RFCOMM,
service-pack selection, CPU scheduling, or USART completion-IRQ failure.

The separate postfault rail snapshots are later in time:

- `gpioc.bin`: PC1 is already push-pull output HIGH (MODER=0x0409F004,
  ODR=0x2002, IDR=0x3E3F). Omitting MX_GPIO_Init did not leave PC1 LOW.
- `rcc.bin`: BDCR=0x8103; LSE enabled/ready and selected for RTC. This proves
  the MCU LSE state, **not** the Bluetooth controller's slow-clock waveform.
- `gpioi.bin`: PI1 LOW is expected after the timeout closes the transport;
  it must not be confused with `gpioi-start.bin`.

## Controlled CTS bypass: transmitted Reset, no response

The integration task transiently cleared only CTSE in the observed USART1 CR3
(0x3C1 to 0x1C1) while a standard HCI Reset was pending. This is a diagnostic
experiment, not the production flow-control policy.

`bsp-after-bypass.bin` records 7 completed blocks / 28 transmitted bytes,
zero received blocks/bytes and zero UART errors. `bt-after-bypass.bin` still
has no manufacturer, HCI revision or LMP subversion. `gpioa-idr-bypass.bin`
remains 0xCF10, including RX and CTS HIGH. Thus bypassing CTS releases the MCU
TX path but produces no controller response. The normal 20-second startup
deadline closes BT with service error 0x302.

## Stock initialization comparison

Original V516 APP (`3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`):

The actual original full backup A has SHA-256
`38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
Its 458748-byte APP slice at offset 0x10000 is byte-for-byte identical to this
V516 download. A different downloaded firmware version does not explain the
present startup mismatch.

1. PA8 LOW at 0x0803E82A.
2. PI1 HIGH at 0x0803E83C.
3. USART1/RX transport setup, bootstrap 115200.
4. Delay 10 at 0x0803E940, PA8 HIGH at 0x0803E954, delay 150 at 0x0803E958.

The delay wrapper 0x080483C8 divides by literal one before calling the RTOS
delay. The existing clock audit supplies the millisecond timebase.

The stock common GPIO routine initializes PC1 as output and writes HIGH at
0x08037054/0x08037066. Examination of the direct HAL_GPIO_WritePin call sites
in `analysis/2026-09-11-bootloader-re/app-bt-update/app-all.asm.txt` finds this
single PC1 write; no second PC1 pulse has been established. No MCU output has
been established as the Bluetooth 32.768 kHz clock source. A ready MCU LSE is
insufficient evidence to claim that physical clock is reaching the controller.

The first image had one transport-order difference: stock arms its receive
ring before releasing PA8; that image armed its first H4 receive after
BSP_BT_HCI_Open returned, holding host RTS HIGH during the reset delay.
The subsequent source correction keeps PA8 LOW until the first H4 receive
arms DMA and releases RTS, then yields 10 ms, raises PA8 and yields 150 ms.
H4's open path calls this first receive before returning or sending its first
command. Production-C ARM tests at O0/Os validate that ordering. The corrected
APP `54c6643656ce5d48619135d5b1870a88fc6fa9c4975db0153e3da97c97bde8ca`
reaches the integrated runtime/display (29.8 FPS in the later snapshot), but
`probe-hud-fixed/diagnostics.json` again records error 0x302, one reset release
and zero completed TX/RX blocks. The correction did not resolve controller
readiness. That later snapshot is after transport close and does not supply
a new pre-timeout CTS measurement.

`tools/audit_boot_gpio.py` executes only the original BL common GPIO routine
0x200002B0 in an offline ARM emulator, with the previously measured revision6
strap inputs. `tools/boot-gpio-intent.json` records the actual HAL call intents:
PC1 HIGH (0x20000902), PI1 LOW (0x2000070E), PA8 HIGH (0x2000089E), PE3 LOW,
PD13 HIGH, PG14 LOW and PI9 HIGH. Thus inherited PC1 HIGH has a verified BL
source. This emulation does not establish physical controller supply/clock.
The stock ALS platform initializer 0x080437DC initializes I2C3 through HAL;
there is no separate GPIO enable in that bounded call path. A shared external
power circuit remains a hardware hypothesis, not an identified software pin.

TI's [CC256x testing guide](https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/968/CC256x-Testing-Guide-_2D00_-Texas-Instruments-Wiki.pdf)
states that controller HCI_RTS LOW indicates successful power-up. Relevant
conditions are stable supplies, reset release and the slow/fast clocks. The
[CC256x datasheet](https://www.ti.com/lit/ds/symlink/cc2564.pdf) specifies initial
115200 baud and selection of H4/H5 from the first command, rather than a
documented CTS-at-reset strap.

The next discriminating checks are a stock APP startup on the same physical
board and, if it too fails, physical reset/enable, controller supply/clock and
UART-level-shifter measurements. The existing evidence does not justify
declaring the Bluetooth chip damaged or toggling an unknown GPIO as a fix.

## Re-audit: startup ownership and digital timeline

The later actual `stock-probe-04` reached both checkpoints: stock HCI Reset
timed out with TX NDTR3 and host CTS HIGH. Its final UART/GPIO configuration
matches the selected CFW observations. This narrows the fault but does not
prove physical damage or identical initialization history.

A remaining CFW order difference was found in `NoodoeRuntime_Start`: it called
`MX_USART1_UART_Init` before the storage worker started Bluetooth. Thus UART
TX/RTS configuration preceded PA8 LOW/PI1 HIGH. Stock's first UART/MSP init at
`0x0803E88A` follows those GPIO writes at `0x0803E82A/0x0803E83C`. TI section
5.7.1.2 prohibits driving non-fail-safe HCI pins without VDD_IO; the physical
relationship of PI1 to that supply is unknown, so back-power is a hypothesis.

The bounded correction removes the Runtime UART call. BSP Open owns the
first HAL/MSP initialization, establishes PA8's output mux with a preloaded
LOW before raising PI1, then initializes UART. It checks inherited DMA EN
before reusing hardware, never aborts a zero handle, and retains the existing
abort quarantine and USART1-only reset for later opens. Pre-open baud/flow/
quiescence operations reject safely; pause/close do not change GPIO before
the driver owns a UART handle. This change has not itself demonstrated a
working radio; that requires the next actual image measurement.

`g_bsp_bt_hci_startup` is a separate 472-byte ABI defined by `BSP_BT_HCI.h`.
The six-word header contains magic `0x42545431`, version1, publication
sequence, Open attempt number, sample count and flags. Each of16 samples has
seven words: HAL tick, stage, GPIOA IDR, GPIOI IDR, USART SR, RX NDTR, TX NDTR.
Only entries below sample count belong to the current attempt. Readers must
verify an unchanged even sequence. Startup, fault and close publications
share that rule; neither USART DR nor status-clear registers are read/written
by sampling.

Stages1–6 mark entry, reset LOW, enable HIGH, UART ready, RX armed and reset
release. Stage7 observes requested release offsets1/2/5/10/20/50/100/150ms;
stage8 records fault and stage9 records shutdown GPIO completion. Flags0/1/2
mean the150ms settle completed / fault recorded / Close reached its GPIO
writes. These are flag bit indices. Absolute RTOS deadlines retain the
original150ms interval without accumulating each sampling delay. The actual
HAL tick timestamps may be late or repeated if scheduling is delayed. These
are digital samples, not edge timing, analog voltage or clock-waveform proof.

Actual production C was exercised at O0 and Os with352 assertions each,
including first Open from a zero handle, inherited non-output mux, restart,
pending DMA quarantine, pre-open calls, coherent trace publication and a
30ms delayed sample that does not extend the final150ms deadline. See
`tools/tests/bt_transport_host/output/results.json` for the tested hashes.
The hardware remains owned by the integration task; no device was accessed
by this audit or host-test run.
