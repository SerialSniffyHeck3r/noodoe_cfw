# Noodoe custom firmware bootloader strategy

> **2026-09-09 재검증:** SR1 CRC 재현, 하위 flash의 16KiB shadow/4KiB program 구분, UART5 byte-IRQ 수신 및 USART1 baud 재설정에 대한 정정은 [최신 BIN·APK 대조 보고서](2026-09-09-ak550-firmware-uart-boot-review.md)를 우선한다. 아래는 당시 분석 기록이며, 충돌하는 설명을 실물 작업의 확정 근거로 사용하지 않는다.

Date: 2026-09-01 KST, updated 2026-09-02 KST

This note answers one specific question: can an Android app update a custom
Noodoe firmware over Bluetooth, and which bootloader path should it use?

## Current conclusion

Yes, Bluetooth firmware delivery is a realistic goal, but the target should be
the **Noodoe/Sunray SPP OTA path**, not the raw STM32 system-memory ROM
bootloader.

There are two different things that are easy to mix up:

| Name | Where it lives | Speaks Bluetooth? | Current usefulness |
| --- | --- | --- | --- |
| STM32 system-memory bootloader | ST factory ROM around system memory | No direct Bluetooth/SPP support | Good for bench recovery through physical USART/USB/I2C/SPI/CAN paths if BOOT0/boot selection is reachable |
| Noodoe/Sunray boot/update path | Noodoe flash/application/update state machine | Yes, through the running Noodoe Bluetooth stack and SPP file transfer | Correct target for an Android updater app |

The archived SR1.5 image starts at `0x08010000`. The lower flash area
`0x08000000..0x0800FFFF` is not inside the archived OTA application image.
Assembly now proves that the running application can preserve-update internal
flash sectors 2 and 3 at `0x08008000..0x0800FFFF`. This makes a layout with boot
code below `0x08008000` and persistent configuration/update state in sectors 2/3
a strong hypothesis, not yet a confirmed partition map. Dump the entire lower
area before any write experiment.

## Why the STM32 ROM bootloader is not the Bluetooth updater

ST AN2606 describes the STM32 system-memory bootloader as a factory programmed
ROM bootloader whose job is to download an application through selected serial
peripherals such as USART, CAN, USB, I2C, SPI, and related interfaces.

For STM32F42xxx/43xxx, AN2606 lists bootloader interfaces including USART1,
USART3, CAN2, I2C1/2/3, SPI1/2/4, and USB OTG FS DFU, depending on bootloader
version and boot selection.

That bootloader does not initialize the Noodoe application, Bluetopia, SPP, or
the TI CC256x service-pack flow. Once the MCU is in ST ROM bootloader mode, the
normal Noodoe application stack is gone. The phone cannot simply keep an RFCOMM
socket open and talk to ST ROM over Bluetooth.

Using ST ROM bootloader from Android would require an extra bridge that is not
present in the ROM path, for example:

- external hardware wired to a ROM-supported physical interface, or
- a resident Noodoe-side loader that receives Bluetooth packets first, then
  writes flash itself.

The second option is no longer "using the ROM bootloader"; it is a custom or
Noodoe-owned OTA loader.

## What the Android app should reproduce

For the current AK550 evidence, the practical path is `VERSION_1_5` modern SPP
OTA:

- device generation: `VERSION_1_5`
- transport: Classic SPP/RFCOMM
- firmware location: `0x0800`
- transfer type: file
- negotiate command: `0x0A`
- start/control command: `0x0B`
- data command: `0x0D`
- data chunk size: 11816 bytes
- chunk progress gate in the official Android app: sequence send success
- meter feedback: a real 16-byte `0x0D` reply with accepted chunk and cumulative
  byte counts, confirmed by V5.16 assembly and a live general-file transfer
- resume source: `0x0B START` reply `receivedLength`
- keepalive: periodic `CONTINUE`
- final install proof: post-reboot DeviceInfo/version read, not just transfer
  DONE

This means a custom Android updater can exist, but it must behave like the
official SPP updater and preserve the recovery behavior: generation check,
series check, official archive manifest binding, CRC/trailer handling,
resume, cancel/reset, timeout, and power-loss rules.

## What Revision 1.1 changed

The v1.1 BSP report moves the project from "the MCU seems to use these buses"
to "we can begin writing a clean-room BSP contract."

Confirmed or strongly supported hardware contracts:

