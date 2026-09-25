# 6.8.1 update compatibility correction

## Observed code paths

`NdcpSession.identity()` previously required the canonical448KiB running APP SHA to equal the selected ZIP. That is appropriate for the exact Bootstrap installation and automatic post-reset candidate confirmation. `ProductBootSession.confirm()` was also used by manual `cfw-verify`; a healthy older Product therefore failed with `Running APP does not match bundle` when a newer ZIP was selected. Recovery wizard exposed only that verification button for Product and no update/resume action, trapping the user in the wrong check.

RoutineUpdateSession already allowed source Product SHA to differ from the candidate. The correction keeps that protocol path and makes it reachable: manual inspect reads confirmed current firmware independently, while automatic confirmation remains exact. An outstanding locally recorded candidate/remote update cannot be cleared by showing an older healthy firmware.

The new ZIP includes optional manifested `uninstall.bin`, introduced in6.7. A pre-uninstall importer calculates a smaller allowed set and rejects it as `Unmanifested or missing files`. Current6.8.1 retains strict per-role SHA/layout/entry validation and supports both schemas (optional uninstall present/absent). It does not ignore arbitrary extra files.

## Additional compatibility boundaries

- Shared runningIdentity handles startup busy and validates status/role/schema.
- Gate identity inspection and candidate Gate equality are separate. Routine updates require the exact compatible Gate as before. Read-only current inspection and KEEP recovery do not require the selected future Gate to already exist.
- First-install factory backup remains mandatory on Bootstrap. Existing Product on a replacement phone can instead durably bind reported BL hash to UID, after Gate identity checks; this does not recreate a missing raw backup. Any available original full backup remains authoritative. Later hash changes reject. No BL/factory writes were added.
- Unknown transaction results, changed UID, unsupported protocol, or a Gate requiring migration are not treated as ordinary version-string mismatches. Future or historically incompatible partition/protocol revisions cannot be guaranteed a direct Product-only update by an APK change.
- Phone logs, original files, transactions and pairing are preserved. New APK has the same signing certificate and a higher versionCode13.

## Validation / provenance

`verification-summary.json`:209 Android tests,0fail/error/skip; lint0error/82existing warnings; same signer; seven published package imports accepted. Tests cover healthy old-source inspection then Product update, strict new-candidate rejection, ambiguous work preservation, replacement-phone binding, raw-backup precedence and different-Gate KEEP recovery.

The first test attempt exposed a missing Robolectric service stub in the new UI test; fixed in the fixture. A subsequent run failed due to Windows disk exhaustion from accumulated synthetic128MiB `expected.bin` NOR fixtures. Only matching test-prefix files under the resolved Windows Temp directory were removed, with paths recorded in `removed-temporary-test-images.json`. No project evidence, real NOR backup, phone log or device data was deleted. The completed build after the Android6-compatible hash sentinel check is in `android-build-release.log`.

No GUI/ST-LINK/UART/ADB/RF device access, firmware installation or physical recovery test in this turn. Firmware ZIP is byte-identical to the preceding flash-optimization release.
