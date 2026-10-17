## 0.11.3 language stability update

From 0.11.2, install **the 0.11.3 APK and NewDoe-CFW-0.11.3.zip** through CFW update. Gate is byte-for-byte identical to 0.11.2, so this path does not require stock return. Older Gates retain the existing keep-data stock-return migration requirement.

- Reuse **NewDoe-LANGUAGE-B-0.11.2.zip**. Its older filename is intentional: resource contents are unchanged. A matching verified pack is checked without retransmission.
- An older installed pack gets an explicit upgrade offer. Later preserves the existing pack and language preference.
- Transfer, verification/saving, automatic restart and actual menu activation are separate statuses. A lost final reply triggers fresh device readback, not a firmware reinstallation or blind transfer replay.
- Turn English only off, choose the device language, then accept the restart prompt on the stopped device. Follow app applies the app language at the next boot; a manual choice is not overwritten on reconnection.
- Transient read-only storage errors get two additional attempts. Published text and glyph caches remain usable during a recheck. Persistent errors fall back to English and retain preferences; fonts are not automatically repaired.

Hardware check: choose Korean, accept restart, and verify the Korean settings without tapping the app. Repeat with Japanese, then cycle power and verify the saved choice. With the current pack already installed, Install/resume resources should show the active language without retransmitting. If errors persist, retain diagnostics and use Install/resume resources to recheck. Firmware completion and language resource failure remain separate.

Keep the vehicle stopped and the module powered. Ignition ON/OFF does not cancel resource installation. Confirmed font damage or insufficient storage retains the existing explicit recovery/English-only choices.

---

# FuckNudo manuals

[0.10.1 languages and installation transition —0.10.1 beta](User/35-Languages.en.md)

**APK / Product0.10.1 beta · 2026-10-16**

1. [Usage guide](User/README.en.md): buttons, pairing, media, calls, maps, settings and power
2. [Installation, updates and recovery](Installation/README.en.md): distinct stock-first-install and CFW-update procedures
3. [Troubleshooting and diagnostics](Installation/11-Troubleshooting.en.md): Paused, unknown results, pairing and recovery
4. [Menu, GPS, battery and fuel-warning indicators](User/23-Controls-Recovery.en.md)
5. [NOR backup and file browser](User/13-NOR-Backup.en.md)

[0.10.1 APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeCompanion-0.10.1.apk) · [matching ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeInstaller-CFW-0.10.1.zip) · [release notes](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Use APK and ZIP from the same release. Ordinary updates retain settings, photos, trips and service baselines. First installation, an explicit fresh start and full removal are different operations. The app includes an offline quick guide; its Full manual button opens the appropriate Korean or English index.

Versioned historical notes and older screenshots describe their original versions. The current usage and installation guides take precedence; software-rendered pictures are labelled as simulations.

## Compatibility before installation

Read the [compatibility requirements and vehicle evidence](Installation/07-Compatibility.en.md) first. Stock 5.14 or 5.16 is admitted without HW, bootloader-version, model or PCBA whitelists. Wiring and vehicle data on every other model are not verified. The Korean and English installation/recovery guides now cover the same steps and conditions. This release includes a new Product and matching APK/ZIP; Bootstrap and Gate are unchanged.

[0.9.62 · Dashboard blink alerts](User/24-Dashboard-Alerts.en.md)

[0.9.63 · Indicators and reconnection](User/25-Connection-Indicators.en.md)

[0.9.64 · HOME AUTO · manual pairing · background return](User/26-Home-Auto.en.md)

[0.9.65 · Latest messages · location/music replies](User/27-Latest-Messages.en.md)

[0.9.66 · Balanced speed ring colours](User/28-Speed-Ring.en.md)

[0.9.67 · Actual stock mode](Installation/29-Stock-Mode.en.md)

[0.9.68 · Sharing, settings and smooth ring](User/30-Sharing-Settings.en.md)

[0.9.69 · Settings catalog update fix](User/31-Settings-Catalog.en.md)

[0.9.70 · Ignition-off photo without shading](User/32-Off-Photo.en.md)

[0.9.71 · Summary contrast, retained settings and oil arc](User/33-Ride-Preservation.en.md)
