# Installation connection recovery — Companion 0.9.36 / CFW 0.9.35

## Update and stock recovery blocked after a rollback

0.9.36 is an **APK-only hotfix**. The supplied **CFW 0.9.35 ZIP is byte-for-byte unchanged**.
You do not need to install new firmware first to unlock the current attempt.

1. Install the **0.9.36 APK over the existing app**. Keep app data and installation records.
2. Keep the latest ZIP selected. Choose **Identify device → Current CFW / installation result**.
3. If the previous CFW was restored, choose **Acknowledge rollback · unblock recovery** and
   wait for the device to store the acknowledgement. A reboot-dismissed warning can still be queried.
4. Choose **CFW update** or **Back to stock → Keep CFW data**. Follow the keep-data stock
   route if the app explains that the recovery Gate needs migration.
5. After the stock screen appears, choose **Stock restored on device · verify**. This also
   reconciles prior ZIP boot waits against the fresh stock response.

The previous ZIP is not needed for these result checks. Failure records, photos, settings and
service data are retained. Active operations, mismatched devices/transactions and uncertain
file writes remain protected. The app acknowledgement also works independently of the old
firmware warning's O-button handler. If no phone connection is possible, use the existing
independent gesture: **key OFF → hold O for about 1 second → key ON while holding O →
hold 2 more seconds**, then follow the device recovery screen.

**Use Companion 0.9.36 with the CFW 0.9.35 ZIP.** Settings, photos and maintenance records
are retained. If the app requests recovery-layer migration, follow keep-data
stock return → this ZIP’s Bootstrap → Gate and CFW → confirm normal startup.
A device already using the new Gate takes the ordinary Product update path.

## Pair approved, but Couldn't Pair keeps returning

1. Open **Connection recovery → Completely reset this Noodoe connection**.
2. In Android Bluetooth settings, **forget only this Noodoe**, then return.
   Keep the app, its data and installation records.
3. Press **UP** on the device’s installation/trial connection screen. During
   normal Product operation with IGN ON, release all keys, then hold
   **UP+DOWN+O together for three seconds**. Settings → Connections → Re-pair
   phone opens the same recovery screen.
4. Choose your phone or **ALL** on Noodoe. ALL removes every stored phone key.
   Release O, then hold it for a fresh **two seconds** to approve. Keep power on.
5. Wait for **Ready to pair on your phone**, mark the device ready in the app,
   and choose **Pair and verify connection**. Approve Android’s request once.
6. The app verifies secure SPP, device identity and the newly saved key before
   resuming the same installation. Completed files are checked before reuse.

This recovery also works with PH9 in Dash Mode. If Gate shows ACTION REQUIRED,
choose RETRY to start Product first. Stock firmware uses its original pairing
controls; CFW key reset becomes available after Bootstrap starts. The guide
survives Android settings visits and app relaunch. Allow Nearby devices in the
app’s Android permissions when requested, and enable Bluetooth.

## RETRY and speed choices

- **RETRY:** restart Bluetooth at the current speed; retain keys.
- **TRY STANDARD SPEED:** retry Product at 921,600baud.
- **RESET PAIRING:** open the explicit peer/all-key reset workflow.
- **FIRMWARE RECOVERY:** enter independent Gate for retry, previous CFW or stock.
- **DETAILS:** inspect initialization, baud, first error and key persistence.

Bootstrap uses standard speed only. Product’s high-speed HCI check and the
phone’s authenticated connection are separate; UART baud is not RF throughput.

## Countdown and startup screens

New Product trial boot has **five minutes**; healthy-run confirmation takes
**five seconds**. The device owns the deadline. Reconnecting or resetting
pairing does not restart it. At 0:00, read the actual confirmation/failure result.
New Gate keeps the failure and waits for a choice. Explicit Gate RETRY verifies
the image and starts a new trial.

DEVICE STARTUP means startup checks, INSTALLING FIRMWARE means image writing,
RESTARTING DEVICE means reset handoff, and RESTORING PREVIOUS/STOCK FIRMWARE
means restoration. Independent Gate also checks ordinary reboots. Progress bars
use real work; connection waits do not invent installation percentages.

## If the menu stops responding

CFW 0.9.34 or later, IGN ON: release every key, hold **UP+DOWN for ten seconds**,
then release all keys to force an MCU restart. Unsaved state may be lost. This
differs from **UP+O for three seconds**, which restarts only the display. The CPU
and I/O task must still be running. On supported installation/trial screens,
a fresh **DOWN hold for three seconds** requests keep-data stock recovery.

[Validation and change evidence](../Validation/bt-retry-0.9.35.md) (Korean).
