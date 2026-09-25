# Current persistence and layout — 2026-09-22

The historical RAM-only and APP-at-0x08010000 descriptions below predate
App_Persistence and RecoveryGate. Current Product starts at0x08020000.
App_Persistence serializes its explicit stable field map into CFWCFG.DAT;
ride/service baselines use the ride store. Applying a preference to RAM is
distinct from durable completion. Pair/restart/probe actions are not saved as
preferences. Re-pair explicitly waits for the Bluetooth key generation to be
persisted before reopening pairing. Full Settings moves the clock upward and
the odometer downward. Latest captures/tests are indexed in
`../../../../analysis/2026-09-22-status-icons-bluetooth/REPORT.md`.

# Configurable OFF sequence — 2026-09-18

This supersedes earlier AWAKE panel-sleep cycling and indefinite-BT policies.
System → Power sequence controls three explicit stages:

| Label / state | Panel | Backlight | BT | Residence |
|---|---|---|---|---|
| Screen hold / OFF_DISPLAY_HOLD | ON (retained scanout) | OFF | maintained | 0..1440min, default60 |
| Bluetooth hold / OFF_BT_HOLD | OFF (retained EVE RAM) | OFF | maintained | 0..1440min, default60 |
| Deep sleep / OFF_DEEP_SLEEP | OFF | OFF | stopped | Until IGN ON |

All stages default enabled. Each has Enabled/Skipped; at least one must remain
on. Disabled stages and zero-minute intermediate stages are skipped. The last
enabled stage is held until IGN ON, even when its duration is zero: a timeout
never silently enables a disabled deeper state. A malformed empty mask falls
back to Screen hold. Times belong to state entry, are wrap-safe, and are never
extended by buttons, phone links, clock refresh or repeated IGN samples.

Deep sleep is MCU STOP, not verified physical removal of all board power.
No unknown PD13/PG14/PI9 rail switch is asserted. It waits for all four owners
and DMA idle; inherited watchdog and RTC continue to bound sleep intervals.
BT retention applies only to the first two stages. This damaged donor has not
proven actual RF link retention or electrical consumption.

Screen hold never issues panel/EVE sleep. It submits a clock/ODO frame at entry
and once per valid minute, waits for actual swap, then leaves scanout running.
BL remains0 throughout. ON from this state starts the Home ring sweep without
Welcome and waits for fresh ON scanout before PWM. ON from an unavailable
panel also waits for hardware wake and replays Welcome. Interrupted wake cannot
accept a prior epoch's frame. Existing1s provisional OFF hold and5s Summary
are unchanged; stage selection never creates another ride-end event.

Settings are session RAM, like existing AppSettings. No NOR migration/format
or new persistent writes. Source ownership: ui_off_stages.c pure selection;
ui_power.c state transitions; power_ui.c hardware policy; settings_power.c
catalog. Existing diagnostic state numbers4/5/6 are retained, DEEP appended7;
AppSettings and SettingsUI version2 track their expanded RAM layouts.

# Settings v1

The riding Settings card is Quick Settings. Full Settings is a retained child
state machine owned by the product UI task. It does not replace the product
power, warning, speed, trip or phone state machines.

## Controls and safety

| Context | UP / DOWN | O short | O long (2001 ms) |
|---|---|---|---|
| Quick Settings | Manual brightness ±5%, or stored auto bias ±1 | Next riding card | Enter full Settings if eligible |
| Settings list | Previous / next row, including Back | Open / edit / Back | Optional parent shortcut |
| Editor list | Select field / Apply / Back | Adjust field / apply / discard | Optional discard shortcut |
| Field adjustment | Adjust draft; hold repeats | Return to editor list | Optional discard shortcut |
| Confirmation | Back / Apply | Confirm selection | Optional discard shortcut |
| Motion lock | Ignored | Ignored | Leave Settings |

