# FuckNudo CFW

**A second life for the round display on a KYMCO AK 550.**

The Noodoe module is more interesting than a Bluetooth accessory: it has its own STM32, graphics controller, display, storage and vehicle link. This project began by listening to that link, reading the original firmware's behavior and bringing up the hardware one peripheral at a time. The result is a custom dashboard application running on the original module, paired with an Android companion. It is an independent reverse-engineering project, not affiliated with KYMCO.

![Dashboard with the speed ring and trip computer](Projects/documentations/manual/images/trip-a.png)

The firmware keeps the display's circular active area and uses the outer ring for speed. Inside it are a trip computer, phone notifications, music and album art, calls, and a breadcrumb trail supplied by the phone's GPS. The original three-button control, ignition transitions, low-fuel warning, photo backgrounds, light/dark themes and persistent settings are integrated into the same UI. Classic Bluetooth SPP carries companion data; vehicle speed, fuel and odometer originate on the dashboard-to-Noodoe UART rather than the phone.

The installation path is as much a part of the experiment as the UI. The original updater launches a small Bootstrap, which checks the target and prepares CFW-owned files in unused FAT space on external NOR. A small independent RecoveryGate can restore the preserved stock application with the physical button even when the main CFW or its Bluetooth link fails. Later updates normally replace only the Product application while retaining a previous working copy until the new version is confirmed. The stock bootloader and factory identity area are not intentionally replaced.

This has run on development hardware and an AK 550, but it remains an active hardware project. The bench module has a damaged Bluetooth/ambient-light path, so that board cannot prove wireless or automatic-brightness behavior for every revision. Installation and recovery depend on the exact board, bootloader and preserved stock image; read the [installer documentation](Projects/documentations/installer.md) before using a release on a vehicle.

## Explore

| Topic | Read |
|---|---|
| What is on the board? | [Hardware](Projects/documentations/hardware.md) |
| How was the original behavior identified? | [Reverse engineering](Projects/documentations/reverse-engineering.md) |
| How did disassembly reveal the chips and peripheral setup? | [Stock assembly analysis (Korean)](Projects/documentations/stock-assembly.ko.md) |
| How is the CFW divided? | [Architecture](Projects/documentations/architecture.md) |
| Where do code, assets and saved data live? | [Memory map](Projects/documentations/memory-map.md) |
| How do installation and rollback work? | [Installer and recovery](Projects/documentations/installer.md) |
| What comes from the bike? | [Vehicle interface](Projects/documentations/vehicle-interface.md) |
| Controls and page-by-page walkthrough | [User manual](Projects/documentations/manual/README.md) |

The source trees are [Android](Projects/Android) and [STM32CubeIDE](Projects/STM32). Historical observations and protocol notes are collected under [documentations](Projects/documentations/README.md). Compiled APKs and firmware packages are distributed through Releases, not committed to the source tree. The source tree does **not** include a raw stock firmware dump, a device backup or factory secrets; an installation release may contain an exact approved stock payload.

[한국어 소개·기술 문서](README.ko.md) · [Releases](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

<!-- NOODOE_LATEST_DOWNLOADS_BEGIN -->
## Latest downloads

**Companion 6.11.4 — Compact two-line notifications**

- [Android companion APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/companion-v6.11.4-phone-compact-two-lines/NoodoeCompanion-6.11.4.apk)
- [Matching installation ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/companion-v6.11.4-phone-compact-two-lines/NoodoeInstaller-CFW-6.11.4.zip)
- [Latest release and validation notes](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Use the APK and ZIP from the same release. A Bootstrap or Gate change may require a different migration path from a Product-only update.
<!-- NOODOE_LATEST_DOWNLOADS_END -->

## Source and rights

The project combines original CFW code with third-party components under their own licenses. The presence of source here is not a blanket commercial-use license. In particular, review BTstack's license and the TI service-pack terms before redistribution. KYMCO's original firmware and personal device dumps are not published as source. See [source and asset notes](Projects/documentations/README.md#source-and-asset-boundaries).
