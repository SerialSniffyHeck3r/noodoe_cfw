# User manual / capture refresh — 2026-09-24

## Deliverables

- Thirteen Korean manual pages covering controls, phone setup, all riding pages, every settings category and option, power/warnings, first install, ordinary updates, Gate/erase/diagnostics, and troubleshooting.
- 55 fresh 480×480 bench EVE screenshots and 19 Android Activity screenshots. Old draft images are replaced at their documentation paths.
- Read-only manual link on the installer home screen, localized into the six existing language variants. Companion 6.10.1/code24 retains the existing signing certificate. No Product, Gate, Bootstrap, Uninstall, Diagnostic or resource payload changes.
- Matching installer ZIP is byte-identical to CFW6.10.0 (`0e52b5804ab9de20824ff3c6471b41f84116c146f5e67205ddd0f2bf1af3822e`). No firmware reinstall is needed solely for this manual update.

## Capture contract

The current release renderer is executed with bounded sample models in an ARM harness. Its actual EVE command lists and texture RAM are replayed through ST-LINK onto the connected bench EVE. CMD_SNAPSHOT2 reads the resulting RGB565 pixels. This tests pixel output, not a full running current APP, UART injection, radio delivery, frame rate, or an installation transaction. The bench APP identity is checked before temporary RAM execution and restored CPU/RAM/GPU state afterward. No APP flash write is performed for screenshots.

The clock is10:08. ODO is chosen deterministically from892,6974,13946,142857km. Stationary scenarios use0km/h; moving settings-lock uses a nonzero sample. Trips and moving speeds use illustrative values. Reserve1.1km and24.8km demonstrate the over20km yellow value.

The user-supplied Kuromi JPEG was committed to **photo slot1** through the firmware's existing PhotoStore SWD mailbox. Firmware-reported physical readback and complete JPEG decoding succeeded. FAT metadata and APP flash were not rewritten. General screenshot wallpaper is Kuromi; music uses separate albums and does not replace the stored wallpaper:

- まねきケチャ: 君のいない世界に / あるわけないの, first-edition B artwork with five members (Nippon Columbia COCP-17696).
- Rick Astley: Never Gonna Give You Up / Whenever You Need Somebody (Apple Music album1559885420).

Current Android MusicTextTiles supplies the title/artist masks; long English title capture represents the beginning of a scrolling title, not forced font shrinking. No audio/lyrics are distributed. Music source links and screenshot fixture limitations appear in the manual.

## Validation

- `android-full.log`: testDebugUnitTest + assembleDebug + lintDebug succeeded before the final capture-only test additions.
- `android-capture-final.log`: all four final manual/navigation/capture tests passed. A dialog capture NPE in Robolectric was removed by checking dialog visibility rather than presenting an unattached dialog render as a real screenshot. Production PermissionCenter was not modified.
- An intervening full test rerun exhausted C: with pre-existing synthetic128MiB fixture files. That environmental failure is not counted as a passing run. The later targeted run avoids those large fixture tests.
- APK signature verified against the previous release. No lint Error/Fatal. Manual local links/images and fresh image SHA256 values checked by `finalize_manual.py`.
- Capture command streams validated before target access. SRAM/SDRAM and GPU restore verified; the final successful capture manifest and restore record are retained.
- Earlier capture harness faults/short reads and disk-full recovery evidence remain in the local analysis directory. They are not omitted or reported as production firmware faults.
- No new Bluetooth, real vehicle, real installation or power-cut validation was performed.

## Remaining environment limits

The user explicitly authorized deleting five Windows Temp synthetic test folder families. The execution tool rejected the scoped deletion command with `blocked by policy`; no Temp files were deleted and no alternate-shell workaround was attempted. Disk pressure remains. Backups, evidence, sources and toolchains are preserved.

The repository is private and reports `has_wiki=false`; attempts to enable Wiki did not activate it. Documentation is published to `docs/manual` in that same private repository, with a link from the APK. Repository visibility is unchanged. This is a repository manual, not a successfully created GitHub Wiki.
