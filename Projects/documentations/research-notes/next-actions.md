# Next actions

Date: 2026-08-30 KST

## Current situation

The motorcycle is not nearby. The working public Noodoe app still works, but
the Noodoe Tools/test app is blocked by server login and should not be treated
as the primary live tool.

Current best strategy:

1. Use static APK analysis at home.
2. Prepare repeatable capture tools at home.
3. Use the working public app near the bike for one feature per capture.
4. Immediately dump app-generated files after each successful transfer.

Update: the Noodoe Tools/test app login gate has now been bypassed with Frida
on the rooted Galaxy Tab Active3. The test app can be used as an additional
runtime oracle, but its manufacturing/service functions must be treated as
dangerous until individually classified.

## Do now at home

### 1. Preserve app state

Do not clear app data or uninstall the currently working public Noodoe app.
Before major experiments, save:

- APK/APKS files.
- App version.
- Android version and device model.
- App data/cache if using a rooted device.
- Bluetooth pairing state as screenshots or `dumpsys` output.

### 2. Continue static analysis

Priority source areas:

- `CreationBundler` and related bundlers.
- Gallery/photo presenter and transfer path.
- `BTInstallApiHandler`, `BTNavApiHandler`, and transfer task handlers.
- `OutputCommandProcessor` serialization for each command.
- `TaskContentUtils` task-row-to-byte conversion.

Questions to answer:

- Which files are generated for dashboard, clock, weather, speedometer, and
  gallery?
- Which file locations are used for each feature?
- Which fields form the install header before file bytes?
- Is the gallery path plain JPEG, resized JPEG, or a bundled creation?

### 3. Prepare capture labels

Use separate capture IDs. Do not combine multiple features into one session.

Recommended IDs:

- `2026-08-30-tabactive3-clock-001`
- `2026-08-30-tabactive3-notification-001`
- `2026-08-30-tabactive3-weather-001`
- `2026-08-30-tabactive3-gallery-001`
- `2026-08-30-tabactive3-dashboard-001`
- `2026-08-30-tabactive3-speedometer-001`

### 4. Improve hooks before the next bike session

Existing hook:

- `tools/frida/noodoe-btsocket-hook.js`
- `tools/frida/noodoe-tools-login-bypass.js`
- `tools/frida/noodoe-tools-offline-and-btsocket.js`

Next hook targets:

- File creation and file reads.
- `Bitmap.compress(...)`.
- `FileUtils.calculateMD5(...)`.
- `BTTaskStructure.TransferringFileInfo` construction/use.
- `sendBTFile(...)`, `addNavigationFile(...)`, `addFile(...)`.

The goal is to correlate:

```text
generated file path -> file size/hash -> file/cache ID -> SPP transfer bytes
```

## Do near the bike

### Session order

1. Connect ADB.
2. Confirm the tablet/phone remains reachable from the laptop.
3. Start logcat.
4. Start Frida before opening or touching the target Noodoe screen.
5. Perform only one user action.
6. Wait until the meter visibly finishes.
7. Stop capture.
8. Pull HCI snoop/bugreport and app cache/files immediately.
9. Fill out `capture-log-template.md`.

### First live feature

Use clock/mobile-status or a harmless app notification first. These should be
small protocol-level commands and are safer than file transfer.

### File-transfer features

For gallery, dashboard, and speedometer:

1. Start Frida first.
2. Trigger one transfer.
3. Record exactly what was selected in the app.
4. Photograph or video the meter result.
5. Pull generated files/cache before doing anything else.

## Implementation order for our own client

1. Build a read-only SPP connector.
2. Parse incoming outer frames and ACKs.
3. Send `DEVICE_INFO` read.
4. Send `MOBILE_STATUS` with current time.
5. Send one test `APP_NOTIFICATION`.
6. Send one structured `UPDATE_WEATHER`.
7. Implement file transfer for known-good captured files only.
8. Recreate gallery/dashboard generation after file formats are confirmed.

## Stop conditions

Stop the session and preserve logs if:

- The meter stops responding.
- The app reports firmware/resource update behavior unexpectedly.
- A transfer enters firmware/resource location.
- A command touches `FACTORY_RESET`, `OQC_DATA_ACCESS_WRITE`, firmware, or
  resource update paths.
