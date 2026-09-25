# Noodoe Tools APK deep dive

Date: 2026-08-30 KST

Subject APK: `evidence/apk/noodoe.com.navigations_1.0/base.apk`

Package: `noodoe.com.navigations`

Version: `1.0` / versionCode `1811131504`

SHA-256: `F08C30B0CFAB34AEFAB26427D4A08834246E922D997FDD044BF9BC0BEEA75AF2`

Decompiled source: `analysis/jadx/noodoe.com.navigations_1.0`

## Executive result

The APK is not just a small companion tool. It contains a production/OQC/repair
tool stack and the same local Bluetooth protocol family used by the public
Noodoe app.

The important conclusion is that the local Noodoe meter link is Classic
Bluetooth SPP/RFCOMM, not BLE GATT for the main data path. The SPP UUID is the
standard serial-port UUID:

`00001101-0000-1000-8000-00805F9B34FB`

This matches the live tablet evidence: the public app opened an RFCOMM
BluetoothSocket to `KYMCO Noodoe [device suffix]` using that UUID.

## What this app can teach us

The app contains enough information to begin an independent Noodoe client:

- Bluetooth connection path and SPP socket UUID.
- Outer packet framing, ACK sequence logic, session type byte, and checksum.
- Inner command framing.
- Command IDs for clock/status, preference settings, weather, navigation,
  notifications, file transfer, OQC mode, OQC data read/write, factory reset,
  firmware/resource update, and meter profile.
- File location IDs for dashboard, clock, weather, speedometer, gallery,
  navigation, firmware, and resources.
- OQC/production data layout, including serial, PCBA, MAC, model, PIN,
  backlight thresholds, max speed, language, firmware version, assembly number,
  resource ID, dashboard ID, and related fields.

This means the preservation project should not begin by trying to decode raw RF
or BLE advertisements. The most productive first target is a desktop or Android
SPP client that sends one safe read-only command and parses the reply.

## Login and cloud boundary

The first login screen is a cloud gate for the tool UI, not proof that local
Noodoe communication requires the cloud.

Observed login/cloud endpoints:

- Base URL: `http://iotc.kymco.com/`
- `/api/login`
- `/logout`
- `/eApi/userOrgInfo`
- `/eApi/scooterInfo`
- `/eApi/rdModelMappingIotDeviceParam`
- `/eApi/countryParams`
- `/noodoeRepair/IotDeviceAssemblyInfo`
- `/noodoeRepair/IotDevicePairInfo`

The app also writes OQC results to Firebase:

- Base URL: `https://sunrayoqctool.firebaseio.com/`
- `/{tableName}.json?auth=...`

Important local observation: `NoodoeToolsPresenter.isLoggedIn()` only checks
whether `AppSettings.getUserName()` is non-null. `AppSettings` stores this under
the `USER` SharedPreferences namespace as key `user_name`.

So the app's first screen is weakly coupled to local storage. Some later
repair/replacement functions still call KYMCO cloud APIs, but the Bluetooth
library and command encoder are local.

## Bluetooth transport

The connection implementation creates an RFCOMM socket:

- `SunraySPPHandler` calls `BluetoothDevice.createRfcommSocketToServiceRecord`.
- Service UUID: `00001101-0000-1000-8000-00805F9B34FB`.

This agrees with live capture evidence:

- Tablet paired to `KYMCO Noodoe [device suffix]`.
- Android Bluetooth dump showed the device as dual-mode with SPP UUID.
- Runtime logs showed `com.noodoe.sunray` connecting to SPP.
- Frida hooks saw actual stream bytes with prefixes `5a ff` and `a5 5a`.

BLE still exists in the vendor stack, and the FCC documents indicate BLE plus
BR/EDR capability for the module family. For the meter payloads we care about,
however, the strong evidence points to SPP.

## Packet layers

There are two visible layers.

Outer sequence packet:

```text
5a ff [len u16 le] [control] [packet_idx] [ack_idx] [session_type] [reserved] [payload...] [checksum]
```

Observed constants:

