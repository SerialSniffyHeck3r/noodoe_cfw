# OpenNoodoe 0.6.0 current field symptom analysis

Date: 2026-09-03 KST

## Scope

- Vehicle profile: AK 550, `SAA1AA(KR)`, PCBA `SR0701`, SR1.5 firmware V5.16
- Phone: Samsung SM-S928N
- App: OpenNoodoe 0.6.0 (`versionCode 16`)
- Main log: `opennoodoe.log`
- Firmware: `1657088080998-s1-SR1.5_ota_V516.bin`
- Firmware SHA-256: `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca`

The rider reported that an official/reference Creation works after deleting the
old content, Navigation appears briefly and returns to the speedometer, and
generated or POI/radar-like Creations commit successfully but show the
dashboard's generic error dialog.

## Conclusions

1. The Noodoe storage and Creation loader are not globally broken. A location
   `0x0200` remove followed by the preserved official clock bundle completed and
   produced `RUNNING_CREATION` values 16 and 32 when the rider changed pages.
2. The short Navigation display is caused by OpenNoodoe closing the Navigation
   DATA task immediately after its first image and metadata update. The official
   app keeps that task open for the route lifetime and sends DONE only when
   Navigation stops.
3. The generated two-file Creation is not a valid universal theme. It contains
   only `BackgroundWidget`, while the V5.16 parser classifies each functional
   page by type-specific widgets.
4. The packaged APK directory-03 POI candidate is incompatible with this V5.16
   parser. Its cfg contains `DistanceWidget` and `EtaTextWidget`; neither name is
   accepted by the firmware's widget-name parser. Transport status 0 therefore
   cannot make this cfg renderable.
5. Failed content does persist and causes later `ALREADY_EXISTS` responses. A
   location REMOVE is required for a controlled retry, but stale content alone
   does not explain the directory-03 failure: its schema is independently
   incompatible.

## Navigation timeline

The last of three equivalent attempts is the clearest:

| Time | Event |
| --- | --- |
| 00:20:54.971 | Navigation DATA BEGIN sent |
| 00:20:57.400 | BEGIN reply status 0 |
| 00:20:57.430 | `RUNNING_CREATION foreground=4` (Navigation) |
| 00:20:57.787 | 9,103-byte image accepted |
| 00:20:57.831 | command `0x08` metadata sent |
| 00:20:57.853 | OpenNoodoe sends Navigation DONE |
| 00:20:57.897 | DONE reply status 0 |
| 00:20:59.054 | `RUNNING_CREATION foreground=0` |

Navigation remains active for about 1.62 seconds, and the transition to zero
follows the explicit DONE. Earlier attempts show the same approximately
1.5-second lifecycle.

The official app's `TaskSender` calls `createNavigationTransferTask()`, then
keeps adding/deleting cached image files and sending `UPDATE_NAVIGATION`. Its
`stopNavigationTransferTask()` path is separate and is what eventually makes
`SunrayTransferFileTaskHandler.stopTask()` send `TRANSFER_UPDATE_DONE`.

OpenNoodoe 0.6.0 instead sends DONE in `sendNavigationImage()` immediately
after one `0x08`. The dashboard is behaving consistently with the command
sequence it receives.

## Known-good Creation control

Location `0x0200` was removed at 00:19:40 and again at 00:19:42; both replies
were status 0. The preserved 22-file clock bundle then ran from 00:19:52 to
00:20:04 with status 0 for BEGIN, every file, and DONE.

After commit, the dashboard reported:

- 00:20:06.313: foreground 16, Clock
- 00:20:08.201: foreground 32, Speedometer
- 00:20:11.050: foreground 0, default dashboard
- 00:20:12.326: foreground 16, Clock

This validates the SPP framing, task IDs, CRC implementation, file ordering,
external-storage write path, and at least the known-good clock widget graph.

## Why transport success is not renderer success

For generated POI content at `0x0500`, BEGIN, both files, and DONE all returned
status 0. The final DONE reply took about 8.5 seconds, but the later running
state remained zero and the rider observed the generic error dialog.

