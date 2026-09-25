# Bootstrap → Product Bluetooth handoff

The reported sequence is installation approval, a radio-less installer/Gate interval, then the Product startup-check screen without an automatic phone connection. The vehicle was not accessed. These are code-confirmed defects; their individual contribution to the user's RF failure cannot be established without that phone's logs.

## Root causes and changes

1. Bootstrap used the common in-memory Bluetooth key DB but did not load or persist its keys. Product imports its keys from `CFWCFG.DAT`. Android's bond therefore could survive while the matching device key disappeared across installation. Bootstrap now restores audited keys, preserving any newer live-session key, and journals the current key set before autonomous installation starts.
2. The new journal writer uses the existing CFG format and 152-byte Bluetooth codec. It audits current FAT ownership, validates committed records and the TLV schema, preserves unknown fields, follows fragmented cluster chains and modifies only the next journal sector. The latest valid record stays intact. It reads back the physical body before programming the completion marker, then reads back the completed record. FAT, stock files, factory data and the recovery image are not modified. A newly negotiated key during writing causes a fresh save before installation can proceed. Keys never enter phone logs or NDCP responses.
3. The old app immediately spent Product reconnect attempts after local approval, even while Bootstrap was still executing. Each attempt nested three socket attempts. `BootHandoffSession` now reads the actual role and UID, waits for the exact Bootstrap without treating that as a Product failure, and invokes the existing exact-candidate confirmation only after Product is identified. Five consecutive failed reconnect attempts remain bounded, with one physical secure RFCOMM attempt per outer attempt. The installer wait has a 240s ceiling; Product confirmation gets one 175s deadline after first detection, not a fresh deadline on every reconnect. Device-side 30s health and 180s trial rollback rules remain unchanged.
4. Secure reconnect refreshes SDP and uses the same selected MAC. If the user deleted the Android bond during the transition, secure RFCOMM may invoke Android's public pairing dialog instead of being rejected immediately by our constructor. The app neither removes bonds nor uses insecure SPP or hidden pairing APIs. User approval of a system pairing prompt cannot be bypassed. See [Android's RFCOMM pairing behavior](https://developer.android.com/develop/connectivity/bluetooth/connect-bluetooth-devices).
5. The pairing window starts when HCI actually reaches READY. Product allows 180s at startup; Bootstrap's explicit local pairing window remains 120s. The fixed `30 seconds` text was replaced by actual controller/link state: starting Bluetooth, waiting for the phone, or checking the new version. That previous text was not a countdown.

The owner task continues servicing power, UI and health while saving keys. A confirmed local install may continue after SPP disconnect. Pending pairing persistence blocks new mutations; local cancellation/recovery drains it first. A persistence error prevents installation from proceeding. Existing emergency key+O rescue and independent Gate are unchanged. No manual bypass of the Product health/phone confirmation was added.

## Verification

- Android: 244 tests, including Bootstrap-alive polling, exact Product confirmation, wrong UID terminal rejection, five bounded physical attempts, interruption, and absence of COMMIT/RESET replay. Existing state-reset and manual stock-return tests remain.
- Actual ARM O0 and Os: 20 erase/page/body-readback/commit interruption points per build, corrupt CRC/UID/future schema refusal, key round-trip, live rekey during writing, duplicate suppression, unknown settings and older peer preservation, fragmented CFG allocation. NOR is memory-backed; SHA/CRC and memory-copy primitives are accelerated by host equivalents. This does not model physical flash timing or power loss.
- Shared BT transport: actual ARM O0/Os scenarios, including a READY-delayed 180s pairing window and expiry. HCI, hardware and RTOS scheduling are mocked.
- Existing Bootstrap UI/rescue and Product trial tests are rerun. Final APK signer, production ZIP importer, component hashes and Release/Debug memory budgets are recorded in `verification-summary.json`.
- Gate, stock, resources, uninstall and Diagnostic package payloads are unchanged. Bootstrap and Product are rebuilt. No GUI, ST-LINK, ADB, bench or vehicle interaction was performed.

Actual phone/vehicle reconnection and elapsed installation time still require RF verification. Publishing Latest does not represent that verification.

## Applying this release

Install APK 6.9.4 and select the matching 6.9.4 ZIP. A device already returned to stock must receive that ZIP's new Bootstrap; the APK cannot retrofit persistence into an older resident Bootstrap. Do not mix a currently running Bootstrap with a different ZIP. Installation approval remains on the device. The app then follows installation and startup automatically. Keep the app running; if an Android pairing prompt appears after a user-deleted bond, approve it.