- Leading bytes: `5a ff`.
- Header length: 9 bytes.
- Payload checksum: `256 - sum(payload bytes)`, truncated to one byte.
- Control byte: `0x40` when carrying an ACK index, otherwise `0x00` in outgoing
  packets.
- Packet index: outgoing starts around `0x00`; incoming expected index starts at
  `0x80`.
- Session type: `0` for control, `1` for file/data.

Inner command packet:

```text
a5 5a [command_id] [attribute] 00 00 [payload_len u32 le] [payload...]
```

The input parser strips `a5 5a` before dispatching command replies, so parsed
reply offsets are usually command ID at `0`, attribute at `1`, status at `8`,
payload at `10`.

## Command map

Known command IDs from `Sunray_1_5_Commands`:

| ID | Name | Direction / role |
| --- | --- | --- |
| 2 | `MOBILE_STATUS` | Phone status and time sync |
| 3 | `FILE_QUERY` | Data/file query |
| 4 | `PREFERENCE_SETTING` | User/unit/display preferences |
| 5 | `DEVICE_INFO` | Read device information |
| 6 | `UPDATE_POI` | Navigation POI update |
| 7 | `UPDATE_GROUP_MEMBER` | Group member location update |
| 8 | `UPDATE_NAVIGATION` | Navigation state update |
| 9 | `UPDATE_WEATHER` | Weather widget update |
| 10 | `FILE_TRANSFER_NEGOTIATE` | Begin/control asset transfer |
| 11 | `FILE_TRANSFER_CONTROL` | Data-channel transfer control |
| 12 | `RIDING_STATUS` | Read riding status |
| 13 | `FILE_TRANSFER` | Data-channel file transfer |
| 14 | `BREATHING_LIGHT` | Breathing light setting |
| 15 | `FACTORY_RESET` | Factory reset, dangerous |
| 16 | `BASIC_FIRMWARE_UPGRADE` | Firmware update |
| 17 | `OQC_DATA_ACCESS_READ/WRITE` | Production/OQC data |
| 18 | `OQC_TEST` | Start/stop OQC mode |
| 19 | `CALL_STATUS` | Call notification |
| 20 | `SMS` | SMS notification |
| 21 | `APP_NOTIFICATION` | App notification |
| 22 | `GET_METER_PROFILE` | Ionex-style meter profile |
| 193 | `NOTIFY_RUNNING_CREATION` | Device notification |
| 194 | `NOTIFY_RIDING` | Device notification |
| 195 | `NOTIFY_KEY_ON` | Device notification |
| 196 | `NOTIFY_OQC_TEST_RESULT` | OQC test result notify |
| 197 | `REQUEST_APP_UPDATE_TIME` | Device asks app to update time |
| 198 | `UPDATE_BATTERY_DATA` | Ionex-style battery status |

Attributes:

- `READ = 1`
- `WRITE = 2`
- `WRITE_MULTIPLE = 6`
- `REPLY = 8`
- `NOTIFY = 16`

## Safe first commands

Good read-only candidates for a first independent client:

1. `DEVICE_INFO` (`id=5`, `READ`)
2. `PREFERENCE_SETTING` read path, if the read command is exposed in the public
   library variant
3. `OQC_DATA_ACCESS_READ` (`id=17`, `READ`) only after confirming that OQC read
   mode is safe on AK550

Avoid these early:

- `FACTORY_RESET`
- `BASIC_FIRMWARE_UPGRADE`
- `OQC_DATA_ACCESS_WRITE`
- firmware/resource file transfer
- any command that changes dashboard/gallery until the read/ACK loop is stable

## File transfer map

Known file/content location IDs:

| Value | Name |
| --- | --- |
| 0 | `DEFAULT_DASHBOARD` |
| 256 | `NAVIGATION` |
| 512 | `CLOCK` |
| 768 | `WEATHER` |
| 1024 | `SPEEDOMETER` |
| 1280 | `POI` |
| 1536 | `GALLERY` |
| 1792 | `GROUP` |
| 2048 | `FIRMWARE` |
| 2304 | `RESOURCE` |