| Block | Bus/peripheral | Evidence/use |
| --- | --- | --- |
| Main MCU | STM32F429xx-class Cortex-M4F | vector table, memory map, IRQ order, ST HAL register references |
| RTOS | FreeRTOS + CMSIS-RTOS v1 | SVC/PendSV/SysTick flow, task names, heap/TCB behavior |
| Vehicle link | UART5, PC12/PD2, 115200 8N1, DMA1 S0/S7 | F5 framed telemetry and command bridge |
| Bluetooth controller | USART1 PA9/PA10/PA11/PA12, 3.6864 Mbps, RTS/CTS, DMA2 S5/S7 | TI CC256x HCI/service-pack path candidate |
| Resource storage | SPI5 PF6/PF7/PF8/PF9 | Macronix `C2 20 1B`, 128 MiB NOR geometry |
| Graphics engine | SPI1 host interface | FT81x EVE register fingerprint, command FIFO |
| LCD control plane | SPI4 + PE4 control/CS | MIPI-DCS-like commands: sleep out, display on/off, power mode |
| Ambient light | I2C3 PH7/PC9, addr `0x45` | TI OPT3001 IDs `0x5449` and `0x3001` |
| MFi AuthCP | I2C1 PB6/PB7, addr `0x11`, PH13 reset | authentication coprocessor path |
| Backlight | TIM5 PWM candidate | brightness ramp and ambient-light consumer |
| USB | OTG_HS/FS vectors and register refs | update/debug/recovery candidate; physical mode still needs closure |

So yes: v1.1 is effectively a BSP reconstruction report plus internal protocol
research. It is not yet a complete custom firmware, but it is enough to define
driver modules and bring-up order.

## Knowledge still needed before Bluetooth flashing custom firmware

The missing pieces are bootloader and image-policy details, not the basic SPP
transport.

Needed before writing a custom image to a real AK550:

1. Full internal flash dump, especially `0x08000000..0x0800FFFF`.
2. Full external NOR dump before any resource or firmware experiment.
3. SR1.x trailer/checksum/signature algorithm, including the final
   `0x70067874` word in the archived V5.16 image.
4. Whether the Noodoe first-stage bootloader validates only size/CRC/version or
   uses a stronger signature.
5. Exact install sequence after SPP transfer: where the image is staged, when
   flash erase happens, which code executes the write, and what reboot state
   means success.
6. Rollback/recovery behavior if image validation fails or power dies mid-write.
7. Whether BOOT0, NRST, SWDIO/SWCLK, and USB pins are physically reachable.
8. RDP/write-protection/option-byte state from read-only SWD/CubeProgrammer
   inspection.
9. Exact FT81x variant, LCD DDIC, SDRAM geometry, and backlight PWM channel for
   a full replacement firmware.

## Recommended updater architecture

The Android updater should be staged in three modes:

| Mode | Writes to bike? | Purpose |
| --- | --- | --- |
| Inspect | No | read DeviceInfo/OQC, classify generation, bind bike identity to archive manifest |
| Dry run | No | produce the exact `0x0A/0x0B/0x0D` frame sequence and compare it to official traces |
| Armed update | Yes, locked | only official/hash-matched or locally signed images, with resume/recovery and external power rules |

For custom firmware development, the first independent payload should run from
SRAM and must not alter internal flash or NOR. After dual backups and a proven
restore path, the first flash-resident clean-room image may occupy only the
dump-confirmed application slot while preserving the lower stock loader and
metadata. Replacing the lower loader is a separate, later milestone after its
validation, install-journal, power-loss, and rollback behavior are reproduced.

## Practical answer

- Can we rewrite the BSP? **Mostly yes**, as a clean-room hardware contract.
- Can we rewrite the internal SPP/Noodoe protocol? **Partly yes already**;
  updater correctness still needs exact ACK/resume/final-install behavior.
- Can a custom Android app deliver firmware by Bluetooth? **Yes, through the
  Noodoe SPP OTA state machine.**
- Can that app use the STM32 ROM bootloader over Bluetooth? **Not directly.**
  ROM bootloader is a physical-interface recovery path, not a Bluetooth path.
- Can we "keep" the built-in update path? **Probably yes**, if we target the
  Noodoe first-stage/update logic and satisfy its image validation rules. That
  is the right strategic target.

## Source pointers

- Local v1.1 BSP report:
  `AK550_Noodoe_Firmware_Reverse_Engineering_Report_2026-09-01_v1.1_BSP.docx`
- Firmware update analysis:
  `analysis/2026-09-01-firmware-update-architecture/README.md`
- SR1.5 hardware cross-check:
  `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`
- Generated peripheral map:
  `analysis/2026-09-01-custom-firmware-hardware-map/README.md`
- Generated IRQ map:
  `analysis/2026-09-01-custom-firmware-hardware-map/IRQ-MAP.md`
- ST AN2606:
  `https://www.st.com/resource/en/application_note/cd00167594-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf`
