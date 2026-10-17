## 0.11.5 Bootstrap display update

**From 0.11.4, use the normal CFW update.** Gate and language resources are unchanged. Older Gates still use keep-data stock return and reinstallation with the matching latest APK and ZIP. The older 0.11.4 Gate migration instructions below apply only when coming from an older Gate.

The initial file checks use a horizontal loading bar without numbers. Menus are centred; mint marks the selected item. Two fixed bars show the overall device stage and current device operation throughout transfer and verification. The phone uses the same device snapshot, with a small polling delay.

On **INSTALL CFW?**, a visible 30-second countdown automatically approves the verified installation. UP/DOWN stops the countdown. **Back → O** cancels; **Install CFW → O** approves immediately. Losing the connection stops automatic approval; a verified reconnect starts a fresh 30 seconds. The existing safe cancellation of an already started write is unchanged.

The Ambient sensor menu and Bootstrap sensor sampling are removed. **Developer’s message** displays the complete original text. DOWN advances and wraps from the last page to the first; O returns to the menu. Riding automatic brightness is unchanged.

Bootstrap runs during installation from stock. Existing 0.11.4 CFW users do not need to return to stock just to see these menus. Use the matching 0.11.5 APK and ZIP.

# First installation from stock · 0.10.1 / manual revision 2

**0.10.1 beta:** [current installation and language-pack sequence](../User/35-Languages.en.md). Confirm normal firmware startup before installing optional language resources.

**Keep permanent power connected:** a power interruption while older CFW records its keep-data stock-return request can stop boot before the new recovery code runs. Automatic recovery is not guaranteed in that legacy interval. Keep permanent power connected throughout stock return/reinstallation and do not operate the starter. IGN OFF is not removal of permanent power. If already stuck, do not assume another reset will recover it; retain diagnostics and ask for help.

**Revised 16 October 2026** · [Installation index](README.en.md) · [한국어](08-First-Install.md) · [Compatible models](07-Compatibility.en.md)

## Preparation

Use the matching **0.10.1 APK and installation ZIP**. Install the APK over the existing app to retain its records. Keep the ZIP intact; do not choose an individual BIN from a dump. Select the correct Noodoe and stop riding integration. If CFW is already running, use [CFW update](09-Update.en.md).

First installation admits stock 5.14 or 5.16 without HW/bootloader-version/model/PCBA whitelists. Read the compatibility page for the difference between admission and vehicle testing, and between the installation backup and a full NOR backup.

Keep the stationary vehicle and phone adequately powered. Keep the installer and Bluetooth service running. Do not operate the starter during installation/writing. IGN is a signal separate from permanent 12V: IGN OFF may leave installation running. Follow the app's ignition instructions at each step. The following captures illustrate stages; they are not a measured transfer timetable.

## 1. Send Bootstrap through stock firmware

![Bootstrap transfer](../images/install-01-stock-transfer.png)

The app identifies the actual stock device/address and sends the installation tool through its stock update protocol. This is different from updating an existing CFW.

## 2. Finish the stock handoff and boot Bootstrap

![Ignition and restart](../images/install-02-stock-reboot.png)

After the app confirms transfer verification and asks for the ignition action, **keep permanent power connected, turn IGN OFF, wait for the Bootstrap screen, then turn IGN ON**. Do not perform this step before verification. `NOODOE INSTALLER` means the stock writer is still working. Bluetooth may disconnect during restart; try automatic reconnect first; use manual pairing below only if it fails.

## 3. Reconnect and identify Bootstrap

![Bootstrap identification](../images/install-03-connect.png)
![Device connection screen](../images/boot-connect.png)
![Bootstrap welcome](../images/boot-welcome.png)

Look for `NewDoe Bootstrap / Welcome` and allow automatic reconnect. If it fails, disconnect and forget Noodoe in phone Bluetooth settings. On the Bootstrap connection screen press UP, select this phone, release O and hold O for two seconds. Pair again, choose Check current device, then Continue from here; do not resend Bootstrap. The app checks the actual role, image and device ID. Connection alone does not complete installation; the Bluetooth test menu is a diagnostic tool.

## 4. Audit storage

![Storage audit](../images/install-04-audit.png)
![Device audit screen](../images/boot-audit.png)

Bootstrap can continue with IGN OFF; this neither cancels installation nor disconnects permanent power. Follow the ignition state requested for the current step. The app checks FAT/allocation data and reserved areas rather than overwriting damaged, cross-linked or unknown files.

