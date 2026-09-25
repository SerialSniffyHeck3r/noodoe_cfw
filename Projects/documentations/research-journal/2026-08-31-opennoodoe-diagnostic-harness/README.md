# OpenNoodoe 0.2.0 diagnostic harness

Date: 2026-08-31 KST

## Purpose

The preserved Noodoe Tools app failed to progress against the current AK 550.
Its commands are therefore treated as protocol candidates, not live compatibility
evidence. OpenNoodoe 0.2.0 reuses only the modern framed SPP transport already
observed on the vehicle and exposes a small set of individually triggered tests.

## Static basis for the notification test

The current official APK's Sunray 1.5 command table assigns `APP_NOTIFICATION`
command ID `0x15`, with WRITE from phone and REPLY from meter. Its output processor
serializes three strings in this order:

1. application ID, US-ASCII
2. application name, UTF-8
3. notification body, UTF-8

Each string is prefixed by its encoded byte count as a little-endian signed-short
compatible two-byte value. It is not a one-byte length and it is not NUL terminated.

Primary static references:

- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/utils/Sunray_1_5_Commands.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/OutputCommandProcessor.java`, notification serializer around lines 714-723
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/invisibi/iv01/cmu/utils/Helpers.java`, `createStringBytes()` and little-endian `GetShortBytes()`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/utils/SunrayCMUConstants.java`, display and ID charsets

OpenNoodoe's fixed first probe is:

```text
appId:       io.opennoodoe.app
appName:     OpenNoodoe
notification: OpenNoodoe notification test
```

The exact payload is locked by `ProtocolCodecTest.appNotificationMatchesOfficialStringEncoding`.

## Exposed candidate commands

| Button | Command | Attribute | Safety state |
| --- | ---: | ---: | --- |
| Device info | `0x05` | READ | Transport/live confirmed |
| Riding status | `0x0C` | READ | Unverified candidate |
| OQC data | `0x11` | READ | Unverified candidate |
| Notification test | `0x15` | WRITE | Unverified candidate |
| Clock/mobile status | `0x02` | WRITE | Manual candidate |
| Breathing light | `0x0E` | WRITE | Manual candidate |

OQC test start/write, OQC production-data write, factory reset, firmware upgrade,
resource install, and file transfer are not callable from this build. Incoming
`0xC4` OQC-result notifications are decoded passively. Incoming `0xC5` time
requests are logged, but automatic clock writes are suppressed during protocol
validation.

## Evidence model

Every command records an `ACTION` line before connection checks. When connected,
the log then records the complete outer SPP frame, retry state, sequence ACK and
latency, decoded command reply, and any spontaneous notification. Buttons add
timestamped `VISIBLE RESULT`, `NO VISIBLE RESULT`, or stop markers.

This distinguishes four outcomes:

- no application send
- sent but no sequence ACK
- sequence ACK but no successful command reply
- successful command reply with or without visible meter behavior

One command type must be tested per capture session. The complete procedure and
stop conditions are in `android/OpenNoodoe/docs/AK550_FIELD_TEST_RUNBOOK.md`.

## Build and deployment evidence

- Version: `0.2.0` (`versionCode=3`)
- Unit tests: passed
- Android lint: passed
- Debug APK assembly: passed
- APK size: 43,148 bytes
- APK SHA-256: `FA41EF66AD051B104E168CCDC45444DCB68730F97CDE403C80A4C892A6E8D85C`
- Installed over wireless ADB on Samsung `SM-T575N`: success
- Device package query returned version `0.2.0`, versionCode `3`
- Installed `base.apk` SHA-256 matched the host APK exactly
- Device capture creation verified at
  `/sdcard/Android/data/io.opennoodoe.app/files/captures/20260831-011353-431/`

No vehicle command was transmitted during this build/deployment verification.
The tablet was not connected to framed Noodoe SPP, so candidate feature behavior
remains unverified until the next bike session.

## Live validation update

The subsequent AK 550 session confirmed notification display, key-off breathing
light control, riding-status read, and OQC-data read. See
`analysis/2026-08-31-opennoodoe-live-validation/README.md` for raw-log hashes,
command counts, state-dependent results, and the official-app SPP ownership race.
