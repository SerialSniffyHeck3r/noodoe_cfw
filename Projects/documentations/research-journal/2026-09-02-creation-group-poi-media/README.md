# Noodoe creation catalog, Group, POI, Music, and Audio

## Correction

OpenNoodoe 0.4.3 exposed MUSIC (`0x0A00`) and AUDIO (`0x0B00`) beside clock,
weather, speedometer, POI, and Group as if all were equivalent AK V1.5
creation targets. That presentation was incorrect.

The names and IDs are real official-APK definitions, but a definition does not
prove that a target is installed by the active transmitter or accepted by this
vehicle. OpenNoodoe 0.4.4 retains the constants for protocol research and
removes both from the ordinary AK V1.5 creation selector and allowlist.

## Multiple themes and the installed snapshot

The official app supports multiple server-backed Creations. `DashboardManager`
maps them as follows:

| Creation type | Dashboard type | Transfer location |
| --- | ---: | ---: |
| `trip` | 1 | navigation flow |
| `clockAnalog`, `clockDigital` | 2 | `0x0200` |
| `weather` | 3 | `0x0300` |
| `speedMeterDigital`, `speedMeterAnalog` | 4 | `0x0400` |
| `aroundMe` | 5 | `0x0500` (named POI in transport) |

The app fetches a Creation by ID from local storage or
`CreationManager.getCreationDetailInfo()`, generates type-specific files, and
writes them into the installed folder for that dashboard type. It also retains
per-type recent Creation records. There may therefore be many catalog themes,
while only one selected bundle per type is staged for transmission.

The rooted-device snapshot in this repository contains one currently installed
clock creation and one currently installed speedometer creation. It is not the
complete official catalog. The APK additionally has fixed default assets for
clock-like type 01, weather type 02, Around Me type 03, Group type 06, Music
type 08, and Audio type 09.

## Music and Audio provenance

The official `FileNegotiateCommand.LOCATION_ID` enum defines `MUSIC = 2560`
(`0x0A00`) and `AUDIO = 2816` (`0x0B00`). `DashboardManager.Dashboard` also
defines `TYPE_MUSIC = 8` and `TYPE_AUDIO = 9`. The APK has a type-08 cfg with
`MusicPlayButtonWidget`/`MusicPauseButtonWidget`, and a type-09 cfg with an
`AudioThemeWidget` and ROM file.

However, active `Transmitter_1_5.handleTriggerInstall()` installs firmware,
resource, clock, weather, speedometer, POI, gallery, and Group. It adds AUDIO
only for `DEVICE_TYPE.VERSION_2_0`; it never adds MUSIC and never constructs
`creationMusicModel`. MUSIC is therefore unreachable in this sync path.

The modern media-state path is V2-oriented too. `MusicStateInterceptor` reads
Android media sessions and calls `BluetoothManagerV20.sendMusicPlayerStatus()`.
The related `MUSIC_CTRL_SETTING` (`0x17`) and `UPDATE_PLAYER_STATE` (`0x1C`)
commands are marked version 2. This is separate from theme-file transfer.

Live AK V5.16 evidence agrees: three `0x0A00` MUSIC negotiations were rejected
with `INVALID_DATA`. AUDIO was not tried live, but it is V2-only in the active
official sync code and absent from the recovered V5.16 accepted-location branch.

## POI / Around Me

POI has two layers:

1. A type-5 `aroundMe` Creation is installed at `0x0500` and defines the
   renderer. The APK default uses `BackgroundWidget`, `DistanceWidget`,
   `DirectionRingWidget`, and `EtaTextWidget`.
2. While active, the app sends runtime coordinates with `UPDATE_POI = 0x06`
   (NOTIFY, no reply).

Each update is nine bytes: one-byte POI type, signed 16-bit X, signed 16-bit Y,
and a 32-bit place ID, all multibyte values little-endian. Types 1 through 9 are
KYMCO station, convenience store, gas station, home, office, friend/favorite,
and three special locations. KYMCO/convenience/gas use place IDs for multiple
instances. The app converts latitude/longitude into rider-relative X/Y using
the phone position and heading. Removal is X=Y=-1.

An arbitrary cfg upload to `0x0500` cannot make POIs appear. A compatible widget
graph and continuing command-`0x06` updates are both required.

## Group

Group also has a static and a live layer:

1. The type-6 creation is installed at `0x0700`. The official default cfg has
   `BackgroundWidget` and `MembersWidget` with a group ID.
