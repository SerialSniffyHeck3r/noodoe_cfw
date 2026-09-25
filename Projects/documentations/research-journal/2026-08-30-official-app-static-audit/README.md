# KYMCO Noodoe 2.1.14 official app static audit

## Scope and confidence

This audit covers the official Android package `com.noodoe.sunray` version
`2.1.14` (`versionCode 579`). It is based on the APK bytecode and packaged
resources, not on assumptions from UI behavior.

Evidence labels used here:

- **Confirmed-static**: directly encoded by app bytecode/resources.
- **Confirmed-live**: previously observed on the attached Android device and
  consistent with the static implementation.
- **Hypothesis**: plausible but requires a capture or execution test.
- **Decompiler-risk**: JADX explicitly reported an incorrectly decompiled
  method or produced internally inconsistent Java.

The APKPure XAPK added at the repository root contains a v7a/mdpi split set.
`tools/compare-apk-entries.ps1` shows that all 3,695 entries in its base APK
are byte-identical to the previously extracted official base APK. Both APKs
also carry the same Sunray signing certificate. The existing JADX tree is
therefore a valid code reference for the newly supplied XAPK.

## Executive conclusion

For the AK550-era Sunray dashboard path, the app is a Classic Bluetooth SPP
client, a protocol engine, a content compiler, and a cloud-backed product UI
combined in one package.

The local dashboard protocol is not intrinsically tied to the account server.
Login is an application routing and content-access dependency placed in front
of local services. Once initialized, the communication stack takes a paired
Bluetooth MAC address, opens secure RFCOMM SPP, performs a raw device-info
bootstrap, selects a legacy or framed protocol processor, and exchanges local
commands/files. A replacement client can implement this stack without
recreating the Noodoe social/account service.

The quickest preservation path is therefore:

1. Reproduce SPP connection and the raw five-byte bootstrap.
2. Parse device type/protocol version and choose the correct framing mode.
3. Implement framed command exchange, ACK/sequence handling, and status/time.
4. Implement the generic file-transfer state machine.
5. Reproduce gallery and creation compilers from the packaged examples.
6. Add notification, weather, and navigation producers independently of the
   retired cloud.

## APK-level architecture

### Package and manifest inventory

The decompiled `com.noodoe` tree contains 3,208 Java files. The largest
segments are `sunray` (2,568), obfuscated product/data code (308), `ccu` (105),
shared Bluetooth framework code `fwk` (94), `ccudev` (64), map (30), and
navigation (27). Their coexistence explains why a package-wide search finds
both GATT and SPP implementations.

The manifest declares 98 activities, 30 services, 24 receivers, 8 providers,
and 62 requested permissions including bundled third-party components. The
app-owned runtime components most relevant to preservation are:

| Component | Role |
|---|---|
| `SunrayForegroundService` | product foreground lifecycle |
| `AutoConnectService` | newer CCU auto-connect path |
| `TrackingService` | location, ride, weather, and vehicle status producer |
| `TransmissionService` | creation/gallery/firmware/resource installation coordinator |
| `NotifyService` | Android notification and media-session interception |
| `SunrayLongLiveService` | dashboard connection longevity helper |
| `SmsReceiver` / `PhoneStateReceiver` | phone event producers |
| `FWCheckTaskReceiver` | scheduled/boot firmware checks |
| `AclConnectedReceiver` | ACL lifecycle hinting, described below |
| `CMUTaskProvider` | shared Bluetooth task provider |

The UI surface includes pairing, main/home, creation catalog/editor, crop and
gallery, navigation/simulation, firmware/settings/maintenance, account/social,
and hidden debug/protocol-test activities. This confirms the APK is both a
cloud product and a local device engineering client, not a thin Bluetooth
remote.

### 1. Product and routing layer

`PreAuthRoutingActivity` and `PreAuthRoutingViewModel` sit in front of the main
application. Normal launch validates/refreshes user authentication, including
legacy login by master or Mystique token. If authentication is invalid, the
router returns to the splash/login path.

There is also an explicit `INTENT_EXTRA_SHOULD_LOGIN=false` branch that posts a
successful auth result without calling the login API. That is static proof that
login and Bluetooth initialization are separable in the original design. It is
not proof that every main-page feature works offline: privacy-policy checks,
creation download, weather, map/search, OTA, and user data still have their own
network dependencies.

