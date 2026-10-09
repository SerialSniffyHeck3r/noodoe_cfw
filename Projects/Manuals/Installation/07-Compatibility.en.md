# Compatible models and installation requirements

**APK / Product 0.9.66 · Manual revision 1 · 2026-10-08**

[Installation index](README.en.md) · [한국어](07-Compatibility.md)

## Which Noodoe modules are supported?

This CFW targets **STM32F4-based Noodoe modules**. Compatibility is not determined by the motorcycle's name alone. Installation is not restricted to the AK550: a module from the same family fitted to another vehicle is admitted when it meets the requirements below.

| Vehicle or module | Support scope and evidence |
|---|---|
| The developer's AK550 with F4 Noodoe | A development and usage target. The user reports stable 0.9.62 operation; Stage 6/4 still requests manual RESET. This is not a 0.9.66 hardware test |
| The developer's bench F4 Noodoe | A separate development/test module. It does not represent every PCB revision |
| F4 Noodoe on another KYMCO model or another vehicle | Admitted when the installation requirements below are met; the model name does not block it. Wiring, instrument-cluster data and the dashboard/Noodoe button-selector signal require separate verification |
| Stock versions other than 5.14/5.16 | Outside this release's installation support scope |

Available evidence does not establish a physically tested list of additional motorcycle models. **Installation admission is different from verified vehicle functionality.** Missing speed or IGN data on another vehicle may prevent the stationary installation check or limit vehicle-data features.

## Requirements for a first installation from stock

| Item | Requirements for the 0.9.66 APK + ZIP |
|---|---|
| HW field in the device response | **No whitelist** |
| Stock bootloader version | **No version whitelist**; actual original identity is captured and retained |
| Stock application firmware | **5.14 or 5.16** |
| Model string | **No whitelist for SAA1AA, SAA1AA(KR) or other model strings** |
| PCBA string | **No whitelist for SR0701 or other PCBA strings** |
| Phone | Android **6.0 / API 23 or later**. This is an Android APK; no iPhone app is supplied |
| Installation files | APK and installation ZIP from the same release. Older ZIPs may retain their narrower checks |

Admission is based on the stock firmware version, not the HW field, bootloader version, model or PCBA. This does not certify every MCU or board. Removing these whitelists does not remove device identification, bootloader capture, image-integrity, supported display-configuration or storage checks.

Identify the device in the app and inspect its HW, bootloader and stock versions. If compatibility checking stops, retain the numeric values and diagnostics. Do not substitute another device's identity or dump. If installation has already started, distinguish a compatibility rejection from an unresolved previous operation.

## If CFW is already installed

The **stock 5.14/5.16 requirement does not apply to the version of an already running CFW**. Use the [ordinary CFW update](09-Update.en.md). The app compares the existing Gate with the new package and requests recovery-layer migration through a keep-data stock return only when needed.

## ST-Link and backups

The normal wireless installation procedure does not require ST-Link. Bootstrap captures and compares two reads of the device-specific lower flash/bootloader, preserves original metadata and backups, and **prepares and verifies the package's stock 5.16 recovery APP in external NOR**. This is not a complete byte-for-byte backup of every flash chip in your module.

The separate [full NOR backup](../User/13-NOR-Backup.en.md) covers external 128MiB storage, not a complete dump of the MCU's internal flash. Keep any original dumps you already have and the app's backup/diagnostic records. Wireless recovery cannot be guaranteed when its image is damaged or the MCU cannot execute recovery code.

**Installing from 5.14 also stages the supplied 5.16 recovery APP: returning to stock gives 5.16, not a captured 5.14 APP.** A healthy stock reply and the existing stationary/IGN check are still required.
