# Bootstrap automatic pairing regression — 2026-09-24

## Observed request and code findings

The rider clarified that the phone app no longer starts pairing after stock firmware installs Bootstrap. This is the stock-to-Bootstrap boundary, not the later Bootstrap-to-Product trial handoff.

The previous APK rejected a non-bonded address in ordinary `AndroidInstallerTransport` before creating any socket or requesting pairing. Version 6.9.4 allowed implicit secure-socket pairing only for `openBoot`, used by the later Product trial handoff. Stock transfer still returned directly after `STOCK_ACCEPTED_WAIT_IGN_OFF`. A manually deleted bond also removed the selected address from the spinner on resume. These are confirmed code paths; no phone log or RF trace was available to attribute every reported failure.

## Changes

- `BondSession` owns the bounded public bonding lifecycle. `AndroidBondSession` registers a selected-address receiver before `createBond()`, checks actual framework bond state, and unregisters on every exit. Already bonded peers are untouched; an existing BONDING operation is awaited. Rejection and interrupted waits stop without repeated dialogs. No hidden APIs, bond deletion, insecure sockets, or automatic acceptance were added.
- Ordinary connections and boot handoff now request pairing when needed. The 60-second approval wait occurs before the 15-second secure RFCOMM deadline. Subsequent connection retries recheck the bond state rather than assuming it survived stock firmware replacement.
- `BootstrapConnectSession` owns read-only, bounded reconnection after stock transfer and the explicit `bootstrap-connect` action. The stock socket is closed first. It uses the boot connection path, checks role, exact Bootstrap package image, UID and existing BL binding, and preserves transaction evidence. It never sends firmware, erase, COMMIT or RESET commands. Identity mismatches and pairing rejection terminate immediately; an absent radio is retried at most five times within a 240-second scheduling window, plus an already-running bounded connection operation.
- The wizard exposes `Bootstrap 자동 연결` for a device already running Bootstrap, retains a removed-but-selected Bluetooth address, and preserves the newly verified Bootstrap role. A CDM appearance cannot run the Product companion protocol while a Bootstrap/installation workflow is known or unresolved.
- Existing Product trial health and confirmation, storage boundaries, Gate recovery, and all firmware binaries remain unchanged. This APK accepts the same current and older bundles; no Bootstrap reflash is necessary to apply these app fixes.

## Verification

See `verification-summary.json` and `android-build.log`. Tests cover a new bond that completes after 25 seconds, existing bonds and pending pairing, refusal, 60-second timeout, cancellation, delayed Bootstrap availability, wrong UID/image/role, unknown-commit preservation, and the full simulated stock transfer followed by Bootstrap identity. Existing Product handoff, journal and installer tests remain in the suite. APK signer and package import matrix are checked using the production build.

Tests are JVM/Robolectric and framed-protocol simulations. Firmware was not changed or rebuilt; memory budgets inherit the exact 6.9.4 binary. No GUI, ST-LINK, ADB, connected bench, real phone radio or vehicle was accessed. Android OEM pairing behavior and on-vehicle reconnection remain to be observed.

Official API reference: https://developer.android.com/reference/android/bluetooth/BluetoothDevice#createBond()
Android may require rider approval; initiating pairing does not bypass that approval. On-device Bootstrap still opens pairing after its local Install CFW selection.
