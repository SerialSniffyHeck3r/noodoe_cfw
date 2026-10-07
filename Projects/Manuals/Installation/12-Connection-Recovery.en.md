# Connection and pairing recovery

**APK / Product 0.9.62 · Manual revision 2 · 2026-10-07**

[Installation index](README.en.md) · [한국어](12-Connection-Recovery.md)

## Identify the running role first

Use APK and ZIP 0.9.62 and retain app records. Stop riding integration and identify the current role. Stock, Bootstrap and Product are different stages; **Gate has no Bluetooth**. If Bootstrap already runs, do not repeat the stock transfer.

## Lost responses or unresolved earlier results

1. Select the same device and installation ZIP and inspect the current device/last operation.
2. If a known-good CFW was restored, choose **Acknowledge rollback · unblock recovery** and wait for the device to save acknowledgement. Do not delete the record.
3. Choose ordinary update or stock return again. If status queries keep failing, preserve diagnostics.
4. The app's **Force recovery and reinstall · keep data** uses independent stock recovery. It checks device identity, boot state, active writers and the stock recovery image; it does not overwrite an active write.
5. After the actual stock screen appears, use **Stock restored on device · verify → install Bootstrap → Keep stored data**. Disconnection alone does not confirm stock return.

If active writes, trial boot or a recovery-image error block this route too, preserve the reported cause and logs. [Physical Gate entry](10-Recovery.en.md) is a separate path when the phone cannot connect.

## Pairing approved, but Couldn't Pair keeps returning

1. Open **Connection recovery → Completely reset this Noodoe connection**. Grant Bluetooth/Nearby devices permissions when requested.
2. When the guide asks, forget **only this Noodoe** in Android Bluetooth settings. Keep app data and installation records.
3. Use **UP** on the device's installation/trial connection-recovery prompt to open re-pairing. During ordinary CFW operation, use **Settings → Connections → Re-pair phone**. Check Noodoe button-selector mode and stationary requirements for ordinary menu access.
4. Select your phone on the device, then **release O and hold it freshly for 2 seconds** to approve. `ALL` removes every phone key; choose it only when that is intended.
5. At `Ready to pair on your phone`, mark the device ready in the app, choose **Pair and verify connection**, and approve Android's prompt.
6. Continue the same operation after the app verifies secure connection, device identity and saved key. Existing files are reused only after verification.

Stock firmware uses its own pairing controls. Gate cannot re-pair. The dedicated recovery screen can operate in dashboard-button mode; distinguish that from entering ordinary settings.

## Bluetooth startup-error choices

| Choice | Meaning |
|---|---|
| RETRY | Restart Bluetooth at the current speed; retain keys |
| TRY STANDARD SPEED | Retry Product at 921,600baud |
| RESET PAIRING | Choose/reset stored phone keys |
| FIRMWARE RECOVERY | Open independent Gate recovery |
| DETAILS | Inspect initialization, baud, error and key-persistence status |

Bootstrap uses standard speed. Product's MCU-to-Bluetooth-chip UART speed is different from actual wireless throughput. Re-pairing, display reinitialization, MCU restart and stock return are also distinct actions. For the installation error screen's UP+O 3-second instruction, follow [recovery](10-Recovery.en.md).
