# Bootstrap admission fix — release 0.9.30

The reported screen contained error `00050002` and progress `262144 / 262144`.
The storage error means `BS_DENIED`, not a content hash failure. The count is
256 KiB, also the size of the last startup file audit (device log). It does not
identify the failed command or prove that installation completed.

Two defects were reproduced in the production ARM policy/UI/input code:

1. Android deliberately probes readiness with a read-only `0x80` request and
   waits after DENIED/BUSY. Bootstrap instead converted a queued rejection into
   a latched SYSTEM ERROR. The last audit byte count remained on that screen.
   The regression test fails at line 33 before the correction.
2. Connection recovery watched UP throughout the installation session. On the
   verified Back/Install dialog it could claim the UP release before the menu
   selected Install. It also revoked storage authorization. The second test
   fails at line 43 with only the first defect fixed.

The first fix leaves DENIED/BUSY on the wire, but does not latch those two
read-only-probe responses as permanent storage failures. It does not grant
access, skip authentication, bypass physical approval or repeat a write.
`BS_FAILED`, IO/hash failures and mutation refusals keep their error handling.

The second fix admits UP pairing recovery only on connection-wait pages and
lets an already started recovery finish. UP remains ordinary selection on
Back/Install; emergency DOWN-three-second stock recovery remains independent.

The field report did not include command/file/phase or the preceding button
sequence. These are demonstrated matching failure mechanisms, not proof that
every possible `00050002` has this cause. No physical unit was accessed.

## Validation

- Existing pairing recovery plus new admission/input scenarios: **84 ARM
  assertions**, across `-O0` and `-Os`, all passing after both corrections.
- Bootstrap UI, cancellation, image proof and ROM-command tests: **52 cases**.
- Android installer/Bootstrap/scoped-backup regression tests: **37 passed**.
- Actual new ZIP import, identity, malformed export and old Gate tests:
  **322 checks passed**.
- APK assembly and lint passed: **0 errors, 120 existing warnings**. APK
  versionCode 70, versionName 0.9.30; previous signing certificate preserved.
- Bootstrap: **458,724 / 458,752 bytes**, 28 bytes remaining. Same partition
  and compiler policy; static RAM 125,976 bytes and CCM allocation 32 KiB.
- Product Release and Debug are byte-identical to 0.9.29. Prior validated free
  flash remains **35,688 / 2,052 bytes**, above the 4 KiB / 2 KiB minima. No
  Product, Gate, recovery, resource, diagnostic or uninstall payload changed.
- No new visual layout or glyph asset was introduced. ROM command tests ran;
  no physical LCD capture, Bluetooth or vehicle-install success is claimed.

## Applying this fix

This fixes **Bootstrap**, not the Product already running after installation.
Update the APK and import the new matching installation ZIP. An old Bootstrap
already running in RAM/flash cannot acquire this fix from an APK update or by
pressing Continue alone. Return to stock using the on-device keep-data recovery
and let the installer send the new Bootstrap, then continue with retained data.
Preserve logs and the existing installation journal; do not clear app data.

The APK/release is 0.9.30; the unchanged Product reports **0.9.29**. This is
intentional. Its exact-image riding contract is preserved, so an already
working Product does not need to be reflashed for this installer correction.
