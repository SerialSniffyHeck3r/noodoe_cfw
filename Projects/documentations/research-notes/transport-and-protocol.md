# KYMCO Noodoe transport and protocol notes

Date: 2026-08-30 KST

## Current conclusion

The confirmed app-layer transport for the Noodoe meter is Classic Bluetooth SPP
over RFCOMM.

The Bluetooth service UUID is:

```text
00001101-0000-1000-8000-00805F9B34FB
```

Evidence:

- `Noodoe Tools` and the public app both contain SPP/RFCOMM code.
- The decompiled test APK defines the SPP UUID above.
- The app creates an RFCOMM `BluetoothSocket` with that UUID.
- Tablet live logs showed `com.noodoe.sunray` connecting to `KYMCO Noodoe
  [device suffix]` on the same UUID.
- Frida stream hooks observed Noodoe payload bytes on Java Bluetooth streams.

## Related BLE/GATT UUIDs

The code also defines BLE services and characteristics. Treat these as
documented but not yet proven active on the AK550 meter:

Services:

| UUID | Label |
| --- | --- |
| `00002B00-0000-1000-8000-00805f9b34fb` | Control Service |
| `00003B00-0000-1000-8000-00805f9b34fb` | Voice Service |
| `00004B00-0000-1000-8000-00805f9b34fb` | Notification Service |
| `00005B00-0000-1000-8000-00805f9b34fb` | Command Service |
| `14839ac4-7d7e-415c-9a42-167340cf2339` | SPP LE / Sunray Application Service |

SPP LE characteristics:

| UUID | Label |
| --- | --- |
| `8B00ACE7-EB0B-49B0-BBE9-9AEE0A26E1A3` | SPP LE Output |
| `0734594A-A8E7-4B1A-A6B1-CD5243059A57` | SPP LE Input |
| `BA04C4B2-892B-43BE-B69C-5D13F2195392` | SPP LE Input Credit |
| `E06D5EFB-4F4A-45C0-9EB1-371AE5A14AD4` | SPP LE Output Credit |

Other BLE characteristics include control, command, voice, notification,
incoming call, missed call, SMS, time sync, stock, and ACK characteristics.

## Packet layers

### Outer sequence frame

```text
5a ff [len u16 le] [control] [packet_idx] [ack_idx] [session_type] [reserved] [payload...] [checksum]
```

Important fields:

- Magic: `5a ff`
- Header length: 9 bytes
- Length: little-endian 16-bit
- Control byte: usually `0x00`, or `0x40` when ACK is present
- Packet index: sender sequence number
- ACK index: last received packet index being acknowledged
- Session type: `0 = control`, `1 = file/data`
- Checksum: `256 - sum(payload bytes)`, one byte

Observed pure ACK style:

```text
5a ff 09 00 40 xx yy 00 00
```

### Inner command frame

```text
a5 5a [command_id] [attribute] 00 00 [payload_len u32 le] [payload...]
```

The app parser strips `a5 5a` before command dispatch. After stripping:

- Offset `0`: command ID
- Offset `1`: attribute
- Offset `8`: reply status for replies
- Offset `10`: payload start for most replies

## Command IDs

| ID | Command |
| --- | --- |
| 2 | `MOBILE_STATUS` |
| 3 | `FILE_QUERY` |
| 4 | `PREFERENCE_SETTING` |
| 5 | `DEVICE_INFO` |
| 6 | `UPDATE_POI` |
| 7 | `UPDATE_GROUP_MEMBER` |
| 8 | `UPDATE_NAVIGATION` |
| 9 | `UPDATE_WEATHER` |
| 10 | `FILE_TRANSFER_NEGOTIATE` |
| 11 | `FILE_TRANSFER_CONTROL` |
| 12 | `RIDING_STATUS` |
| 13 | `FILE_TRANSFER` |
| 14 | `BREATHING_LIGHT` |
| 15 | `FACTORY_RESET` |
| 16 | `BASIC_FIRMWARE_UPGRADE` |
| 17 | `OQC_DATA_ACCESS_READ` / `OQC_DATA_ACCESS_WRITE` |
| 18 | `OQC_TEST` |
| 19 | `CALL_STATUS` |
| 20 | `SMS` |
| 21 | `APP_NOTIFICATION` |
| 22 | `GET_METER_PROFILE` |
| 193 | `NOTIFY_RUNNING_CREATION` |
| 194 | `NOTIFY_RIDING` |
| 195 | `NOTIFY_KEY_ON` |
| 196 | `NOTIFY_OQC_TEST_RESULT` |
| 197 | `REQUEST_APP_UPDATE_TIME` |
| 198 | `UPDATE_BATTERY_DATA` |