After permission/privacy routing, the activity invokes both
`ServiceInvoker.onApplicationStart()` and
`PairingStatusController.onApplicationStart()`. This is the bridge from the UI
router to the local vehicle stack.

### 2. Pairing and connection orchestration

`PairingStatusController`:

- installs the Sunray service configuration;
- starts the Bluetooth service at application startup and adapter `STATE_ON`;
- observes bond-state changes;
- saves the selected device MAC/name;
- calls `SunrayConnectionLibrary.linkToDevice()` after bonding.

The official app does not normally remove and recreate the bond for every
feature. `unlinkToDevice()` is the explicit path that clears the target,
invokes hidden `removeBond`, updates its database, and disconnects. This differs
from the old dealer/test app behavior.

`AclConnectedReceiver` does **not** start the Bluetooth service directly on
`ACTION_ACL_CONNECTED`. It records that a target ACL connection was recently
seen; on the later non-connected action (normally disconnect), it starts the
service and clears the flag. Sunray connected/disconnected broadcasts separately
control a Huawei long-lived service.

### 3. Transport layer

`SunrayBTConnection.setupServices()` creates:

- `SunraySPPHandler` for RFCOMM I/O;
- `SunrayBTSyncProcessor` for pre-protocol bootstrap;
- task processors for command, data, file, install, and navigation work.

`SunraySPPHandler` uses secure
`BluetoothDevice.createRfcommSocketToServiceRecord()` with UUID
`00001101-0000-1000-8000-00805F9B34FB`. All Sunray command, data, and file
operations ultimately enter this SPP socket. BLE/GATT code also exists in the
package for shared frameworks, CCU/Ionex paths, and DFU, but it is not the
selected AK550 Sunray data path.

Both official and dealer apps compile `RETRY_CONNECTION_MAX_COUNT = 0`; a
failed socket `connect()` is not immediately retried by `SunraySPPHandler`.
The official app instead has a broader service/reconnect lifecycle around it.

### 4. Raw bootstrap and protocol selection

Immediately after the SPP streams open, `SunrayBTSyncProcessor` writes this raw
five-byte request, outside the normal `A5 5A`/`5A FF` framing:

```text
05 00 00 00 00
```

The response starts with reply command `85`, followed by a 32-bit little-endian
length. The parser hands the body beginning at offset 6 to the device-info
decoder. Sync uses a 5 second timeout, 1 second retry delay, and at most three
retries.

Raw device-info body layouts are length-versioned:

| Minimum body | Fields |
|---:|---|
| 12 bytes | firmware major/minor (`u16le`), packed protocol major/minor, hardware version, MAC |
| 20 bytes | plus bootloader and resource major/minor (`u16le`) |
| 36 bytes | plus sixteen language-support bitfield bytes |

JADX shows `Arrays.copyOfRange(body, 6, 11)` in the 12-byte parser although a
MAC requires six bytes. Treat that endpoint as decompiler/source-risk and
verify the raw response before implementing it literally.

Device-type selection is:

- protocol 0.0/hardware 0 is deprecated only when the firmware is older than
  4.0 -> `VERSION_1_0`;
- non-deprecated protocol 0.0, including the live AK550 firmware 5.16 response,
  -> `VERSION_1_5`;
- protocol major 2 or newer under the app's condition -> `VERSION_2_0`;
- unsupported intermediate values -> `INVALID`.

The live AK550 response reports protocol 0.0 and hardware 0, but firmware 5.16.
This therefore follows the statically recovered `VERSION_1_5` branch and its
modern sequence framing; protocol and hardware fields alone are insufficient
to select the legacy processor.

After bootstrap, input switches from the raw sync listener to either the
deprecated five-byte-header processor or `PacketSequenceProcessor` for the
modern outer sequence frame.

### 5. Framed protocol engine

The modern stack has two nested layers. See [PROTOCOL.md](PROTOCOL.md) for the
field-level reference.

- Inner command: command ID, attribute, and arbitrary command payload.
- Outer sequence: packet/ACK indexes, session, payload, and checksum.

The sequence processor supports multiple inner commands in one outer payload,
tracks phone indexes `0..127` and expected device indexes `128..255`, and uses a
3 second ACK timeout. A sent packet is retried once before the operation fails.

### 6. Command/task API layer

The app does not write feature data directly to the socket. Public-ish handler
classes create typed structures, then the task layer serializes them:

