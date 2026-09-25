# BT / ambient-only investigation, 2026-09-12

## Current installed firmware: UART CFW retaining full480 viewport

The later UART bring-up installed396928-byte APP SHA256
`51f4d63537148f30565fdc43e48552ba15ca732256a47317fe91dcf4decf96ab`.
See [UART evidence](../2026-09-12-uart-bringup/README.md). Automatic stock-style
A1/CMD01 and actual UART DMA are now verified against a PC peer. This does not
repair this donor's BT/ALS failures. The previous display installation follows
as historical evidence.

## Previous full480-only installation

The subsequent display request was implemented and installed APP-only in
[`../2026-09-12-display-full480`](../2026-09-12-display-full480/README.md).
Installation `../bringup-runs/2026-09-12-202707-574-Release` passed exact APP
readback, lower64KiB preservation and the stock-BL reset chain. The current
394276-byte Release APP SHA256 was
`67e08698c5174678844ccedcd616ba6278148e11f9772799f6f366f27bae9305`.
Before that installation, two full reads still matched the original stock
SHA256 below. This graphics change does not fix BT or the ambient sensor.

## Historical full-stock restoration and investigation

The user subsequently requested restoring the original **entire 512KiB**, including
the bootloader. [Full restoration record](original-full-restore-01/README.md)
documents a verified installation at `0x08000000..0x0807FFFF`. Two independent
full readbacks equal the original SHA256
`38207bed741a8fef9e6d2ed24b86c8cb8b376ba5e4c957163f5edd83fbeb8037`.
The previous CFW full image was backed up first. UID/options were unchanged;
the debug freeze setting was restored and the CPU was running after reset.
Stock remained installed during the observations below, until the subsequent
full480 display APP installation above. The earlier CFW results below are also
historical. Stock UI/BT/ambient success was not established by restoration alone.

Subsequent [original startup observation](stock-full-boot-probe-01/README.md)
confirmed stock HCI Reset wait returning-14 with CTS HIGH/TX NDTR3/RX NDTR256,
and ALS wrapper returning0x23. The user reports no Noodoe discovery entry on the
phone despite a pairing screen. The probe removed its hardware breakpoints,
restored debug freeze and resumed stock; it did not rewrite firmware.

Before restoration, `power-cycle-01` and `power-cycle-startup-01` recorded a
bench-only cycle with USB or debugger still attached. The later user-reported
all-cables-disconnected cycle is in `all-disconnected-cycle-01` and
`all-disconnected-startup-01`. Both exact-image reads of the 60b96374 CFW found
BT fault0x302/CTS HIGH/no HCI RX and ambient ARLO/no valid ID. No flash/reset
command was sent during those observations. The user reported completing the
disconnection procedure but did not supply a measured power-rail voltage.

The second cycle's read-only RCC values were BDCR=`0x3`, CSR=`0x0E000003`,
CFGR=`0x940A`: MCU LSEON/LSERDY were set, RTCSEL/RTCEN were clear.
The attach-only RTC driver returned NOT_READY before reading BKP19; its reported
backup19=0 is consequently not a measured backup-register value. This is a
separate cold-start RTC initialization gap, not evidence that BT's external
slow clock is absent. CFW was not modified before the requested stock restore.

This continuation is limited to Bluetooth HCI/SPP bring-up and OPT3001 ambient
sensor bring-up. A successful queue submission or host test is not a working
peripheral. The host alone owns ST-LINK; no GUI automation is used.

## Starting evidence

`before-change/diagnostics.json` independently verifies the live APP against
`2026-09-12-181155-055-Release`: SHA256
`7ca54df0825f028472bba6d1070449849bb9934c9be9e334c459a3303733d8c0`.
CPU was running before and after these reads. BT remained FAULT with no received
HCI bytes. Ambient had no valid lux reading.

Previous exact-image tests and A-B-A SDA bias evidence are retained in
`../2026-09-12-integrated-bringup/DEVICE_SERVICES_RESULT.md`. An internal weak
PC9 pull-up corrected the first transmitted HIGH at MCU IDR, but address 0x45
still NACKed. Restoring NOPULL reproduced the conflict. This does not locate a
broken trace, missing resistor, or damaged IC.

## Bounded changes under test

1. Match stock BT initialization ordering: known PA8 reset assertion and PI1
   enable precede the first USART1/MSP initialization. Record digital GPIO,
   UART status and DMA counts at bounded points in the existing reset timing.
   Final register equality did not establish equal initialization ordering.
2. Test only the four documented OPT3001 address straps 0x44..0x47, temporarily
   using the previously tested weak PC9 pull-up. Read manufacturer/device ID
   registers only after an address ACK. Restore all owned configuration and
   preserve the normal 0x45 driver configuration. This is not a general bus
   scanner, configuration write, or new power-pin experiment.

## 2017 public certification evidence

Downloaded primary applicant-submitted documents from the public FCC mirror;
the official FCC attachment endpoint returned HTTP403. Model attribution is
37130-LGC6 / 37140-AEB9, not the later Noodoe II certification.

