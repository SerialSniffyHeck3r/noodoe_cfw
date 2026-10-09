# Connection and pairing recovery

**APK 0.9.72 / Product 0.9.71 · 2026-10-10**

[Installation index](README.en.md) · [한국어](12-Connection-Recovery.md)

## Identify the running role first

Use APK and ZIP 0.9.72 and retain app records. Stop riding integration and identify the current role. Stock, Bootstrap and Product are different stages; **Gate has no Bluetooth**. If Bootstrap already runs, do not repeat the stock transfer.

## Lost responses or unresolved earlier results

1. Select the same device and installation ZIP and inspect the current device/last operation.
2. If a known-good CFW was restored, choose **Acknowledge rollback · unblock recovery** and wait for the device to save acknowledgement. Do not delete the record.
3. Choose ordinary update or stock return again. If status queries keep failing, preserve diagnostics.
4. The app's **Force recovery and reinstall · keep data** uses independent stock recovery. It checks device identity, boot state, active writers and the stock recovery image; it does not overwrite an active write.
5. After the actual stock screen appears, use **Stock restored on device · verify → install Bootstrap → Keep stored data**. Disconnection alone does not confirm stock return.

If active writes, trial boot or a recovery-image error block this route too, preserve the reported cause and logs. [Physical Gate entry](10-Recovery.en.md) is a separate path when the phone cannot connect.

## When automatic Bootstrap connection fails

Starting with 0.9.72, installation no longer requires a pairing-completion receipt. The app tries to reconnect after sending Bootstrap. If connected, continue directly.

1. If automatic connection fails, open phone Bluetooth settings, **forget only this Noodoe and pair again**. Keep app data and installation records.
2. Return to the app and manually choose **Check current device → Continue from here**. An older pairing-wizard record no longer blocks installation.
3. Only if pairing itself still fails, on the Bootstrap connection screen use **UP → select your phone → release O, then hold it for two seconds** to forget the phone on Noodoe too, and pair again. During ordinary CFW operation, use the existing **Settings → Connections → Re-pair phone** menu. `ALL` removes every phone; use it only if intended.
4. Choose **Check current device → Continue from here** again. There is no need to return midway so the app can witness the unpaired state. **Do not resend Bootstrap.**

Device identity, selected ZIP, image validation and unresolved-operation checks remain. A Bluetooth connection alone does not confirm installation success. Stock uses its own pairing controls; Gate has no Bluetooth.

## Bluetooth startup-error choices

| Choice | Meaning |
|---|---|
| RETRY | Restart Bluetooth at the current speed; retain keys |
| TRY STANDARD SPEED | Retry Product at 921,600baud |
| RESET PAIRING | Choose/reset stored phone keys |
| FIRMWARE RECOVERY | Open independent Gate recovery |
| DETAILS | Inspect initialization, baud, error and key-persistence status |

Bootstrap uses standard speed. Product's MCU-to-Bluetooth-chip UART speed is different from actual wireless throughput. Re-pairing, display reinitialization, MCU restart and stock return are also distinct actions. For the installation error screen's UP+O 3-second instruction, follow [recovery](10-Recovery.en.md).
