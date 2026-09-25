# Strategy reassessment

Date: 2026-08-30 KST

## Decision

The project should not depend on long manual packet-capture sessions on the
motorcycle. The better approach is:

1. Extract protocol and content-generation behavior from the APKs.
2. Use Frida on Android to bypass UI/server gates and observe high-level method
   arguments.
3. Use short bike sessions only to validate commands against the real AK550
   meter.
4. Build an independent client after the command and content pipeline are
   understood enough.

## Why

Raw Bluetooth capture is useful but low-level. It tells us what bytes moved,
but not why they were generated. Static analysis and Frida can expose names,
objects, file paths, MD5 values, CRC values, location IDs, command IDs, and
feature states before they are serialized into bytes.

The motorcycle should be used as a verifier, not as the main microscope.

## Test app login gate

The Noodoe Tools/test app login gate appears weakly coupled to local app state.
`NoodoeToolsPresenter.isLoggedIn()` returns true when
`AppSettings.getUserName()` is non-null.

The `user_name` value is stored in the `USER` SharedPreferences namespace,
which resolves to an app-private SharedPreferences file named `user`.

Best first bypass attempts:

1. Inject SharedPreferences values on the rooted tablet.
2. If that fails, use Frida to force `isLoggedIn()` or `getUserName()`.
3. If runtime bypass works and repeated use is needed, make a patched APK.

## Why runtime bypass before patched APK

A patched APK is possible, but it adds extra failure modes:

- Re-signing changes the app signature.
- Package install may wipe or isolate existing app data.
- Some APIs or providers may assume the original package/signature.
- Split APK handling and old support libraries can make rebuilds noisy.
- We may patch the wrong branch before learning what the runtime gate actually
  checks.

Frida or SharedPreferences injection gives a faster answer: does the tool UI
actually work if the login screen is skipped?

## Runtime bypass validation

Frida validation succeeded on a rooted Samsung Galaxy Tab Active3
(`SM-T575N`). `tools/frida/noodoe-tools-login-bypass.js` forced the local login
decision and the app entered the Noodoe Tools main menu. The combined script
`tools/frida/noodoe-tools-offline-and-btsocket.js` also loaded Android
Bluetooth socket read/write hooks.

See `docs/frida-runtime-instrumentation.md` for commands and evidence.

## Custom APK plan

If runtime bypass proves useful, build a personal research APK that:

- Starts directly at the display/main tool screen.
- Seeds non-null user/org fields locally.
- Removes or no-ops cloud login calls.
- Keeps Bluetooth protocol code unchanged.
- Makes dangerous functions visually obvious before use.

Do not distribute this APK. Treat it as a lab instrument for owned hardware.

## Main app role

The public Noodoe app should still be preserved and instrumented. It is the best
source for:

- Gallery file generation.
- Creation bundle generation.
- Real dashboard/clock/weather/speedometer install paths.
- Navigation image generation.
- Actual user-facing behavior.

The public app should not be heavily modified yet because it currently works.
Use it as the reference, and use the rooted tablet/test app for invasive
experiments.

## Revised work order

1. Make the test app enter the main tool UI without server login.
2. Use the test app to exercise read-only hardware info and low-risk settings.
3. Build Frida hooks for high-level command objects, not only Bluetooth bytes.
4. Pull public-app generated gallery and bundle files from the rooted tablet.
5. Reproduce gallery file generation offline and compare hashes.
6. Reproduce one creation bundle offline and compare file inventory.
7. Use brief bike sessions only for validation.