* `fcc/internal-photos.pdf`: [FCC exhibit3613609](https://fccid.io/2AM4E37130-LGC6/Internal-Photos/Internal-Photos-3613609.pdf),
  SHA256 `3cbc9e812b0c0f118d62fc6ba533f4c2a2f89ded6b7dcb539bd09304434c37c1`,
  456284bytes. Page1 identifies the RF module physically on the Noodoe PCB;
  page2 shows its internal components. The chip marking is not reliably
  readable enough here to newly establish an exact silicon suffix.
* `fcc/module-installation.pdf`: [FCC exhibit3613615](https://fccid.io/2AM4E37130-LGC6/User-Manual/Manual-Module-Installation-3613615.pdf),
  SHA256 `0c74109887eed597a02292c4174bf13b7ad153c5eb910621e9012f4934b08e82`,
  206451bytes. Page2 places the wireless production test before joining the
  Noodoe assembly to the speedometer. The fixture wiring and supplied rails
  are not specified, so this does not prove any arbitrary bench wiring works.

Rendered pages and board crops are inspection aids only. They do not establish
the ambient sensor's reference designator, its physical net routing, or that
the user's PCB revision is identical. No confidential schematic was obtained.

Stock audit reconfirmed PC1 HIGH with no additional pulse, and PH13's pulse
belongs to an I2C1 error-recovery path. Neither is evidence of a shared BT/ALS
power control. No unknown power GPIO is changed by this investigation.

## Installed test and actual outcome

`image/` preserves the exact Release ELF/map/APP/manifest, APP392864bytes,
SHA256 `671e312c2d876655b533de729cab9634637a1a6ad1cf7b426fc69c5ca21e3ef5`.
Guarded APP-only installation `../bringup-runs/2026-09-12-185044-758-Release`
passed independent readback, original lower64KiB/options preservation, reset
chain and matching HAL/kernel tick progress (10434ms). No external NOR write,
format, firmware OTA or unknown power-pin pulse was requested.

### Bluetooth

`bt-startup-01/manifest.json` is an equal-even sequence snapshot after exact
APP/UID/running checks. PA8 LOW and PI1 HIGH were visible before the first UART
initialization. RX was LOW while reset was asserted and HIGH after release;
CTS was HIGH at every captured point. Requested release offsets1,2,5,10,20,50,
100,150ms were also the actual recorded HAL tick offsets in this run.

At approximately20seconds the initial HCI transaction timed out: fault0x302,
TX DMA3bytes remaining, RX DMA1byte remaining, no completed HCI receive. The
normal fault path then set PA8/PI1 LOW. The initialization ordering correction
alone did not recover BT. These are MCU-side digital samples: neither actual
module supply voltage nor continuity to the module pad has been measured.

### Ambient

`als-address-01/manifest.json`: request1223676976, operation2, committed seq2.
With temporary weak PC9 pull-up, every address44/45/46/47 returned an initial
address NACK; no ID register read was attempted. Per-address elapsed times were
656/656/772/655us; every STOP succeeded. Total2777us, maximum scheduling gap118us,
maximum SCL LOW29us, timing_uncertain0. Attemptedmask0xF, ACKmask0, IDmatchmask0.

All saved/final CR1,CR2,CCR,TRISE,FLTR and PH7/PC9 packed configurations match;
restore_result0. The CPU remained running. This rejects a simple change to one
of the four supported address straps as the sole explanation under these
conditions. It does not prove a physical part failure or verify usable lux.

`after-address-probe/diagnostics.json` again verifies the exact live APP. The
1004ms window had30frames (29.8FPS), estimated CPU60.7%, no missed frame slots,
IO stack2136bytes free and heap28232bytes free. These are short-window runtime
checks, not long-duration or hardware electrical qualification.

### Remaining physical evidence

The front-photo U8 is only an optical-package candidate (left edge, above the
BT antenna and left of the MCU). Its marking and net continuity are unreadable
in this photo, so it is not a confirmed OPT3001 identification. Before physical
measurement, verify the actual PCB and part marking/footprint. A complete power
removal differs from the MCU warm resets used here; a software reset does not
establish loss of all module supply rails. For an unambiguous cold-power test,
disconnect the external Noodoe supply, its USB and the debugger connection;
do not assume signal wires cannot feed an otherwise unpowered board.

## Offline validation before the last raw-radio experiment

The first combined image passed458 actual C assertions per O0/Os for the
ambient service/bitbang/address paths, and352 per O0/Os for BT transport.
Initial Debug compilation exceeded FLASH by2056bytes. The first CDT file-level
override was ignored because option instance superclasses were unresolved.
The corrected ST extension IDs produced actual `-Os` compilation of only
BSP_AmbientBitbang.c and BSP_AmbientAddress.c, leaving all other non-exempt BSP
sources at O0. Debug then passed at458440bytes (312bytes reserve). See
`build-debug-repaired-2.log`. This is a build result, not a hardware test of the
Debug image; the verified installed image remained the Release SHA above.

Further raw-BT diagnostic code is being validated separately. Its result must
not be inferred from these earlier artifacts.