```text
UI/service adapter
  -> BTServiceApiHandler / BTCommandApiHandler / BTNavApiHandler
  -> SunrayTransferTaskManager and typed task handlers
  -> OutputCommandProcessor
  -> CommandPacketUtils + PacketSequenceProcessor
  -> SPPOutputDataProcessor
  -> BluetoothSocket OutputStream
```

Inbound data follows the reverse route through `SPPInputDataProcessor`, the
sequence processor, `InputCommandParser`, and listener callbacks.

### 7. Generic file installation layer

Dashboard themes, gallery, navigation images, firmware/resources, group/music,
and audio reuse one generic installation protocol.

The top-level negotiation command identifies a task, transfer type, location,
total size, begin/continue/done/remove/reset/cancel attribute, and a 16-byte
content ID. Location IDs are spaced by `0x0100`: dashboard `0x0000`, navigation
`0x0100`, clock `0x0200`, weather `0x0300`, speedometer `0x0400`, POI `0x0500`,
gallery `0x0600`, group `0x0700`, firmware `0x0800`, resource `0x0900`, music
`0x0A00`, and audio `0x0B00`.

Individual files then use `FILE_TRANSFER_CONTROL` and `FILE_TRANSFER`:

- control identifies task, operation, transfer/file ID, MD5 or numeric file ID,
  CRC32, total size, and optional target path;
- data chunks start with task ID, transfer ID, and a constant `u16le(1)`;
- each file-data body is capped at 11,818 bytes and rounded down to a multiple
  of four except for the final chunk;
- the implementation can manage six simultaneous file transfers, while the
  install coordinator allows ten files in flight.

### 8. Creation compiler

Cloud/downloaded creations are not transmitted as the app's high-level model.
The app unarchives them into `SunrayCreation`, renders visible widgets, and
builds a device bundle:

- widget assets are generated locally;
- flattened backgrounds are converted to `RGB_565` and JPEG quality 80;
- most generated assets are renamed to `<md5>.<ext>`;
- `creation.cfg` is compact JSON with a `files` array and ordered widget
  configuration;
- the cfg itself is renamed to `<md5>.cfg`;
- transfer uses the extensionless cfg MD5 as the creation content/config ID.

Packaged `assets/default_creations` gives six complete sample families. Of 68
packaged files, 66 filenames equal their content MD5. Two packaged exceptions
exist (`06/...c9e9.jpg` and `09/...6d88.cfg`), so content-addressing is the
compiler convention, not a safe universal validation rule for every asset in
the APK.

This is the highest-value static corpus for writing an independent creation
compiler. It provides real cfg grammar and image assets without touching the
bike or server.

### 9. Gallery compiler and transfer

Gallery supports six slots. Installing one image performs:

1. decode the selected URI;
2. convert/scale to 480 x 480 `RGB_565`;
3. encode JPEG at quality 80;
4. append the slot index as one trailing byte **after the JPEG EOI**;
5. calculate MD5 over the JPEG plus slot byte;
6. rename to `<md5>.jpg` and save its path in app settings;
7. trigger `ACTION_SYNC_GALLERY`;
8. transfer all populated gallery files under location `0x0600`.

`GalleryTransmitModel` computes the gallery content ID by updating one MD5
digest with each populated file's **filename bytes** in slot order. It does not
rehash the file contents. Because each normal filename is already
`<file-md5>.jpg`, this is effectively `MD5(name0 || name1 || ...)` over populated
slots and is distinct from every file's own content MD5.

### 10. Time and mobile status

`MOBILE_STATUS` is a 12-byte payload:

```text
year-2000, month, day, hour, minute, second, weekday(Mon=1..Sun=7),
gps_state, internet_state, gps_accuracy, battery_level, map_availability
```

Sending mobile status therefore also corrects the dashboard clock. The payload
is local and does not require a time server. Command `REQUEST_APP_UPDATE_TIME`
(`0xC5`, notify from device) exists in the enum, but JADX omitted its handler
case from an explicitly incorrectly decompiled method. Whether current
firmware actively requests a resend must be confirmed with smali or one live
capture.

### 11. Weather

The final Bluetooth weather payload is local and fully serializable:

- fixed 33-byte location field: one-byte length plus up to 32 bytes;
- `u16le` air quality (`0x00FF` when absent);
- `u16le` current temperature and condition;
- three forecast pairs of `u16le` temperature/condition.

