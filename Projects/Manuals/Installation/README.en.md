# Installation, updates and recovery

[0.9.32 connection recovery: authentication, timers and speed selection](12-Connection-Recovery.en.md)

[한국어 상세 절차](README.md) · [Using Noodoe](../User/README.en.md) · [Release](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

**Current pair: APK / Product 0.9.32.** Routine Product updates preserve your data. New GPIO7 display profiles use the matching installer Gate.

## Update an existing CFW

1. Install the [APK](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeCompanion-0.9.32.apk) over the existing app, keeping its data and logs.
2. Select and identify the correct Noodoe. Stop riding sync before starting an update.
3. Select the [matching ZIP](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest/download/NoodoeInstaller-CFW-0.9.32.zip), review its version and follow the wizard.
4. Allow transfer, verification, device approval and restart to finish. With unchanged Gate/assets, the normal payload is the 384KiB Product image.
5. Follow the app's candidate-screen/reconnection checks and wait for confirmed completion. A 100% transfer is not a confirmed boot.

**Ordinary updates preserve settings, rider name, photo slots, trips and maintenance baselines.** The old manual's reset-after-confirmation policy no longer describes ordinary updates. Start-fresh, explicit reset and complete removal remain separate choices.

If connection drops, reopen the same device/ZIP and check the last operation before resuming. A complete uploaded image can be reused after integrity checks. Do not blindly repeat COMMIT or RESET. Trial boot checks and rollback remain in place.

## First install

Follow the [step-by-step illustrated guide](08-First-Install.md). The stock-to-Bootstrap handoff is separate from routine CFW updates. First setup allocates the CFW files in verified free space, preserves stock files and prepares recovery data. Select retained-data restoration or start-fresh deliberately when reinstalling. The stock `NOODOE INSTALLER` screen is not the new CFW's completed-boot screen.

## Return to stock

The app offers **keep CFW data** (Gate restores stock APP) or **erase CFW data** (a dedicated uninstall firmware removes verified CFW data and returns its space). Emergency Gate recovery always keeps data. Stock files and factory identity are preserved; returning to stock may require Bluetooth re-pairing separately.

For the emergency gesture, turn IGN OFF, hold O for about one second, turn IGN ON while holding O, then keep holding for about three seconds. In Gate, follow **Back to stock** and its fresh two-second O confirmation when requested. Gate has no Bluetooth. See the [illustrated recovery guide](10-Recovery.md).

## When something stops

Keep the displayed code, last file/sector and operation stage. Reconcile actual device state before retrying an uncertain operation. App connection-state reset clears transient selections and sockets, not device data or an in-progress device operation. Export a diagnostic ZIP; the app keeps its internal originals.

For a storage error during ordinary use, open device settings, inspect **Settings / Ride records**, then request **Retry failed saves** once if automatic writes are paused. Check the result; this is not a format command. Detailed help: [troubleshooting](11-Troubleshooting.md).

As of0.9.28, the healthy-run threshold is5seconds. Candidate-screen approval, exact-version reconnection checks and rollback remain; the overall confirmation deadline is unchanged.
