# CFW architecture

[한국어](architecture.ko.md)

The firmware is split by **responsibility and failure domain**. A good-looking UI must not own raw flash writes, and a broken Product UI must not be required to restore stock. The STM32CubeIDE tree is [Projects/STM32](../STM32); the phone is [Projects/Android](../Android). They communicate through the project's NDCP messages over Classic Bluetooth SPP.

## On-device executables

| Component | Runs when | Owns |
|---|---|---|
| **Bootstrap** | First installation through the original updater | Target identification, Bluetooth/SPP session, FAT/storage preparation, transfer/readback and local installation confirmation. |
| **RecoveryGate** | Normal boot before Product, or recovery waiting | Independent Product verification/copy, boot journal, watchdog, button-only stock restore. Its own EVE ROM-text UI needs no Product heap, SDRAM fonts, RTOS or Bluetooth. |
| **Product CFW** | Normal riding and companion use | Vehicle state, UI, settings, SPP services, assets, storage requests and trial-health reporting. |
| **UninstallBootstrap / Diagnostic** | Explicit temporary maintenance operation | Data-removal or deeper hardware tests. Neither is part of normal riding. |

The stock bootloader remains the resident installer. During ordinary Product updates, Gate stays in its separate internal-flash sector. The byte-exact approved stock application is held in a CFW-owned NOR file for button recovery; this is not a copy of the bootloader.

## Product layers

```text
Android connection service ── NDCP/SPP ── Control + domain state (App_Logic)
                                              │
                                device services / queues (Middlewares/Noodoe)
                                  │                           │
                         graphics model → EVE port       storage / UART / HCI
                                  └────────── BSP ─────────────┘
```

- `App_Logic/UI` owns the global riding/settings/power state machines and the central page-specific substate. `App_Logic/Vehicle`, `Settings`, `Control` and `Recovery` own the meanings of values, button actions, settings and trial boot. No raw register operation belongs here.
- `Middlewares/Noodoe` owns bounded asynchronous operations: vehicle frame parser, Bluetooth/NDCP transport, input event queue, ambient policy, NOR file validation, journals, pictures, update state and health monitoring. A request API distinguishes **accepted** from **completed**.
- `Graphics/UI` turns a page model into the circular display, speed ring, text, bitmaps and animations. `Graphics/Port` converts those primitives into EVE display lists. EVE swap completion matters: a texture or display list still referenced by scan-out cannot be overwritten merely because the CPU started the next frame.
- `Drivers/BSP` owns MCU pins, SPI/UART/I2C, EVE and panel bring-up, backlight PWM, external memory and watchdog. Cube-generated `Core` code is kept separate from project-owned layers.
- `Middlewares/Third_Party` contains pinned upstream LVGL, FreeRTOS, BTstack and support libraries with their own notices. The Product's Bluetooth host stack is not the Gate's: Gate intentionally has **no Bluetooth**.

## Runtime flow

Key ON starts the Product's welcome/riding transition. The speed ring, time and ODO form a persistent frame around a variable center page; short ENTER moves categories and UP/DOWN changes a page's item. Media, notifications, phone calls and GPS are data-driven views: the device displays received state but keeps vehicle speed sourced from UART. Key OFF first holds the last frame briefly, then presents the ride summary and progresses through configurable screen-held, screen-off/Bluetooth-held and full-power-off stages. The radio and panel power policy is separate from backlight power.

The Android app has a single socket/command owner. It distinguishes ordinary riding connection from update work; installation pauses content transfer. Music and notification text are rendered on the phone when CJK or layout makes it cheaper than embedding every glyph in MCU flash. The device receives bounded images with session generations and CRC. Capability bits prevent a new app from sending a layout that an older CFW cannot draw.

## Failure boundaries

- Image transfer is not an APP commit. The receiver checks length, CRC/hash, expected target, UID, resource requirement and physical NOR readback before changing the boot journal.
- Product trial health is **30 seconds of healthy owner progress** in the current code, followed by the matching phone/device confirmation; the boot journal retains the previous working Product until confirmation. The phone cannot turn a mere Bluetooth connection into proof of a healthy UI.
- An interrupted NOR journal write leaves the prior completed record. A committed copy-in-progress is replayed from the verified source before Product is started. Repeated unconfirmed boots enter Gate waiting rather than blindly booting a partial APP.
- The independent Gate can restore the pinned stock APP by physical gesture and local confirmation without Product or radio. It cannot repair a damaged resident bootloader, power rail or NOR chip.
- The watchdog detects a CPU that stops making healthy progress; it is a recovery aid, not a proof that storage or a user-visible transition succeeded.

Interfaces and code: [Product state machine](../STM32/App_Logic/STATE_MACHINE.md), [Gate contract](../STM32/RecoveryGate/README.md), [NDCP protocol](technical-notes/ndcp-protocol.md), [installation session](technical-notes/install-session-v2.md).
