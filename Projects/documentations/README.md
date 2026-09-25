# Documentation

Start with the six maintained technical chapters. They summarize the current code and explicitly separate measured behavior, stock-firmware analysis and open questions.

1. [Hardware](hardware.md) — devices, buses, display, controls and confidence levels.
2. [Reverse engineering](reverse-engineering.md) — preservation, captures, stock-code cross-checks and corrections to early hypotheses.
3. [Architecture](architecture.md) — Bootstrap, Gate, Product, Android and their failure boundaries.
4. [Memory map](memory-map.md) — internal flash/RAM, external NOR/SDRAM, FAT containers and GPU RAM_G.
5. [Installer and recovery](installer.md) — first install, Product update, trial confirmation, rollback and stock restore.
6. [Vehicle interface](vehicle-interface.md) — UART frames, buttons, PH9, IGN and brightness.

The [page-by-page user manual](manual/README.md) contains UI and installation screenshots. Those images include simulated data and should not be read as proof of an on-road measurement. The [technical notes](technical-notes/) preserve protocol and session contracts. [Research notes](research-notes/) are dated working documents; [research journal](research-journal/) holds selected historical reports and their verification summaries. Some old notes link to raw local captures or binaries that are intentionally **not** public. Later controlled measurements and current code take precedence over an early hypothesis.

## Source and asset boundaries

The Git source snapshot is under [`Projects/Android`](../Android) and [`Projects/STM32`](../STM32). Build products (`APK`, `ELF`, `BIN`, ZIP, compiler objects and generated test output), device-specific NOR/flash backups, pairing keys, factory data, raw official firmware, private logs and extracted media are excluded. Releases are the separate channel for validated installation packages. The source alone cannot recreate the OEM stock image needed by a particular board's installer/recovery workflow.

Third-party code and data keep their own copyright and license notices in the source tree: STM32 HAL/CMSIS, LVGL, FreeRTOS, fonts, BTstack and TI controller service packs are not relicensed by this repository. BTstack's bundled license has noncommercial conditions; TI patch terms restrict use to TI devices. There is no repository-wide permission to commercially redistribute the assembled product. Review the applicable terms before building or distributing a derivative.

The STM32 directory preserves the CubeIDE `.ioc`, CDT project settings, linker files and project-owned tests/tools. The Android directory preserves the Gradle project and its test sources. Toolchain caches, local SDK paths and signing keys are deliberately absent. Details of running an individual build are in each project's README and source comments; first-install packaging additionally requires a legitimately obtained exact stock input and device-profile evidence.
