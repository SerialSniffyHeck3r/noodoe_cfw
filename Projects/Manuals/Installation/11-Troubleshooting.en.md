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
| Cannot apply an ODO value | Use Settings → Vehicle → ODO check; require IGN ON and 5 seconds of valid stationary speed. Back/long O exits. ODO* is a saved-distance adjustment, not a change to the vehicle's original reading |
| Only LCD output is wrong | Display reinitialization in quick settings. See how it [differs from MCU restart](10-Recovery.en.md) |
| GPS Waiting / no map | Precise-location permission, service, IGN, fresh fixes, imported region and map enablement on both sides |
| Music/artwork delay | Notification access, player MediaSession and background restrictions. Distinguish current-track cache reuse from new/missing content transfer |
| Phone Permission Denied | Contacts, call history, outgoing calls and companion call control require separate permissions |
| Storage error while the display works | Inspect storage status and last successful save. Request Retry failed saves and inspect its result. A live display does not prove a durable write |

## Preserve records while recovering

Resetting app connection state clears sockets and temporary selections. It does not automatically send device ABORT/RESET or confirm cancellation of an unresolved operation. Do not bypass records by clearing app data, formatting NOR, changing ZIPs or substituting device identity. Choose [recovery](10-Recovery.en.md) from the actual screen.

Export the diagnostic ZIP using **Share diagnostics** and include: **vehicle model; module HW/PCBA/bootloader/stock version when available; old/new APK and CFW versions; device wording, Stage and Code/phase; IGN state and buttons used; last file/sector/offset**. Avoid publishing sensitive device identifiers in an open issue.

Normal event logs omit notification bodies, track names, precise coordinates and pairing secrets, but recovery evidence can contain device information. Internal originals are retained. The phone also needs space for mandatory journals. Use the normal APK's export feature; development-APK `run-as` instructions are a separate workflow.
