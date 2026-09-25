# Settings v1 — implementation and verification

Completed 2026-09-14. Product Release is installed on the connected Noodoe.
Settings are implemented as an App-owned child state machine, with separate
feature files, middleware RTC queue, reusable Graphics scene transition,
fixed round-screen layout and Material Round warning icons.

## Implemented behavior

- Quick Settings: UP/DOWN adjusts manual brightness by 5%, or auto bias by one
  step within -2..+2. Auto stores intent only and retains the manual output.
- Full Settings: O held for 2001 ms, released after fresh UART speed has stayed
  at or below 3 km/h for five seconds. UP/DOWN selects or edits; O short opens
  or applies; O long cancels, returns to the parent, or leaves.
- Display, date/time/time zone, vehicle units/scale, independent maintenance
  intervals/baselines, physical connection status/selected phone, and System
  catalogs are implemented. Unsupported pairing, ALS calibration, language
  alternatives and power/welcome settings are explicitly unavailable.
- A shared 400 ms slow-fast-slow animation expands/fades the speed ring and
  moves complete clock/ODO groups upward. Return uses the inverse pose.
- Full settings uses the 480-pixel active circle. Position arcs are 9 px wide:
  30 degrees at the top, 60 degrees on the right.
- Speed above 3 km/h or missing/stale UART locks editing and starts a fixed
  30-second warning/countdown. Repeated movement cannot extend the deadline.
  Speed strictly below 3 for three seconds restores the current draft/menu.
  Otherwise expiry discards the draft and restores the previous riding card.

Preferences and service baselines are RAM-only for the power session. The UI
says Applied rather than Saved. Confirming date/time is the explicit exception:
it requests an RTC write and readback. No NOR format/migration or automatic
settings persistence was added.

## Build and install

| Configuration | APP bytes | Remaining APP bytes | Result |
|---|---:|---:|---|
| Product Release | 427364 | 31388 | Built and installed |
| Product Debug | 457924 | 828 | Built |

Release SHA-256:
`e77408e6158b184f77d0856b3bd789cd721c736c63d0a6ef120f9e1f81b930ee`

Lower 64 KiB before/after SHA-256:
`f38ed839c4fbce74d4d8227fcf13764194f405c4860cda75c628fce66064b574`

APP starts at `0x08010000`; no BL/lower-64-KiB or option-byte changes.
[Installation evidence](install-complete/summary.json) records successful full
APP readback and two reset cycles with advancing HAL/RTOS ticks and heartbeat.
Release/Debug logs are `build-release-complete.log` and
`build-debug-complete.log`. The existing RWX LOAD-segment linker warning remains;
new C compiler warnings were not reported. Some Product Debug files use the
documented Os/g3 policy, which limits local-variable/line stepping.

## Automated validation

| Suite | Coverage/result |
|---|---|
| settings_ui | Actual ARM C, 70 checks each at O0 and Os; PASS |
| clock_queue | Actual RTC queue/error/expiry/readback code, 19 checks each at O0 and Os; PASS |
| graphics_input_host | 98 checks each at O0 and O2; PASS |
| product_ui | 110605 state + 2434 speed assertions each at O0 and Os; PASS |
| project_layers | Relative paths, source profiles, restoration and byte-idempotence fixtures; PASS |

The product tests had two stale expectations: speed-model input time is now
already local; Music DOWN already emits NEXT rather than selecting a child
row. Those test expectations were corrected without changing media behavior.
No CubeMX GUI operation was performed; regeneration coverage is fixture-based.

## Device evidence

- [Motion/edit test](motion-and-edit/result.json): UART 4 locks, exactly 3 does
  not recover, 2 held for three seconds recovers with the draft intact;
  deadline restores the prior GPS card.
- [Final control test](final-device-resume/result.json): auto bias clamps at +2
  without changing PWM, manual 25% restored, unconfirmed 30% draft cancelled,
  warning/deadline recovery and renderer diagnostics pass.
- [Confirmation and UART loss](confirmation-and-loss/result.json): cancel-first
  service confirmation, cancellation without applying a baseline, and actual
  COM11 sender stop produce invalid speed and the settings lock.
- [PWM register read](confirmation-and-loss/tim5-read-only.log): TIM5 PSC=1679,
  ARR=99, CCR4=25, consistent with the restored 25% PWM setting.
- [Final live health](final-health/result.json): settings closed, previous GPS
  card 6 restored, fresh UART speed 61 km/h, 29.9 FPS, CPU estimate 66.5%,
  zero missed frame slots, zero graphics/SPI errors, zero renderer warnings,
  and no recorded fault. These are live samples, not a worst-case guarantee.
- COM11 115200 8N1 speed 1→200 repetition is restored and left running.
  Its status/stop marker live in `uart-ramp-restored`.

Timed test-button stimuli use the DATA_DEBUG-only mailbox and the real
ButtonEvents routing. This does not establish that the physically damaged UP
input has recovered. RTC failure paths were tested with the actual queue code
and host stubs; an arbitrary date was not written to the user's RTC for testing.
Actual BT/ALS functionality is not claimed.

## Actual EVE captures

These are device GPU output, not photographs of the LCD glass/backlight.

- [Quick Settings](final-device/quick/display.png)
- [Brightness editor](final-device-resume/brightness-editor/display.png)
- [Motion warning](final-device-resume/moving-warning/display.png)
- [Final service confirmation](confirmation-and-loss/confirmation/display.png)

The final installed image adds the two-line confirmation wording. Only a
header comment and test expectations changed after its build; runtime source
and installed binary remain the verified release above.