2. The app opens a Group DATA transfer task for the installed group's lifetime.
3. Each member receives a dashboard-local ID from 0 through 64. Its avatar is
   sent as a Group member file keyed by that ID.
4. Positions use `UPDATE_GROUP_MEMBER = 0x07` (NOTIFY, no reply).

The update payload is six bytes: signed 16-bit member ID, X, and Y, all
little-endian. The app derives rider-relative coordinates from member GPS data.
Members beyond 2000 metres and removed members use X=Y=-1; removal also deletes
the member file.

The AK's empty radar-like screen is therefore consistent with a selected Group
shell that received neither member images nor command-`0x07` positions. It was
not proof of a complete Group implementation.

## Implementation consequence

- Archive and select complete known-good Creation bundles separately from the
  single currently installed clock/speed snapshot.
- Build an Around Me simulator around a compatible renderer plus manual
  command-`0x06` POIs.
- Build a Group simulator around its DATA task, member image lifecycle, and
  manual command-`0x07` positions.
- Keep Music and Audio disabled for the AK V1.5 profile unless device capability
  evidence selects a V2 implementation.

## OpenNoodoe 0.5.0 runtime experiment matrix

The `레이더/POI` tab now exposes both individual frames and repeatable data
patterns. These controls deliberately use the official payload layouts rather
than inventing a second test protocol.

| Experiment | Data emitted | Intended observation |
| --- | --- | --- |
| Single POI add/move | one command `0x06`, selected type/X/Y/place ID | identify coordinate orientation and icon for each type |
| Single POI hide | one command `0x06` with X=Y=-1 | determine whether the renderer removes the selected type |
| Nine-type circle | nine `0x06` frames at equal angles | identify all supported icons and whether one instance per type is retained |
| Nine-type rotation | repeated nine-frame circles, 15 degrees per step | verify live redraw, coordinate axes, clipping, and update rate |
| Clear nine types | nine `0x06` frames with X=Y=-1 | return to a known empty POI state |
| Single Group member | avatar file keyed by member ID, then one `0x07` | verify image conversion, file ID binding, and coordinate orientation |
| N-member circle | the selected avatar copied to consecutive IDs, then N `0x07` frames | test simultaneous members and dashboard member capacity |
| Group rotation | repeated N-frame `0x07` circles, 15 degrees per step | verify live member redraw and stale-member behavior |
| Group remove | hide frame plus file-control DELETE | distinguish visual hiding from persistent avatar deletion |

The motion interval is clamped to 500..10000 ms and the radius to 10..2000.
Only one motion tick may wait for the single SPP command executor. If a prior
tick is still transmitting, the scheduler drops the new tick instead of
building an unbounded command backlog. This directly addresses the application
stall seen during earlier continuous tests.

## Field procedure

1. Keep the motorcycle stationary, switch Noodoe on, connect OpenNoodoe, and
   start a new capture session from the connection tab.
2. Install the `APK 기본 Around Me 후보 렌더러`. Record whether transfer and
   activation both complete. Directory 03 exists in the APK, but this APK build
   does not activate it in its default-creation initializer, so compatibility is
   still a hypothesis.
3. Send one type-1 POI at X=0, Y=500. Then move it to X=500, Y=0 and X=-500,
   Y=0. Photograph each display state; this identifies the real axes before the
   richer patterns are interpreted.
4. Send the nine-type circle at radius 500. Record which icons appear and any
   collisions or missing types. Start rotation at 1000 ms for 10 seconds, stop
   it, and finally use `9종 모두 숨김`.
5. Install the `APK 기본 Group 렌더러`. Unlike directory 03, directory 06 is
   selected by the official APK's default-creation initializer.
6. Start a 300-second Group DATA session and wait for a successful `GROUP BEGIN`
   status before sending files. Select a high-contrast square image and add
   member ID 1 at X=0, Y=300.
7. If the single member appears, set first ID 1, count 4, radius 300 and choose
   `선택 이미지로 N명 원형 배치`. Then rotate those four members at 1000 ms
   for 10 seconds.
8. Stop member motion, remove one member, and stop the Group session before
   disconnecting. Preserve the capture directory and display photographs.

Do not run POI motion and Group motion simultaneously in the first validation.
Both share the SPP command executor, and a mixed capture would make renderer
failures harder to attribute. A successful transport send is not proof of a
visible renderer update: log the command result and the observed LCD result as
separate facts.