Transfer types:

- `DATA = 1`
- `FILE = 2`

Transfer attributes:

- `UPDATE_BEGIN = 1`
- `UPDATE_CONTINUE = 2`
- `UPDATE_DONE = 3`
- `REMOVE = 4`
- `RESET = 5`
- `CANCEL = 6`

Raw file payloads are sent after negotiation. The sender reads files in
1024-byte chunks, sends up to 16384 bytes in a loop, then waits 2500 ms before
continuing.

## Feature payload notes

`MOBILE_STATUS` includes:

- Time sync data.
- GPS permission/state.
- Internet permission/state.
- GPS accuracy.
- Phone battery level.
- Map availability.

`PREFERENCE_SETTING` includes:

- Distance unit.
- Temperature unit.
- 12/24h time format.
- Breathing light experience flag.
- Display brightness.
- Default dashboard enable flag.
- Language.
- Shutdown time.
- User name string.
- Notify-on-navigation flag.

`UPDATE_WEATHER` includes:

- Location name, one-byte length, max 32.
- Air quality or no-data value.
- Current temperature and condition type.
- Three forecast temperature/condition pairs.

`UPDATE_NAVIGATION` includes:

- Next turn distance.
- Block count.
- Navigation icon.
- File IDs for turn/current road/next road/overall status/best-fit view.
- Screen coordinates for best-fit, map-to-display, and next-turn items.
- Left-driving flag.
- Night mode flag.
- Speed-limit and speed-camera fields when enabled.

`APP_NOTIFICATION` includes:

- App ID string.
- App name display string.
- Notification display string.

## OQC and production data

The OQC data block is especially valuable because it documents meter identity
fields. The parser expects a reply length of at least 147 bytes and extracts:

- Part number: bytes 10..25, ASCII
- Serial number: bytes 26..43, ASCII
- PCBA hardware version: bytes 44..49, ASCII
- MAC string: bytes 50..61, ASCII
- Model: bytes 62..71, ASCII
- PIN: unsigned 16-bit at offset 72
- Ten backlight thresholds: unsigned 32-bit values from offset 74
- Max speed: unsigned 16-bit at offset 114
- Language: unsigned 16-bit at offset 116
- Firmware major/minor: offsets 118 and 120
- Assembly number: bytes 122..137, ASCII
- Panel version: unsigned 16-bit at offset 138
- Unit/type/language pack: offsets 140..142
- Motor series: unsigned 16-bit at offset 143
- Resource ID: byte 145
- Dashboard ID: byte 146

OQC test mode (`OQC_TEST`) has start/stop states. Device notifications report a
5-byte `OQCTestData` structure:

- byte 0 bit 0: power on
- byte 0 bit 1: power off
- byte 0 bit 2: button up
- byte 0 bit 3: button enter
- byte 0 bit 4: button down
- byte 0 bit 5: MFI chip enabled
- bytes 1..4: light sensor value, little-endian

## What remains to prove

- Whether AK550 accepts the exact same command set without model gating.
- Whether OQC read is always read-only/safe on this meter firmware.
- Exact timestamp byte layout in `SunrayDateTimeUtils.getTimeSyncData()`.
- Asset file format for dashboard, speedometer, clock, weather, and gallery
  objects.
- Whether initial pairing PIN/challenge has any app-layer pairing handshake
  beyond Android Classic Bluetooth bonding.
- Why Android btsnooz did not include ACL payloads in current captures; Frida
  remains the better source for app-layer bytes.

## Recommended next milestone

Build a small local protocol harness that does only this:

1. Connect to `KYMCO Noodoe [device suffix]` over Classic SPP UUID
   `00001101-0000-1000-8000-00805F9B34FB`.
2. Send a correctly framed `DEVICE_INFO` read command.
3. Receive `5a ff` sequence packets.
4. Verify the payload checksum.
5. ACK incoming packet indexes.
6. Parse and print device info fields.

Once this works, the rest of the project becomes an implementation problem
rather than a blind reverse-engineering problem.
