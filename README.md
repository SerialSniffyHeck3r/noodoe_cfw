<!-- NOODOE_LATEST_DOWNLOADS_BEGIN -->
# Latest downloads

**Companion 0.10.3 beta / unchanged CFW 0.10.2 — language setup**

- [Android APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.10.3-language-setup/NoodoeCompanion-0.10.3.apk)
- [Firmware ZIP (unchanged 0.10.2)](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.10.3-language-setup/NoodoeInstaller-CFW-0.10.2.zip)
- [Optional offline language archive](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.10.3-language-setup/NOODOE-LANGUAGE-B-0.10.1.zip)
- [Release notes / 설치 안내](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/tag/cfw-v0.10.3-language-setup)

Already on CFW 0.10.2? Update only the APK. Language installation is now explicitly offered after firmware confirmation, with verified automatic download. 이미 0.10.2이면 APK만 갱신하고 앱 상단 기기 언어 · 언어팩 설치에서 진행하세요.
<!-- NOODOE_LATEST_DOWNLOADS_END -->

# FuckNudo CFW

![My AK550, with Noodoe in the middle of the dashboard](Projects/documentations/images/my-ak550.jpg)

**A custom firmware project to give KYMCO's factory-option Noodoe another life.**

![Speed ring and trip computer](Projects/Manuals/images/trip-a.png)

Noodoe arrived in 2017 and spent six or seven years on KYMCO motorcycles. Word is KYMCO will wind down its service in 2027. What, exactly, will still work afterward? Your guess may be as good as mine. The motorcycle itself is still perfectly fine, though, and losing the screen because a service disappears sounded daft. So I started trying to use it again—and adding things I always wished the original had.

There's more inside Noodoe than I expected: an STM32, a graphics controller, a display, storage, and a phone link, all in a system separate from the instrument cluster. I studied the stock firmware, brought the hardware up piece by piece, and built both replacement firmware and an Android companion app. No iPhone app. Blame Apple, not me. lol

Want to actually use it? Start with the [user manual](Projects/Manuals/User/README.en.md), or the [installation and recovery manual](Projects/Manuals/Installation/README.en.md).

**Latest release:** [download the Android APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeCompanion-0.10.3.apk) · [download the matching installation ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeInstaller-CFW-0.10.2.zip) · [release notes](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest).

## From my research notebook

These are selected notes, not the entire pile. I'll keep updating them as I go.

| Topic | Notes |
|---|---|
| The physical hardware | [Hardware](Projects/documentations/hardware.md) |
| How I picked apart the stock system | [Reverse engineering](Projects/documentations/reverse-engineering.md) |
| Finding hardware in the machine code | [Stock firmware assembly](Projects/documentations/stock-assembly.md) |
| How CFW is built | [Architecture](Projects/documentations/architecture.md) |
| Memory and assets | [Memory map](Projects/documentations/memory-map.md) |
| Installing, rolling back and recovering | [Installer architecture](Projects/documentations/installer.md) |
| Buttons and vehicle data | [Vehicle interface](Projects/documentations/vehicle-interface.md) |
| The dashboard UART, byte by byte | [Vehicle UART](Projects/documentations/vehicle-uart.md) |

[한국어](README.ko.md)

Source code is maintained in a separate private repository. This public repository contains research notes, manuals, screenshots and downloadable releases.

[Display wiring revisions from stock assembly](Projects/documentations/hardware-revisions-0.9.29.en.md)

**Installation manual0.10.1 beta:** [compatible models and limits](Projects/Manuals/Installation/07-Compatibility.en.md) · [install/update/recover](Projects/Manuals/Installation/README.en.md) · [한국어](Projects/Manuals/Installation/README.md). Stock 5.14 or 5.16 is admitted without HW/bootloader-version/model/PCBA whitelists; vehicle validation is listed separately.


Current: **APK0.10.3 / Product0.10.2 beta**. [Ride Summary, retained settings and oil arc](Projects/Manuals/User/33-Ride-Preservation.en.md).

[0.9.69 · Settings catalog update fix](Projects/documentations/settings-catalog-0.9.69.md)

[0.9.70: Ignition-off photo without shading](Projects/documentations/off-photo-0.9.70.md)

[0.9.71: Ride Summary, settings preservation and oil arc](Projects/documentations/ride-preserve-0.9.71.md)


[0.9.73: caller identity and automatic riding reconnect](Projects/Manuals/User/34-Call-Reconnect.en.md). That historical0.9.73 release was APK-only;0.10.1 also requires the matching firmware and transition above.
