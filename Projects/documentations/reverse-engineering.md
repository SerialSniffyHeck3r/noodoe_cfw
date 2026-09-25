# Reverse engineering the original system

The goal was to replace the application while retaining the useful vehicle interface and a path back to stock. This is a behavior-level reconstruction. The repository does not publish an OEM firmware dump, extracted factory data, a Bluetooth pairing secret or a disassembly of the complete stock image.

## Evidence chain

1. **Preserve the original state.** Read and hash the donor's internal flash and external NOR before changing them. Keep a separate known-good stock V5.16 application and record its model, PCB and bootloader identity. The bench and motorcycle were not assumed interchangeable: the examined bench bootloader is 0.14, whereas the motorcycle reports 0.15.
2. **Watch the vehicle link.** Passive UART observations established `115200 8N1` and an `F5 | command | length | payload | XOR` frame shape. An independently captured Noodoe-to-cluster `A1` brightness frame and cluster-to-Noodoe `21` frames were compared with the source instrument's actual ODO and fuel display. That resolved direction and several fields without writing to the cluster.
3. **Trace the stock application.** Static inspection of the V5.16 receive dispatcher, GPIO descriptors, EVE setup and boot/update paths identified UART5, the three button inputs, ignition PG13, FT81x, NOR, HCI and the light-sensor path. A code path is evidence of intent, not proof that every production board populates and exercises it.
4. **Compare the phone side.** The official Android application's commands, pairing flow and device-info responses helped identify the stock Bluetooth transport. CFW deliberately uses its own NDCP companion protocol over Classic SPP rather than claiming wire compatibility with the official app.
5. **Bring up one device at a time.** Display and backlight, buttons, vehicle UART, storage, then phone transport were tested independently. A real FT81x snapshot through the SWD capture mailbox was used for UI evidence where the bench was available. ARM-host protocol tests and Android screenshots are labeled separately from physical-panel or road tests.

## Findings that changed the design

| Early possibility | Evidence-driven conclusion |
|---|---|
| The LCD might be driven by STM32 LTDC and a full framebuffer. | The examined application sends SPI commands to FT81x EVE. The CFW therefore builds display lists and uses GPU RAM_G, not LTDC/DMA2D. |
| Four values in an outbound command could be four button bits. | The observed `01` payload is a communication/re-request code path. The three actual buttons have independent active-low GPIO descriptors. |
| Fuel percentage might be the same as the visible bars. | On the examined AK 550, the low nibble of one status byte matched 0- and 1-bar observations. A separate byte feeds a generic six-step stock widget; its exact physical meaning was not promoted to a validated fuel percentage. |
| A successful firmware transfer means a safe boot. | Installation has multiple boundaries: transport completion, NOR readback, APP staging, reboot, runtime health and phone confirmation. Each must be reported separately. |
| Key OFF removes power. | An always-supplied path and the PG13 ignition input require explicit awake/sleep states. Recovery also cannot rely on a true battery disconnect. |

The stock `F5 21/22` receive path and `F5 A1` transmit path are described in [vehicle-interface.md](vehicle-interface.md). The storage and rollback implications are in [installer.md](installer.md). Historical notebooks under [research-notes](research-notes/) retain dated hypotheses; where they disagree, use the later controlled measurement or current source, not the oldest note.

## What remains unresolved

- The exact connector cavity-to-MCU pin mapping, full electrical schematic and all unlabelled vehicle status bytes.
- Whether every year/model uses the same cluster frames, bootloader, NOR allocation and CC256x revision.
- The behavior of the motorcycle's cluster upon every CFW brightness step; the measured Noodoe TX packet alone does not prove the receiving algorithm.
- Long-duration RF, power-loss and rollback behavior across all hardware revisions. A host test or bench SWD success is never substituted for these physical trials.

Source notes: [UART payload analysis](research-notes/2026-09-09-ak550-uart-payload-map.md), [bidirectional live capture](research-notes/2026-09-10-upper-lower-uart-confirmed.md), [fuel-bar comparison](research-notes/2026-09-10-fromdash-gas1.md), [stock update/boot study](research-notes/2026-09-11-noodoe-bootloader-and-bluetooth-update.md).
