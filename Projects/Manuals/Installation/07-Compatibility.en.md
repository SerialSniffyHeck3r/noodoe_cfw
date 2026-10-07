# Compatible models and installation requirements

**APK / Product 0.9.62 · Manual revision 2 · 2026-10-07**

[Installation index](README.en.md) · [한국어](07-Compatibility.md)

## Which Noodoe modules are supported?

This CFW targets **STM32F4-based Noodoe modules**. Compatibility is not determined by the motorcycle's name alone. Installation is not restricted to the AK550: a module from the same family fitted to another vehicle is admitted when it meets the requirements below.

| Vehicle or module | Support scope and evidence |
|---|---|
| The developer's AK550 with F4 Noodoe | A development and usage target. The user reported successful stock-to-CFW installation and fuel-warning dimming in 0.9.59. This does not establish physical validation of every 0.9.62 feature |
| The developer's bench F4 Noodoe | A separate development/test module. It does not represent every PCB revision |
| F4 Noodoe on another KYMCO model or another vehicle | Admitted when the installation requirements below are met; the model name does not block it. Wiring, instrument-cluster data and the dashboard/Noodoe button-selector signal require separate verification |
| Other MCU families or versions outside the ranges below | Outside this release's installation support scope |

Available evidence does not establish a physically tested list of additional motorcycle models. **Installation admission is different from verified vehicle functionality.** Missing speed or IGN data on another vehicle may prevent the stationary installation check or limit vehicle-data features.

## Requirements for a first installation from stock

| Item | Requirements for the 0.9.62 APK + ZIP |
|---|---|
| MCU family / HW field in the device response | F4 family / **HW 0** |
| Stock bootloader | **0.10–0.19**, the 0.1X range |
| Stock application firmware | **5.16** |
| Model string | **No whitelist for SAA1AA, SAA1AA(KR) or other model strings** |
| PCBA string | **No whitelist for SR0701 or other PCBA strings** |
| Phone | Android **6.0 / API 23 or later**. This is an Android APK; no iPhone app is supplied |
| Installation files | APK and installation ZIP from the same release. Older ZIPs may retain their narrower checks |

`HW 0` is the hardware-family value reported by the protocol. It does not mean all PCBs have the same revision. Removing model/PCBA restrictions does not remove device identification, bootloader capture, image-integrity, supported display-configuration or storage checks.

Identify the device in the app and inspect its HW, bootloader and stock versions. If compatibility checking stops, retain the numeric values and diagnostics. Do not substitute another device's identity or dump. If installation has already started, distinguish a compatibility rejection from an unresolved previous operation.

## If CFW is already installed

The **stock 5.16 requirement does not apply to the version of an already running CFW**. Use the [ordinary CFW update](09-Update.en.md). The app compares the existing Gate with the new package and requests recovery-layer migration through a keep-data stock return only when needed.

## ST-Link and backups

The normal wireless installation procedure does not require ST-Link. Bootstrap captures and compares two reads of the device-specific lower flash/bootloader, preserves original metadata and backups, and **prepares and verifies the package's stock 5.16 recovery APP in external NOR**. This is not a complete byte-for-byte backup of every flash chip in your module.

The separate [full NOR backup](../User/13-NOR-Backup.en.md) covers external 128MiB storage, not a complete dump of the MCU's internal flash. Keep any original dumps you already have and the app's backup/diagnostic records. Wireless recovery cannot be guaranteed when its image is damaged or the MCU cannot execute recovery code.
