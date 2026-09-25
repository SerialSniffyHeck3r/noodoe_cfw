# Bootstrap photo-container reuse correction — 2026-09-24

## Observed failure

The vehicle screenshot reports `00050006`, command `56`, state `9`, file `3`, phase `9.2`, and `1048576/1048576 B`. File 3 is the 1MiB `CFWPIC.DAT` photo container. `BS_FORMAT=6` is a format-validation rejection after reading the container. The separate 753,664-byte progress value was a stale UI sample, not the device's final read position.

The actual vehicle's NOR bytes have not been read in this task. The code defect below reproduces the reported failure at this stage; the tests do not establish a forensic diagnosis of every byte on that vehicle.

## Root cause and correction

Product's `PhotoStore_ResetSlots()` invalidates each of the six A/B photo-bank headers by erasing its 4KiB header. Obsolete JPEG body bytes remain. Product's normal boot scanner correctly treats an erased header as an empty bank.

Bootstrap instead required the entire 160KiB bank to be erased before accepting an empty bank. Consequently, a normal photo reset followed by stock return and CFW reinstallation could reject Product's own valid empty-photo representation.

`bootstrap_storage.c` now checks the 4KiB bank header for the empty condition, matching Product's writer and scanner. The UID-bound outer container, committed records, schema, JPEG size and body CRC checks remain. A partially erased or invalid bank still requires a valid partner; two invalid, nonempty headers are rejected. No extra erases, automatic formatting, arbitrary overwrite or bypass of failed validation was added.

The APK identifies the file as `CFWPIC.DAT` in error details and updates the failed offset from the device's progress snapshot. A completed read is not presented as a successful validation or installation.

## Validation

- Actual Product ARM `TestPhotoReset`: 25 assertions, including interruption/reboot between header erases. Its resulting logical photo container is exported as a regression fixture.
- Product and Bootstrap tests mock different silicon UIDs. Only the fixture's outer container UID and header CRC are adapted to the Bootstrap fixture. Every photo-bank header and body byte is preserved exactly.
- Preserved pre-fix Bootstrap ARM source: 20 cases, reproducing format error 6 for the actual Product reset result and synthetic resets after each of the six header erases.
- Fixed Bootstrap ARM, `-Os` and `-O0`: the same 20 cases per build. Reset slots pass; wrong UID/schema/length/magic/CRC, uncommitted headers and partial erases are rejected. A valid A bank with torn B remains usable. Every inspection checks zero mutations and unchanged whole 128MiB simulated NOR.
- Scoped-v2 install regression covers generated files, Product sent once, device-side copies, existing stock preservation, interrupted reseeding and draining writes after disconnect. See `bootstrap-v2-results.json` and `verification-summary.json` for completed results.
- Android: 230 unit tests, no failures/errors/skips; lint has no errors. Includes failed-photo progress and existing stock-return/reinstall regressions.
- APK uses the existing signer, version 6.9.2/code16. Production ZIP importer accepts this bundle and older 6.9.0/6.8.1/6.4 bundles.
- New package differs from 6.9.0/6.9.1 only in `bootstrap.bin` and `manifest.properties`. Product, Gate, resources, stock recovery, Diagnostic and Uninstall images are byte-identical.

Bootstrap occupies 453,292 bytes, leaving 5,460 bytes in its 448KiB execution region. Product Release/Debug and other image budgets are unchanged and checked by the bundle builder.

All firmware tests here use ARM emulation with memory-backed NOR and host-accelerated SHA/memory primitives. They do not measure physical NOR timing, RF behavior or power-loss timing. No bench, ST-LINK, ADB, vehicle or GUI was accessed. Actual vehicle reinstallation remains to be confirmed.

## Installing the correction

The rejecting validator runs in Bootstrap, so updating only the APK cannot fix a Bootstrap already installed on the vehicle.

1. On the existing Bootstrap, use Back to stock; release O and newly hold it for 2 seconds. Wait for stock to boot.
2. Install APK 6.9.2 and select the matching **6.9.2 Photo Reuse Fix ZIP**.
3. Transfer the new Bootstrap from stock, then continue CFW installation with that same ZIP.

Do not continue the old ZIP on the old Bootstrap expecting the new APK to alter its validator. Retain logs and recovery evidence; app data deletion is not part of this correction. If the corrected Bootstrap still reports an error, collect its diagnostic export rather than repeatedly provisioning storage.
