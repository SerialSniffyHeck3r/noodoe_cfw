## 0.11.13: Bootstrap message layout and manual connection handoff

Use the matching **NewDoe-0.11.13.apk + NewDoe-CFW-0.11.13.zip**. Product must also be updated to **0.11.13** for the ODO/UART fixes. Devices with the same Gate (0.11.4 onward) use the existing CFW update route. Gate and B language resources are unchanged. The Bootstrap display fix applies to new stock installations with this ZIP; do not return to stock just to change that temporary screen.

From 0.11.13, ODO storage, correction and replacement with an older value are removed. ODO displays the current valid dashboard UART reading. Missing data is shown as unknown until reception resumes. The key-OFF display may retain this running session in RAM only. TRIP A/B, today/session trips, oil, belt, service records and settings remain preserved. The ODO check menu is removed; old correction records are neither read nor rewritten.

After the stock transfer has been verified, the app closes the connection and waits. Keep module power connected. Follow the stock screen: IGN OFF, wait for the NewDoe Bootstrap menu, then IGN ON. Tap **Check current status** in the app. On Noodoe choose **Install CFW** and answer **Reset Bluetooth?** (default **No**, preserves pairing). The app waits for this local choice on the same connection, then continues the existing preparation/verification/install flow without a third phone button. If already in the installation screen, it proceeds as soon as local permission and readiness are verified. The existing separate restore-data installation choice remains available; an existing selected restore policy is retained.

Transfer verified is not CFW installed. The final on-device confirmation/countdown and Product boot confirmation remain required. A connection failure or an uncertain commit does not authorize resending Bootstrap or silently starting a new installation. If pairing fails, use the existing optional Bluetooth recovery guide and check the device again.

The developer message keeps all original text and page controls. Its left/right placement now uses the correct EVE text alignment bits. Progress displays fresh acknowledged transfer bytes rather than an old device job; internal work is sampled every 250 ms and the screen refreshes every 200 ms. Repeated progress ticks no longer rebuild unchanged settings controls. These intervals are software scheduling targets, not a physical Bluetooth latency guarantee.

Software verification only; no physical LCD/RF/installation qualification this turn. Keep module power stable. The previously accepted old-code stock-return power-interruption limitation remains.

## 0.10.3 · Optional Bootstrap help

Choose **Cannot connect to Bootstrap?** for the explicit phone unpair → Bootstrap UP, select your phone, release O and hold 2 seconds → phone re-pair → Check current device / Continue from here sequence. Opening help alone changes nothing. Connection interruption is disabled during firmware transfer/writing. Existing automatic connection and Bootstrap code are unchanged.

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