Entry requires fresh UART speed **≤3 km/h for 5000 continuous ms**. Eligibility
is checked on release. Full Settings locks immediately at **>3 km/h**, or when
speed becomes unknown/stale. Further motion does not restart the 30000 ms
deadline. Fresh speed **strictly <3 km/h for 3000 ms** cancels the lock and keeps
the draft and selection. Exactly 3 does not recover. Recovery wins if it and
expiry become eligible in the same tick. Unsigned tick arithmetic handles wrap.

Expiry cancels unconfirmed editing and restores the last non-Quick riding card,
its subitem and footer. Clock, speed, ODO, trips and applied preferences use
their current data. An active reserve lock keeps the reserve footer. IGN/fuel
warning priority and context tokens invalidate held input across transitions.

## Feature ownership

| Source | Responsibility |
|---|---|
| `settings_ui.c` | Menus, draft/confirm state, controls, restore destination |
| `settings_motion.c` | Pure speed/dwell/timeout policy |
| `app_settings.c` | One-slot request queue, RAM values and completion |
| `settings_display.c` | PWM brightness and wallpaper settings |
| `settings_time.c` | Local date/time → UTC request; Gregorian validation |
| `settings_vehicle.c` | Units and speed-ring scale catalog |
| `settings_maintenance.c` | Independent OIL/BELT/SERV intervals and baselines |
| `settings_connections.c` | Live radio/bond status and asynchronous pairing/restart policy |
| `settings_system.c` | Version, English, display-default reset |
| `Middlewares/Noodoe/Clock` | RTC queue, bounded worker operation/readback |
| `Graphics/UI/settings_view.c` | Fixed geometry and text; no device commands |
| `Graphics/UI/settings_position.c` | Position arcs and motion countdown |
| `Graphics/UI/scene_transition.c` | Shared reversible 400 ms eased scene pose |

Manual brightness is 5..100%, initially25. The existing PWM implementation
clamps requested100 to99% duty. Auto uses the calibrated live sensor path described below and bias -2..+2.
Sensor failure retains an explicit manual fallback; no valid lux is fabricated.

Display also exposes wallpaper enable, three registered photo slots and center
brightness. Empty photos cannot be applied. This UI does not import images.
Time uses the RTC's2000..2099 range,24-hour local display and a configurable
UTC offset (15-minute UI steps); weekday follows the Gregorian calendar.
Vehicle units are km/mi and the ring's canonical scale is20..400 km/h.
The UART parser does not currently identify a reliable vehicle model code.

Each service can independently enable distance, key-ON hours and calendar days
(0 disables a criterion). The first reached criterion makes it due. Missing
enabled criteria prevent a misleading healthy remaining percentage; a known
expired criterion can still report due. “Service completed” establishes only
that service's current available baselines, never resets ODO or lifetime usage.
Unconfigured service values retain the existing external publication path.

Connections uses the actual single-phone Bluetooth service. Pair phone opens
an explicit 120-second discoverable/SSP window (`Noodoe CFW`). Close pairing
keeps the connection; Disconnect keeps bonds. Restart Bluetooth stops HCI and
makes one bounded start attempt. Re-pair asks for confirmation, stops HCI,
clears all saved phone keys, waits for that generation to be committed to
CFWCFG.DAT, then starts the controller and opens pairing. Also forget the old
Noodoe bond in Android; the device cannot remove Android's own bond. The live
radio status shows hardware failures, not fabricated successful connections.
Operations have a 35-second limit and cannot interrupt install/trial confirmation.
No stock pairing area or arbitrary NOR address is written.

Ambient sensor shows live lux or the actual error; Retry uses the normal queued
I2C probe, never experimental pull-ups. Automatic brightness uses the existing
factory-calibrated live DashService index 0..9, mapped to 10..100 percent with
bias -2..2 indices, one-second dwell and 2 percent/100ms slew. This PWM curve is
CFW policy, not a claim to reproduce stock PWM. Stale/invalid/overridden light or
missing calibration uses manual brightness. PWM updates run only in IGN ON.
The damaged bench board cannot validate successful physical RF/lux operation.

English and UART vehicle identification remain read-only facts. Empty photo
slots require an uploaded image. Display reset preserves vehicle units/scale,
service counters, date/time and connection records.

## Application API

