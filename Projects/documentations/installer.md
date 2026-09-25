# Installation, updates and recovery

The installer is designed around one rule: **the rider must still have a way back if Product or Bluetooth fails.** It is not a generic STM32 flasher. It depends on a compatible, identified Noodoe board, its original updater and an exact stock recovery image. Check the release's target profile and validation notes rather than assuming every AK 550 year is identical.

## First installation

```text
Stock application → stock updater → FuckNudo Bootstrap
    → device/BL/FAT audit → CFW-owned files + exact recovery image
    → Gate + Product installation → Product trial → confirmed CFW
```

The Android wizard first identifies the selected device and package. The stock updater loads the Bootstrap through the stock-supported path; CFW does not ask the app to program the stock bootloader or arbitrary MCU addresses. Bootstrap uses the same HCI/SPP/NDCP low-level family as Product, but has its own UI and storage permissions. It checks the observed hardware/bootloader profile, FAT ownership, free clusters and current journal before enabling a bounded write scope. The phone records critical intent and recovery evidence locally before a mutating command.

On the device, stock recovery and the initial CFW files are prepared and physically read back. Product is transmitted once, with local copies used where possible, rather than sending the same 384 KiB image multiple times. A prepared file is not an installed application: after verified staging and local confirmation, the existing stock installation mechanism places Gate/Product in internal flash. The first launch is a **trial**. Product must show healthy progress for the current 30-second interval, initialize the needed services and be matched to the phone's candidate/UID confirmation before its journal is committed as working. The device's evidence is authoritative; a phone progress bar is only an interpretation of it.

The stock bootloader's own flash/metadata operation is a separate failure window. Gate cannot make a previously unchanged stock bootloader atomic. Do not interpret “transfer complete” or “Bluetooth connected” as proof that the trial has been confirmed.

## Routine Product update

For a compatible installed Gate and unchanged resources, the ordinary update path sends the **384 KiB Product body** to the inactive NOR A/B file. It verifies the header, vectors, resource requirement, UID and complete image hash with NOR readback, then commits the candidate. Gate copies it into `0x08020000–0x0807FFFF` and verifies internal flash before boot. The previous confirmed Product remains available on NOR until the new version is accepted. If the new runtime fails or its confirmation does not arrive within the bounded trial window, Gate can restore that previous confirmed image. A trial failure and an actual stock return are distinct results.

Disconnects are reported as uncertain until the device is queried again. A file's verified sector/offset and package identity determine whether the transfer may resume; `COMMIT` or `RESET` is never blindly replayed simply because an app request timed out. Changing the Bootstrap/Gate or resource format may require a separate first-install-style migration; a Product-only ZIP cannot silently change those components.

## Button-only recovery and stock return

RecoveryGate is an independent executable in its own 64 KiB internal-flash sector. It starts its watchdog early, runs without Product, Bluetooth, SDRAM, LVGL or downloaded fonts, and can draw a basic EVE ROM-text recovery screen. On the studied wiring, the deliberate entry gesture is: key OFF, press and hold the center/ENTER button, key ON while holding, then continue holding through the recovery threshold. The current [Gate policy](../STM32/RecoveryGate/README.md) records its debounce and timing. Entry into recovery **does not itself authorize** erasing Product or restoring stock; a fresh local confirmation is required.

Gate validates the preserved `CFWREC.DAT` against the pinned stock version, board identity and original boot-code fingerprint, stages the image in the OEM update area, checks physical readback and then asks the original installer to restore the stock APP. A normal Gate stock return leaves the CFW-owned NOR files allocated. That is intentional: they make later reinstallation or forensic review possible and do not overwrite the original photos. An explicit uninstall/data-erasure route has its own temporary firmware and FAT ownership checks; it is not what the emergency button performs.

If both CFW and radio are dead but Gate, the button input, NOR, MCU and original installer still work, this local route is the intended no-ST-LINK recovery. It cannot recover a physically damaged NOR, a broken power/clock source, lost stock image or corrupted resident bootloader. On a vehicle without an accessible SWD header, these limits matter more than a successful bench flash.

## Data and identity boundaries

The project never treats factory MAC/PIN/vehicle identity as user settings. The original internal boot/factory area remains in place across Product updates and normal stock restoration. CFW's settings, trips, three custom picture slots, logs, resource banks and A/B images live in separate validated FAT files. A restored stock application sees its own photos and OEM file layout; additional CFW files remain unless explicitly removed. A changed phone bond may still require Android re-pairing, which is separate from restoring factory data.

The [user manual](manual/08-First-Install.md) walks through visible screens. The exact wire/storage contracts are in [NDCP protocol](technical-notes/ndcp-protocol.md), [install session](technical-notes/install-session-v2.md), [Gate source contract](../STM32/RecoveryGate/README.md) and [memory map](memory-map.md). These documents describe mechanisms; each release separately states which hardware and interruption tests were actually completed.