The `0x0A`/`0x0B`/`0x0D` replies describe negotiation, storage, CRC, and task
commit. They contain no post-selection renderer result. V5.16 itself contains
the user-facing string `An unexpected error occurred.` at `0x080756D4`, so the
LCD dialog is a later local firmware error, not an SPP failure reply.

## V5.16 widget parser evidence

The firmware parser at approximately `0x08017846..0x08017D7A` reads JSON
strings and compares them against literal widget names. Relevant branches are:

| Parser branch | Accepted widget |
| --- | --- |
| `0x0801785E` | `BackgroundWidget` |
| `0x08017966` | `HourWatchHandWidget` |
| `0x08017AA8` | `WeatherConditionWidget` |
| `0x08017CDE` | `MembersWidget` |
| `0x08017D06` | `LocationsWidget` |
| `0x08017D2E` | `RadarWidget` |
| `0x08017D56` | `DirectionRingWidget` |

The complete nearby registry also contains the expected speed, clock, weather,
date, battery, odometer, and location widgets. It does not contain
`DistanceWidget` or `EtaTextWidget` anywhere in the V5.16 image.

The APK's directory-03 cfg uses exactly these incompatible names:

```text
BackgroundWidget
DistanceWidget       <- absent from V5.16 parser
DirectionRingWidget
EtaTextWidget        <- absent from V5.16 parser
```

That directory was already only a candidate: this official APK build's default
initializer copies directory 06 only. Directory 03 is not proof of an AK V5.16
installable Around Me bundle. Its 29 files all transferred correctly, but its
widget graph cannot be classified by this firmware.

## Generated Creation failure

`ContentGenerator.creation()` emits only a JPEG and this cfg shape:

```json
{"files":["...jpg"],"configuration":{"widgets":[
  {"name":"BackgroundWidget","images":["...jpg"],"imageType":"single"}
]}}
```

`BackgroundWidget` is accepted, but it does not identify a Clock, Speedometer,
Weather, POI, or Group renderer. The firmware parser assigns additional type
and discovery-mode state only when it sees widgets such as `ClockDigitWidget`,
`SpeedDigitWidget`, `LocationsWidget`, `RadarWidget`, or `MembersWidget`.
Using the same background-only cfg at every location is therefore not a valid
Creation generator.

## Internal-state assessment

There is evidence of persistent failed content: several early `0x0500` BEGIN
attempts returned status 21 `ALREADY_EXISTS`. There is no evidence of global
filesystem corruption. The successful clean `0x0200` install is a strong
counterexample.

For future tests, remove only the target location, install exactly one bundle,
and record the LCD result before another install. This makes stale content and
schema errors distinguishable. Do not remove the currently working clock or
speedometer as part of the POI test.

## Required implementation changes

1. Replace one-shot Navigation with a persistent session state machine:
   `BEGIN -> repeated file/update operations -> explicit STOP/DONE`.
2. Remove the universal background-only Creation action, or label it as an
   intentionally invalid parser probe. Build a location-specific cfg instead.
3. Build the V5.16 Around Me renderer from firmware-supported widgets. The
   official bundler gives the candidate schema: `BackgroundWidget`,
   `RadarWidget` (`centerColor`, `shadowColor`), `LocationsWidget`
   (`poiTypes`, `poiColors`, three `customLocations`), and optionally
   `DirectionRingWidget`.
4. Send runtime POIs with command `0x06` only after the dashboard reports
   foreground type 2. A sequence ACK proves delivery, not that a renderer is
   active.
5. Keep Group separate: its renderer uses `MembersWidget`, then requires a
   long-lived Group DATA task, member image files, and command `0x07` updates.

The next high-value field test is therefore a clean `0x0500` REMOVE followed by
one firmware-compatible Radar/Locations bundle. The old directory-03 candidate
should not be retried on V5.16.

## 0.6.1 follow-up

This recommendation was implemented and live-tested later the same day. The
persistent Navigation task and V5.16 Radar/Locations Creation both worked on the
vehicle; the latter produced `C1 foreground=2`. See
[`../2026-09-03-s24-v061-live-validation/README.md`](../2026-09-03-s24-v061-live-validation/README.md)
for the preserved evidence and remaining POI visual matrix.