## 5. Preserve original metadata

![Metadata backup](../images/install-05-backup-metadata.png)

The phone keeps the original FAT/directory metadata to be changed and the allocation plan. Device-specific lower-flash/bootloader capture is retained and verified. Normal installation does not download the entire 128MiB NOR image; that is a separate backup operation.

## 6. Prepare, write and verify CFW files

![Resource preparation](../images/install-06-file-resource.png)
![Device preparation screen](../images/boot-prepare.png)

The package's stock recovery APP, fonts/Bluetooth resources, settings/photos/logs and A/B update slots are prepared and read back for verification. Existing stock photos are preserved. An ordinary fresh installation initializes new CFW settings/photo slots.

**A/B means firmware candidates and a known-good fallback, not two photo slots.** A prior good CFW is retained until the new candidate is confirmed.

## 7. Compare the final layout

![Final comparison](../images/install-07-preserve.png)
![Device verification screen](../images/boot-verify.png)

The app rechecks FAT, file ranges, required images and resources. A verification failure is not simply slow transfer. Preserve the app error and the device's Help code.

## 8. Final installation confirmation

After device verification, **INSTALL CFW?** displays a 30-second countdown and automatically installs if untouched. UP/DOWN stops the countdown and changes selection. Choose **Install CFW → O** to install now or **Back → O** to cancel. Connection or verification errors cannot automatically approve installation.

## 9. Approve Install CFW? on Noodoe

![Local approval](../images/install-09-confirm.png)
![Device approval screen](../images/boot-confirm.png)

Release held keys first. Select `Install CFW` with UP/DOWN, then confirm with O as instructed on screen. Committed installation can continue without the phone. A phone error popup does not mean the whole image should immediately be sent again.

## 10. Install and restart

![Restart stage](../images/install-10-reboot.png)
![Device installation screen](../images/boot-install.png)

The stock writer may appear and Bluetooth may disconnect/reconnect. The app shows its last confirmed state. Do not confirm the new CFW while `NOODOE INSTALLER` is still displayed.

## 11. Confirm the actual new CFW screen

![Candidate health confirmation](../images/install-11-health.png)
![Device CFW confirmation screen](../images/cfw-confirm.png)

Only after the new CFW's `CFW UPDATE` screen actually appears, use the screen-confirmation button in the app. The app rechecks the device/candidate and Bluetooth. At least **5 seconds of healthy execution**, Bluetooth initialization and app reconnection are required.

The current candidate has a device-owned window of up to **5 minutes**; follow any different deadline reported by older running firmware. Screen approval does not extend it indefinitely. A failed candidate uses a known-good rollback when available. A first installation without a previous CFW may enter independent recovery instead.

## 12. Wait for completion

![Completion](../images/install-12-done.png)

Completion requires the app to verify permanent candidate confirmation. First installation or an explicit fresh start prepares new CFW data. A compatible keep-data reinstall can restore retained records. Ordinary CFW updates preserve settings, photos, trips and maintenance baselines.

## Cancellation or a paused restart

During a cancellable Bootstrap session, release O and hold it freshly for **3 seconds** to request safe cancellation. An active write may need to reach a safe boundary. Committed work is not forcibly undone; IGN OFF alone does not cancel it.

If 0.10.1 shows `INSTALLATION PAUSED` with the manual-reset instruction, follow [paused-installation recovery](10-Recovery.en.md). Record Stage and Code/phase, hold UP+O together for 3 seconds, release the buttons, and inspect the same device/package's state. Older running firmware can still show its older wording. Do not apply this action merely because a transfer is still running.

## Other Bootstrap menus

| Main menu | Help |
|---|---|
| ![Menu](../images/boot-menu.png) | ![Help](../images/boot-help.png) |

Use UP/DOWN to choose and O to enter. Ambient-light diagnostics show raw/lux/brightness values; Bluetooth test shows initialization, connection and request/reply state. Injected example values are not proof of a particular device's sensor or radio working.

| Ambient light | Bluetooth diagnostics |
|---|---|
| ![Ambient](../images/boot-ambient.png) | ![Bluetooth](../images/boot-radio.png) |

![Error example](../images/boot-error.png)

Keep Help codes together with phone logs. Closing a screen or resetting the app's connection state does not confirm an unknown device operation.