The official producer fetches `FullWeatherData` from the Noodoe Sunray API and
maps AccuWeather condition IDs before sending. A replacement app only needs to
provide equivalent normalized values; it does not need that API.

### 12. Phone notifications and media

`NotifyService` is an Android `NotificationListenerService`. The serializers
are:

- call: `status:u8 + caller:string16le`;
- SMS: `caller:string16le + message:string16le`;
- app notification: `appId:string16le + appName:string16le + text:string16le`.

`string16le` means `u16le byte_length + encoded bytes`. Display strings use the
Sunray display charset (UTF-8 in this build); app ID uses the ID charset.
Protocol 2.0 also includes player state updates and device-initiated player
status/settings requests.

### 13. Navigation

Navigation has two independent outputs:

- transient files: generated road labels, summaries, 2D/best-fit map images;
- `UPDATE_NAVIGATION`: metadata that names the file IDs, screen positions,
  maneuver icons, distance, traffic side, night mode, speed limit/camera data,
  and post-turn icon.

The image generator targets 480 x 480 `RGB_565`, generally JPEG, and reduces
quality to keep JPEG output at or below 40,960 bytes where possible. Polnav map
rendering is native (`libjniPolnav6.so`, about 35.7 MB in v7a, plus
`libPolnavSdkSqlite.so` and JNI pointer wrappers). The Bluetooth protocol itself
does not require Polnav: an independent navigator may render equivalent image
files and populate the same metadata.

### 14. Cloud boundary

Static production endpoints include:

- `https://sunray.noodoe.com/` for legacy/GraphQL product services;
- `http://sunray-api.noodoe.com/` for Sunray REST/API services;
- GraphQL and WebSocket clients with bearer-token refresh;
- Firebase, S3-backed creation files, map/search, OTA, and telemetry clients.

Cloud-dependent in the official product: account routing, social/creation
catalog, remote creation retrieval, official weather source, map/search data,
OTA/resource catalog, user/ride sync, and some policy/config checks.

Locally reproducible: bonding and SPP, bootstrap, framing, time/mobile status,
preferences, notification serialization, gallery conversion, creation bundle
generation from local inputs, generic file transfer, navigation transfer, OQC,
and most settings commands.

## What static analysis cannot yet prove

- The exact raw bootstrap bytes returned by this AK550 firmware.
- Whether `REQUEST_APP_UPDATE_TIME` is emitted and handled on this firmware.
- Whether every protocol-2.0 command is accepted by the AK550's reported type.
- Firmware-side validation rules not mirrored by the app.
- Timing/window constraints around ignition, pairing, and dashboard UI state.
- Whether the two content-MD5 filename exceptions are intentional firmware
  semantics or packaging mistakes.

These are small, targeted capture questions. They do not justify broad manual
packet collection for every feature.

## Recommended next static work

1. Disassemble `InputCommandParser.handleReceivedCommand()` to smali and recover
   the missing `0xC5` case without running Frida.
2. Convert all packaged default cfg files into JSON fixtures and derive a formal
   schema per widget.
3. Implement the gallery converter and compare its output byte-for-byte with a
   rooted-device gallery file.
4. Implement the file-transfer state machine against synthetic streams before
   connecting to the bike.
5. Use one short official-app capture only to anchor bootstrap, negotiated
   device type, and one gallery transfer. Then replay from the new client.

## Primary code references

- `sources/com/noodoe/sunray/main/PreAuthRoutingViewModel.java`
- `sources/com/noodoe/sunray/service/PairingStatusController.java`
- `sources/com/noodoe/sunray/cmu/service/connection/SunrayBTSyncProcessor.java`
- `sources/com/noodoe/sunray/cmu/service/connection/handler/spp/SunraySPPHandler.java`
- `sources/com/noodoe/sunray/cmu/service/connection/handler/spp/command/utils/CommandPacketUtils.java`
- `sources/com/noodoe/sunray/cmu/service/task/OutputCommandProcessor.java`
- `sources/com/noodoe/sunray/cmu/service/task/InputCommandParser.java`
- `sources/com/noodoe/sunray/cmu/service/task/SunrayTransferFileTaskHandler.java`
- `sources/com/noodoe/sunray/bundle/bundler/CreationBundler.java`
- `sources/com/noodoe/sunray/model/DashboardManager.java`
- `sources/com/noodoe/sunray/communication/transmitter/model/GalleryTransmitModel.java`
- `resources/assets/default_creations/`
