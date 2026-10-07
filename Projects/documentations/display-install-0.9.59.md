# 0.9.59 display and installation validation

The candidate changes the warning compositor, manual map range and F4 installation
admission, and fixes a reproduced local update reset failure. Matching Android
versionCode89/versionName0.9.59 and Product images are software-tested. Physical
LCD, ignition, radio and installation validation remains pending.

## Warning compositor

The previous ordinary LVGL popup backdrop did not resolve the reported ignition
warning on the vehicle. The new implementation has no LVGL backdrop object.
After ordinary LVGL drawing, before viewport composition and the validated EVE
swap, a final pass explicitly initializes alpha, stencil, scissor, vertex,
color-mask and blend state. It draws a full-screen black rectangle with alpha235,
then the existing rounded fuel vector, warning symbol and32px text. The veil is
independent of icon blinking. Existing icon geometry, fonts and phase timing are
preserved; no new framebuffer or GPU bank is allocated.

Software pixel checks cover36 dimmed frames and9 expired frames, spanning riding,
welcome and parked-return conditions and all three warning levels. Eighteen
injected alpha/vertex, blend/color-mask and stencil/scissor faults produce the
same final pixels as the baseline. This establishes state isolation and draw order
in the software renderer; it does not prove the cause of the original vehicle
failure or substitute for an ignition test.

## Map behavior

AUTO/MANUAL and automatic preference labels use20px instead of16px text. The
existing road classification receives more distinct RGB444 colors for motorway,
trunk, primary, secondary, tertiary, residential and service roads. Geometry,
road admission, tile protocol and cache sizes are unchanged.

Manual range changes apply immediately, including when the target tiles are not
ready and when interrupting an automatic transition to the same target. The
retained map is transformed while missing detail is prepared. Automatic range
keeps its readiness check and600ms easing. The preceding release's predictive
tile preparation, complete RAM raster and GPU texture reuse, hidden-map stop,
retained artwork and audio/call/notification/map resource order remain intact.

Map tests cover manual changes without target tiles, AUTO-to-MANUAL interruption,
automatic readiness, scale reversal, page reentry, eviction and delayed swap
fences. The full display stress passes1,203 frames, at most4,984/8,192 display-list
bytes, with zero displayed GPU or shade overwrites. These are software workload
and correctness results, not measured LCD FPS or radio latency. Cold or absent
tiles still require preparation and transfer; continuous coverage at180/200km/h
has not been physically established.

## F4 compatibility and device binding

New packages declare the F4 family policy: HW0, bootloader0.10 through0.19 and
stock firmware5.16. Model and PCBA strings, including SAA1AA variants, are no
longer acceptance criteria. The Android check, modern capture binding, Bootstrap,
Gate, Product and host tools use the same policy. The actual bootloader version
is read from the device and carried through the journal, double capture, backup
and recovery binding; it is not replaced with the nominal package's0.15 value.

Stack/reset-vector bounds, supported F4 panel configuration, exact captured
bootloader SHA-256, device UID, CRC, durable backup and trial confirmation checks
remain. Old manifests retain their old policy. Legacy donor-schema recovery is
still limited to the verified0.14 donor image; it does not inherit the broader
family admission. This is a software admission change, not proof that every
allowed bootloader/revision has been physically qualified.

An Android target mismatch is now reported explicitly as a compatibility stop
before firmware transfer. Previously it could fall into the generic unfinished
installation message. Diagnostics record the numeric compatibility tuple to
distinguish this case from an in-progress update failure.

The latest privately uploaded diagnostic archive contains474 events, including
18 session failures and seven installer-stage observations, but no update command
results. Its stock-handshake failures are recorded only as IOException; later
Bootstrap connection attempts also fail. It does not contain enough target fields
to prove the rejected model/PCBA/version, and it does not document the separate
Code6/Phase4 vehicle report. The new typed failure and numeric tuple improve that
distinction without inventing missing diagnostic evidence.

## Update handoff

The local physical-approval path could reach reset but fail a check intended for
the phone's NDCP RESET acknowledgment. The fix separates local approval and its
1.5second display delay from remote reset's transmitted sequence/ACK requirement.
Both paths retain verified committed-image and durable Bluetooth-key checks,
including a final interrupt-protected check.

Repeated identical Bluetooth key notifications no longer dirty the key journal.
Actual key changes still advance its generation and require physical persistence.
The local key-save timer no longer restarts each tick, and a recorded persistence
failure cannot silently retry through the committed-state path.

ARM reproductions pass local approval without a RESET ACK, remote ACK ordering,
late/rekey persistence and failure cases. This fixes a definite reset-path defect
and unnecessary key writes. The reported Stage6/8 Code6/Phase4 can also represent
a real persistence/final-reset precondition failure; this run does not establish
one cause for all vehicle reports, nor bypass those checks.

During an update from0.9.58 or earlier, the old Product still performs the first
handoff. Installing the new APK/ZIP cannot retroactively replace that running
code. The new Product handoff fix applies once0.9.59 is running. Ordinary Product
updates do not require reinstalling a working compatible Gate.

## Validation and artifacts

- Android full suite:551 tests, no failures/errors/skips. Lint:0 errors/fatal,
  135 existing warnings. Current and previous real ZIP importers pass. APK
  signing certificate matches0.9.58.
- Robolectric's Windows native loader misread a space in the user-home path.
  Tests used hardlinked SDK JARs under a space-free path and an external Gradle
  init script. No product behavior was changed for this environment workaround.
- ARM target-profile and Product-reset suites:33 assertions each under O0/Os;
  Bluetooth transport/pairing, protocol services and26 storage runs pass.
  Recovery bundle host tests:6 pass.
- Debug free flash1,896B; Release33,880B. SRAM free49,304/48,336B;
  CCM16,320B. The authorized1KiB Debug and16KiB Release minima pass.
  Compiler flags, heaps, stacks, fonts, icons and GPU reservations are unchanged.
- The ZIP Product is the final Release binary padded to its0x60000 partition.
  The APK riding contract matches its SHA-256. Gate, Bootstrap, Uninstall and
  Diagnostic are rebuilt; stock.bin and resources.bin equal0.9.58 byte-for-byte.
- The replaceable Mapsforge object kit reconstructs through D8, zipalign and APK
  signature verification. It contains compiled objects, no application source
  or signing keys.

Product identity (padded partition):
`97584f5084b2f6c6fdb1a4379bada5fac8d45214bb7303cbbc865d87cb7e90e1`.
Detailed machine-readable results are retained with the matching private source
snapshot. No bench, ST-Link, vehicle, RF or physical display access was used.