Attributes:

| Value | Attribute |
| --- | --- |
| 1 | `READ` |
| 2 | `WRITE` |
| 6 | `WRITE_MULTIPLE` |
| 8 | `REPLY` |
| 16 | `NOTIFY` |

## File/content locations

| Value | Location |
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

Transfer type:

- `1 = DATA`
- `2 = FILE`

Transfer attribute:

- `1 = UPDATE_BEGIN`
- `2 = UPDATE_CONTINUE`
- `3 = UPDATE_DONE`
- `4 = REMOVE`
- `5 = RESET`
- `6 = CANCEL`

The sender reads files in 1024-byte chunks, sends up to 16384 bytes per loop,
then waits 2500 ms before continuing.

## Feature payloads

### Clock and phone status

`MOBILE_STATUS` includes the 7-byte time-sync value:

```text
[year_minus_2000] [month] [day] [hour] [minute] [second] [day_of_week]
```

The day-of-week mapping is:

- Android Sunday `1` becomes `7`.
- Other Android values become `value - 1`, so Monday becomes `1`.

`MOBILE_STATUS` then adds:

- GPS state
- Internet state
- GPS accuracy
- Phone battery level
- Map availability

### Weather

`UPDATE_WEATHER` contains:

- Location string, one-byte length, max 32 bytes
- Air quality, signed/short-like value, or `255` no-data
- Current temperature
- Current condition type, valid 0..8
- Three forecast temperature/condition pairs

### Navigation

`UPDATE_NAVIGATION` contains:

- Next-turn distance
- Block count
- Navigation icon
- File IDs for rendered text/assets
- Coordinates for best-fit/map/turn items
- Left-driving and night-mode flags
- Speed-limit and speed-camera fields

This suggests navigation may use a mix of control fields and pre-rendered
text/image assets referenced by file ID.

### Notifications

`APP_NOTIFICATION` contains:

- App ID string
- App display name
- Notification text

`CALL_STATUS` and `SMS` have separate command IDs.

### OQC/test mode

`OQC_TEST` starts or stops OQC mode. `NOTIFY_OQC_TEST_RESULT` carries a 5-byte
status block:

- byte 0 bit 0: power on
- byte 0 bit 1: power off
- byte 0 bit 2: button up
- byte 0 bit 3: button enter
- byte 0 bit 4: button down
- byte 0 bit 5: MFI enabled
- bytes 1..4: light sensor value, little-endian

## Safe implementation order

1. Connect SPP only.
2. Receive and parse any initial meter frame.
3. Send `DEVICE_INFO` read.
4. Implement sequence ACKs.
5. Parse `DEVICE_INFO`.
6. Send `MOBILE_STATUS` with current time.
7. Send benign notification/weather test only after read/ACK is stable.
8. Defer file transfer until content format is understood.

## Dangerous or deferred commands

Do not send these during early experiments:

- `FACTORY_RESET`
- `BASIC_FIRMWARE_UPGRADE`
- `OQC_DATA_ACCESS_WRITE`
- `FIRMWARE` transfer
- `RESOURCE` transfer
- `TRANSFER_RESET`
- `TRANSFER_REMOVE`

Dashboard, speedometer, clock, weather, and gallery file transfers are not
necessarily destructive like firmware updates, but they still mutate meter
storage and should wait until the read-only client is reliable.