```c
uint32_t request;
uint32_t accepted = AppSettings_RequestChange(SK_BRIGHTNESS, 40, &request);
/* APP_SETTINGS_OK means queued, not device success. */
uint32_t result;
if (accepted == APP_SETTINGS_OK && AppSettings_GetResult(request, &result)) {
    /* result == APP_SETTINGS_OK means the owner finished applying. */
}
```

The queue has one outstanding request and returns BUSY immediately. Do not
wait in the graphics task. The product owner calls Process; other task callers
use Request/GetResult/GetSnapshot. Direct feature apply helpers and Value/Format
are owner-only implementation interfaces. ISR callers are unsupported.

Preferences and service baselines are **RAM only for this power session**.
The UI says Applied, not Saved. No NOR format, provisioning, persistent record
schema change or migration is performed. An explicit date/time confirmation
does update the RTC. The storage worker verifies RTC readback; stale queued
writes expire at4s before the UI's5s timeout. A later HAL transaction failure
can leave a partial RTC change, so it is reported as failure, not rolled back.

## Rendering and validation

All menu titles, rows and selection boxes are centered at X240. Every menu,
including root, appends a normal Back row. Editors expose fields/Apply/Back;
O enters/leaves field adjustment without applying the draft implicitly. Root
hides the top category arc. The footer contains only O to select and Material
UP/DOWN arrows to navigate; persistent technical/storage instructions and
per-row O to open captions are removed. Actual failure/apply messages remain
transient. Right-edge hints show only auto-hiding physical UP/O/DOWN keys.

Normal clock/ODO geometry stays unchanged. Full Settings uses the same480px
round viewport. The18px speed ring expands/fades; complete clock and footer
groups move above the display. The footer includes both dividers and the oil
bar. Return uses the same pose in reverse. No opacity frame buffer is used.
Settings position arcs are9px,30° centered at12 and60° centered at3. Motion
replaces these with a clockwise360° amber countdown. Material Round warning
and speed masks share the project's A4/EVE renderer, including64px scaling.

`tools/tests/settings_ui/run.py` executes the actual ARM policy/editor code in
O0 and Os (97 checks). `tools/tests/clock_queue/run.py` exercises the actual RTC
queue, readback, error and expiry paths (19 checks in each build). The existing
graphics-input suite covers124 checks in O0/O2, including idle fade and
mid-fade reversal. Hardware evidence and original
EVE captures are in `Reversing/analysis/2026-09-14-settings`.

The existing product-model regression passes O0/Os (110605 state assertions
and2434 speed assertions per build). Its expectations now reflect the local
clock-input contract and the already-existing Music DOWN=NEXT behavior.
The project-layer regeneration fixtures also pass; these do not claim that
CubeMX GUI regeneration was performed. Final install/readback, reset-cycle,
UART motion/loss, draft cancellation and PWM evidence are indexed in that
evidence directory's `RESULTS.md`.

`Settings_TestPort.h` is a DATA_DEBUG-only SWD mailbox for timed button events.
It publishes through ButtonEvents so normal observers and routing run. It has
no telemetry/state/flash write command. Actual speed tests use COM11 UART.
Physical GPIO validity and simulated button stimulus remain distinct.

Cube-generated Core/IOC code and vendor files are unchanged. The existing
build/sync tools restore new source includes and exact Product Debug Os file
exceptions after regeneration. These retain debug symbols but some local/line
stepping is optimized. APP stays at0x08010000; BL/lower64KiB is preserved.
# Trip stop threshold (2026-09-17)

Vehicle → Stop threshold is an integer 0..10 km/h, default 5. Positive values
count speeds strictly below the threshold as stopped. Zero means stationary
only. The owner applies `SK_TRIP_STOP_SPEED` to `TripComputer_SetStopSpeed`.
The classification is latched with each sample, so changing the preference
does not reclassify elapsed intervals or erase totals. Creeping still adds
distance and maximum speed; invalid/stale telemetry remains unknown time.
This follows the existing session-only AppSettings lifetime; NOR persistence
is not introduced by this setting. Restore defaults returns it to 5.
