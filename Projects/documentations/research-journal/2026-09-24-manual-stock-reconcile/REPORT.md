# Physical stock return reconciliation — Companion 6.9.3

The user returned to stock with the physical Gate gesture after Product's trial-boot phone reconnection failed. The device was running stock, but the phone journal remained `WAIT_CFW_BOOT`.

## Cause

`InstallerController` only allowed `stock-return-check` after an app-originated recovery/uninstall request or an already confirmed stock return. Button-only restoration never writes those phone states. The recovery UI also hid the stock-return check whenever its cached role was still Product or unknown. Restarting the UI or forgetting a Bluetooth pairing cannot reconcile this durable journal.

## Correction

- `StockReturnSession` performs the real stock raw/framed identity exchange. The selected peer address and bundle's stock version, BL version, hardware, model and PCBA must match before the phone journal changes.
- Trial-boot wait/confirmation and explicit supported post-install/recovery states can transition to `STOCK_RETURN_CONFIRMED`. This ends a failed/abandoned CFW boot wait; it does not label that CFW as successfully installed.
- The prior checksum-protected journal is archived intact, the physical stock identity response is durably written and read back, and its digest is recorded. Existing UID bindings, original backups, transaction evidence and logs remain.
- Stock verification sends only read/handshake packets. It never sends ABORT, COMMIT, RESET, firmware data or a NOR erase. A subsequent explicit install retains the existing fresh stationary/IGN preflight and new-attempt archive.
- A read-only stock-return check can be reached even if another ZIP has an unresolved journal. It only resolves the selected attempt. Unrelated pending attempts continue blocking writes; no bulk clearing is performed.
- Unknown FAT creation/resource operations and uncertain stock upload states are not made safe by observing a running stock APP. Those records remain unresolved.
- The troubleshooting wizard exposes **본체에서 순정으로 돌아왔어요 · 확인** even with a stale Product/Bootstrap/unknown role. On verified success, the existing workflow clears its persisted pending UI state and returns to stock setup.

## Validation

The release verifier requires 239 Android unit tests, including 14 stock/reinstall tests and two physical-return wizard tests, all with zero failures, errors and skips. Coverage includes each supported boot-wait family, incorrect peer/hardware/version, non-stock or missing responses, archive failure, repeated read-only verification, explicit reinstall after restoration, preservation of the old record, another bundle remaining unresolved, and stale UI roles.

APK 6.9.3/code17 retains the existing signing certificate. Production package importer checks the matching ZIP plus 6.9.1/6.8.1/6.4. Lint must have no errors. Exact results and artifact hashes are in `verification-summary.json`.

The accompanying firmware ZIP is byte-identical to 6.9.2 (`04eaedf5215e0dea99c0085a04e7df754cb4319ba7fdf4d5c560ee975b8f66d6`). No firmware, Gate, Bootstrap, pairing or watchdog code changed. This release does **not** add a button override for Product trial confirmation or fix the vehicle's reported Bluetooth reconnection failure. It fixes the phone state after physical stock restoration.

No hardware, vehicle, ST-LINK, ADB or GUI access occurred. Protocol tests use fake transports; UI tests use Robolectric. Real phone/vehicle reconciliation remains to be confirmed.

## User steps

1. Install APK 6.9.3 over the existing app. Keep app data/logs.
2. Select the same Noodoe and retain the ZIP used for that attempt (the 6.9.2 ZIP and bundled 6.9.3 ZIP have the same hash).
3. In the troubleshooting wizard choose **본체에서 순정으로 돌아왔어요 · 확인**, or **순정 복귀 확인** when stock is already identified.
4. When a fresh matching stock response is received, the CFW boot wait ends. No firmware retransmission is required to repair this phone state.

If connection/identity verification fails, the journal remains pending and the app reports the failure instead of claiming restoration. App data deletion is not a workaround.
