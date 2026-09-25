# OpenNoodoe 0.6.1 live validation

Date: 2026-09-03 KST

## Test identity

- Vehicle: AK550, model `SAA1AA(KR)`, PCBA `SR0701`
- Noodoe firmware/resource: `5.16` / `5.14`
- Phone: Samsung SM-S928N
- App: OpenNoodoe `0.6.1` (`versionCode 17`)
- APK SHA-256: `884bc32aeee433d1df5ef9165aea34f0374b5c48a0f7015bad540f16cdd53f48`
- Preserved log SHA-256: `fdc3f87cc4d58f5ab313ccefb01144f7ed88f0fc50e3a68eaebefb21b5999fe2`

The raw device log and capture directories are preserved under `device/`.

## Result summary

| Function | Transport result | Dashboard result | Verdict |
| --- | --- | --- | --- |
| Factory reset `0x0F` | sequence ACK and command reply status 0 | SPP disconnected immediately; rider reports no major visible loss/change | Command verified; exact reset scope still unknown |
| Persistent Navigation DATA task | BEGIN reply 0, two image transfers reply 0, repeated `0x08` ACK | Navigation remained usable instead of returning immediately after each image | Session-lifetime fix verified |
| V5.16 POI Creation | BEGIN/files/DONE all reply 0 | `C1 foreground=2`; rider saw radar content | Renderer classification verified; visual semantics partial |
| POI runtime `0x06` | all tested frames received sequence ACK | Some radar output appeared, but marker identity/coordinates were not fully distinguished | Partially verified |

## Factory reset

At `03:03:50`, the application first performed the mandatory stationary read:

```text
RIDING_STATUS status=0 keyOn=true odometer=25392 stopDuration=115
maxSpeed=0 currentSpeed=0
```

It then sent command `0x0F`, attribute WRITE. The dashboard returned both the
sequence ACK and a command reply whose two-byte status was zero. The SPP socket
closed about 65 ms later. This establishes that the dashboard accepted and
executed the official factory-reset command; it was not merely delivered over
Bluetooth.

The rider observed no major visible change. That observation does not yet prove
which persistent domains are retained. Firmware, resource pack, OQC production
data, and later normal operation remained intact. A future controlled reset
matrix should compare user preferences, paired-phone records, gallery slots,
installed Creations, and clock settings before and after one reset.

## Navigation

The new implementation opened task 1 once at `03:02:31`. The dashboard replied
with status 0 and reported `C1 foreground=4`, the Navigation renderer.

The same task then accepted:

- many metadata updates with different maneuver icons;
- image file ID 1 / transfer ID 1, 9,103 bytes, all replies status 0;
- image file ID 2 / transfer ID 2, 9,103 bytes, all replies status 0;
- a separate `0x08` update referencing each file ID.

There was no automatic `NAV DONE` after either image. This is the material
difference from 0.6.0, where DONE caused `foreground=4 -> 0` after about 1.5
seconds. The rider confirmed that the Navigation screen now behaves normally.

The task later ended through the factory-reset disconnect, so explicit user
STOP/DONE and RESET/DONE still need one clean isolated capture. That is cleanup
validation, not a blocker for the session-lifetime conclusion.

## V5.16 radar and POI

The first POI install request at `03:03:08` was correctly blocked because the
Navigation task was still active. After the reconnect, the V5.16-specific bundle
was installed at `03:05:02`:

```text
location=0x500
contentId=de ec c1 9e 9e d1 e6 74 4d 80 00 a0 e5 03 0c 24
files=2
JPEG=11646 bytes
CFG=633 bytes
widgets=BackgroundWidget, RadarWidget, LocationsWidget
```

BEGIN, both file START/DATA/FINISH phases, and DONE returned status 0. More
importantly, the dashboard then repeatedly reported `C1 foreground=2`. This is
direct evidence that V5.16 classified and entered the Around Me renderer. The
old directory-03 candidate never produced this result and remains incompatible.

The raw `C1` payload contains six `0x7FC00000` float NaNs, matching the three
unset custom-location latitude/longitude pairs intentionally put in the cfg.
That correlation further supports that the installed `LocationsWidget` is the
source of the active renderer state.

Runtime command `0x06` was sent for types 1 through 9, including individual,
circle-pattern, and hide coordinates. Every frame received a sequence ACK. The
rider saw radar content, but this run did not uniquely identify each POI type,
axis orientation, scale, clipping boundary, or `placeId` behavior. Those remain
the next controlled visual experiment.

## Current state

The phone and app remain reachable over wireless ADB. The bike is now elsewhere,
and the application currently shows `Disconnected`; no further SPP connection
was attempted from home.

## Next field test

1. Start a new capture and connect once.
2. Open Navigation, send two distinct images, explicitly press session STOP,
   and confirm status-0 DONE plus `foreground=4 -> 0` without losing SPP.
3. Repeat with session RESET and confirm RESET/DONE replies while SPP stays up.
4. Enter Around Me and send one POI at a time, types 1 through 9, using large
   asymmetric coordinates such as `(700, 100)` and `(-100, 700)`.
5. Photograph each frame and record whether position, icon/color, label, and
   `placeId` changed. Do not run the rotating pattern until the static matrix is
   complete.

