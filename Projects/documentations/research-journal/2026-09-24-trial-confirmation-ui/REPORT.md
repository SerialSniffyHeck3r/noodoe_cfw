# Trial confirmation UI / CFW 6.10.3

## Evidence and cause

User reports CFW→CFW boot verification rolls back even after Bluetooth connects; user did not see the confirmation button. Before this change `ProductBootSession.confirm` called `VisualInstallConfirmation.require`, which blocked in `MaintenanceService.awaitScreen` until the explicit user action. `InstallerHomeActivity` put the action after the entire scrolling progress block without scrolling or an alert. Meanwhile `product_install_view.c` chose `Checking the new version` solely from SPP link status. The device-owned 180-second trial deadline continued to run.

This is a deterministic hidden-required-action defect. ARM `Test_LinkWithoutApproval` and timeout mode3 reproduce a live control connection plus injected successful health callback remaining unconfirmed, with the timeout producing the existing Gate trial failure reason. It is not evidence that every field report has exactly this cause; no vehicle trace was collected. The 30-second health callback is injected in this harness, not measured as a physical task run.

## Implementation

- `BootConfirmationView`: independent pinned view outside the main ScrollView plus foreground AlertDialog. Ready-only, explicit action, localized deadline, foreground re-entry, request-bound clicks. Negative/back dismissal does not authorize anything.
- Maintenance service still owns the single socket/worker. Old candidate/deadline/connection-epoch callbacks cannot approve a new request. Foreground notification names the action and the screen stays awake while awaiting it. UI recreation does not restart transfer.
- `ScreenCheck` forwards through `LoggedInstallerProgress`; while waiting the worker reads status once per second. Response failures/changed candidate/rollback exit and dismiss the request. No extra sender, COMMIT, RESET, image retransmission or timeout extension.
- Distinct `screen-confirm` progress and user-wait metric; timeout has its own exception and no fake Bluetooth retry loop. Requested/received/interrupted events persist in the existing session log; journal records the waiting condition and removes it on confirmed completion.
- `AppRecovery_TrialPhase` is used both for presentation and the trial confirmation gate. Checks remain health, no system error, no boot-store error, Bluetooth ready, live NDCP connection and matching acknowledged epoch. `healthy_pending=2` represents a confirmation request already posted and keeps the health result visible until the journal completes; only value1 posts a request.
- Product screen is no longer based on SPP-UP alone. Distinct action, health and save labels. No vendor/Cube/main/bootloader/flash-layout changes.

## Verification

See `verification-summary.json`, `trial-arm.json`, final Android log and Release/Debug logs. 84 Android tests, 14 ARM O0/Os cases. The native Robolectric images are application-rendered previews; they are not physical S24 screenshots. Initial test harness failures (unflushed UI looper, unattached dialog scroll indicator) were corrected; lint found API24 Consumer under min23, replaced with an owned functional interface. Final lint has zero errors.

New and previous ZIP pass the real Android importer. Gate, Bootstrap, original APP, resources, uninstall and diagnostic bytes match the preceding bundle. Only Product APP changes. Existing APK signer is preserved. Firmware memory minimums are unchanged: Release free65568, Debug free33716; SRAM free52512/53480; CCM free16320; FreeRTOS heap48KiB.

No ST-LINK, UART, USB, ADB, computer-use or real vehicle/phone interaction occurred. Real RF end-to-end completion and measured FPS were not tested. Tests do not prove that arbitrary hardware/BT failures can be bypassed. Existing independent recovery and rollback remain enabled.
