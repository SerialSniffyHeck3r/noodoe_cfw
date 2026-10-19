## 0.11.13: Bootstrap message layout and manual connection handoff

Use the matching **NewDoe-0.11.13.apk + NewDoe-CFW-0.11.13.zip**. Product must also be updated to **0.11.13** for the ODO/UART fixes. Devices with the same Gate (0.11.4 onward) use the existing CFW update route. Gate and B language resources are unchanged. The Bootstrap display fix applies to new stock installations with this ZIP; do not return to stock just to change that temporary screen.

From 0.11.13, ODO storage, correction and replacement with an older value are removed. ODO displays the current valid dashboard UART reading. Missing data is shown as unknown until reception resumes. The key-OFF display may retain this running session in RAM only. TRIP A/B, today/session trips, oil, belt, service records and settings remain preserved. The ODO check menu is removed; old correction records are neither read nor rewritten.

After the stock transfer has been verified, the app closes the connection and waits. Keep module power connected. Follow the stock screen: IGN OFF, wait for the NewDoe Bootstrap menu, then IGN ON. Tap **Check current status** in the app. On Noodoe choose **Install CFW** and answer **Reset Bluetooth?** (default **No**, preserves pairing). The app waits for this local choice on the same connection, then continues the existing preparation/verification/install flow without a third phone button. If already in the installation screen, it proceeds as soon as local permission and readiness are verified. The existing separate restore-data installation choice remains available; an existing selected restore policy is retained.

Transfer verified is not CFW installed. The final on-device confirmation/countdown and Product boot confirmation remain required. A connection failure or an uncertain commit does not authorize resending Bootstrap or silently starting a new installation. If pairing fails, use the existing optional Bluetooth recovery guide and check the device again.

The developer message keeps all original text and page controls. Its left/right placement now uses the correct EVE text alignment bits. Progress displays fresh acknowledged transfer bytes rather than an old device job; internal work is sampled every 250 ms and the screen refreshes every 200 ms. Repeated progress ticks no longer rebuild unchanged settings controls. These intervals are software scheduling targets, not a physical Bluetooth latency guarantee.

Software verification only; no physical LCD/RF/installation qualification this turn. Keep module power stable. The previously accepted old-code stock-return power-interruption limitation remains.

# Troubleshooting and diagnostics

**APK / Product 0.9.66 · Manual revision 2 · 2026-10-07**

[Installation index](README.en.md) · [한국어](11-Troubleshooting.md)

## When installation appears to stop

Read the app's overall stage, current-operation bytes, last reply and file/sector/offset together with the device screen. Transfer, hashing, NOR erase, stock writing and candidate boot are separate operations. A percentage alone cannot establish remaining time or a bricked device.

| Message or symptom | Meaning and next action |
|---|---|
| Compatibility stopped before transfer | Check HW 0, BL 0.10–0.19, stock 5.16 and matching APK/ZIP against the [support scope](07-Compatibility.en.md). Model/PCBA are not restricted. Stopping at this check has not transferred Bootstrap |
| `The current operation cannot continue` / `Installation success or cancellation not confirmed` | A previous change has no confirmed outcome. Inspect the last operation on the same device and ZIP first |
| Stage 6/8 · Code 6 / Phase 4 · `Paused` / `See Phone Help` | The code alone proves neither success nor a brick. Preserve screen/logs and follow [paused-installation recovery](10-Recovery.en.md). The 0.9.66 error screen gives the UP+O 3-second instruction |
| 100% transfer or `NOODOE INSTALLER` | File-transfer/stock-writing stage. Do not approve completion before the actual new CFW screen |
| `CFW UPDATE` / `Checking the new version` | Confirm the actual screen, reconnect to the same candidate, complete at least 5 seconds of healthy execution and permanent confirmation. Current maximum 5 minutes; older firmware reports its own deadline |
| `Update wasn't confirmed` | Candidate confirmation failed. Inspect rollback or Gate state |
| Stock screen but the app still waits for CFW | Use `Stock restored on device · verify` to read and reconcile actual stock identity |
| No Bluetooth in Gate | Expected role difference: Gate has no Bluetooth. Use the local menu |
| Bootstrap is already running but the phone cannot connect | Do not resend through stock; check role/address and approve Android pairing. See [connection recovery](12-Connection-Recovery.en.md) |
| `00050006` or a file-verification error | Preserve file number, phase and offset. File 3 / phase 9.2 concerns photo-container checks. Do not format NOR based on the code alone |

The user reported successful stock-to-CFW installation and fuel-warning dimming in 0.9.59. This is not proof that every CFW-to-CFW migration or Code 6 / Phase 4 has the same cause. An update starting on an older version follows that version's behavior through the first restart.

## Everyday operation

| Symptom | Check |
|---|---|
| Normal page buttons do nothing | Dashboard/Noodoe selector, blue dashboard indication, held keys and stationary restrictions |
| Missing or stale ODO | Update both APK and Product to 0.11.13. Correction is removed; only current UART readings are used. Persistent ------ needs dashboard-link diagnosis |
| Only LCD output is wrong | Display reinitialization in quick settings. See how it [differs from MCU restart](10-Recovery.en.md) |
| GPS Waiting / no map | Precise-location permission, service, IGN, fresh fixes, imported region and map enablement on both sides |
| Music/artwork delay | Notification access, player MediaSession and background restrictions. Distinguish current-track cache reuse from new/missing content transfer |
| Phone Permission Denied | Contacts, call history, outgoing calls and companion call control require separate permissions |
| Storage error while the display works | Inspect storage status and last successful save. Request Retry failed saves and inspect its result. A live display does not prove a durable write |

## Preserve records while recovering

Resetting app connection state clears sockets and temporary selections. It does not automatically send device ABORT/RESET or confirm cancellation of an unresolved operation. Do not bypass records by clearing app data, formatting NOR, changing ZIPs or substituting device identity. Choose [recovery](10-Recovery.en.md) from the actual screen.

Export the diagnostic ZIP using **Share diagnostics** and include: **vehicle model; module HW/PCBA/bootloader/stock version when available; old/new APK and CFW versions; device wording, Stage and Code/phase; IGN state and buttons used; last file/sector/offset**. Avoid publishing sensitive device identifiers in an open issue.

Normal event logs omit notification bodies, track names, precise coordinates and pairing secrets, but recovery evidence can contain device information. Internal originals are retained. The phone also needs space for mandatory journals. Use the normal APK's export feature; development-APK `run-as` instructions are a separate workflow.
