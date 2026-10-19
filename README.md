> **0.11.15:** Verify language resources during installation; later boots load only the selected menu text and glyphs. Japanese index reads reuse existing memory. **Use matching 0.11.15 APK/Product. Gate, Bootstrap and B resources are unchanged. Camera-triggered disconnection still needs vehicle verification.** [Guide](docs/releases/0.11.15/INSTALL.md)

> **0.11.12:** Parking sleep and long-parking shutdown now handle failed RTC, radio, dashboard and UI preparation without abandoning active NOR writes. **Update both APK and firmware ZIP. Same-Gate devices from 0.11.4 onward use normal CFW update. Gate, Bootstrap and language resources are unchanged. Actual vehicle current/endurance remains unmeasured.** [Guide](docs/releases/0.11.12/INSTALL.md)

> **0.11.11:** Full-width dark dashboard, horizontal oil gauge with remaining distance, tap-to-cycle trips, editable stock UI preview and uncropped transparent NEWDOE wordmark. **APK-only update for devices already on Product0.11.10. Firmware ZIP0.11.10 and language resources are unchanged.** [Guide](docs/releases/0.11.11/INSTALL.md)

> **0.11.1:** larger logo/skull, Welcome! → Device Startup, and round-screen text layout. Even 0.11.0 needs keep-data stock return/reinstallation because Gate changed. [Installation](docs/releases/0.11.1/INSTALL.md)

**NewDoe 0.11.0:** [Read the new Gate installation/recovery guide first / 새 Gate 설치·복구 안내](docs/releases/0.11.0/INSTALL.md).

**0.10.7:** Reliable language activation, live preparation status, aligned trip units and split text/font resources. Healthy existing packs need no retransmission. [Details](docs/releases/0.10.7/INSTALL.md).

**0.10.4:** Update both APK and firmware ZIP. Full settings remain accessible when IGN is ON but vehicle speed is missing. Valid moving-speed locks remain. Existing language resources are reused. [Details](docs/releases/0.10.4/INSTALL.md).

<!-- NOODOE_LATEST_DOWNLOADS_BEGIN -->
# NewDoe 0.11.15 beta

Verify language resources during installation; later boots load only the selected menu text and glyphs. Japanese index reads reuse existing memory. **Use matching 0.11.15 APK/Product. Gate, Bootstrap and B resources are unchanged. Camera-triggered disconnection still needs vehicle verification.**

- [Android APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.11.15-language-loading/NewDoe-0.11.15.apk)
- [Matching installer ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.11.15-language-loading/NewDoe-CFW-0.11.15.zip)
- [Optional B resources (unchanged)](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.11.15-language-loading/NewDoe-LANGUAGE-B-0.11.2.zip)
- [Installation / 설치 안내](docs/releases/0.11.15/INSTALL.md)
- [Validation / 검증](docs/releases/0.11.15/VALIDATION.md)

<!-- NOODOE_LATEST_DOWNLOADS_END -->

# NewDoe

![My AK550, with Noodoe in the middle of the dashboard](Projects/documentations/images/my-ak550.jpg)

**A custom firmware project to give KYMCO's factory-option Noodoe another life.**

![Speed ring and trip computer](Projects/Manuals/images/trip-a.png)

Noodoe arrived in 2017 and spent six or seven years on KYMCO motorcycles. Word is KYMCO will wind down its service in 2027. What, exactly, will still work afterward? Your guess may be as good as mine. The motorcycle itself is still perfectly fine, though, and losing the screen because a service disappears sounded daft. So I started trying to use it again—and adding things I always wished the original had.

There's more inside Noodoe than I expected: an STM32, a graphics controller, a display, storage, and a phone link, all in a system separate from the instrument cluster. I studied the stock firmware, brought the hardware up piece by piece, and built both replacement firmware and an Android companion app. No iPhone app. Blame Apple, not me. lol

Want to actually use it? Start with the [user manual](Projects/Manuals/User/README.en.md), or the [installation and recovery manual](Projects/Manuals/Installation/README.en.md).

**Latest release:** [download the Android APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NewDoe-0.11.14.apk) · [download the matching installation ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NewDoe-CFW-0.11.14.zip) · [release notes](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest).

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


Current: **APK0.10.7 / Product0.10.7 beta**. [Ride Summary, retained settings and oil arc](Projects/Manuals/User/33-Ride-Preservation.en.md).

[0.9.69 · Settings catalog update fix](Projects/documentations/settings-catalog-0.9.69.md)

[0.9.70: Ignition-off photo without shading](Projects/documentations/off-photo-0.9.70.md)

[0.9.71: Ride Summary, settings preservation and oil arc](Projects/documentations/ride-preserve-0.9.71.md)


[0.9.73: caller identity and automatic riding reconnect](Projects/Manuals/User/34-Call-Reconnect.en.md). That historical0.9.73 release was APK-only;0.10.1 also requires the matching firmware and transition above.
