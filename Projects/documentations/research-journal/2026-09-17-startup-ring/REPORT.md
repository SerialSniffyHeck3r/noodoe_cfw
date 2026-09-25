# Startup ring sweep — installed 2026-09-17

## Behavior

Cold boot with IGN ON, or an ON event after acknowledged display sleep,
selects Home. Existing Welcome and shell entrance complete first. A displayed
Home frame starts the0→maximum→0 speed-ring sweep. Each leg uses the existing
slow–fast–slow cubic for800ms. Displayed peak and zero endpoints are separately
acknowledged; a stalled renderer cannot skip either endpoint on a time jump.

Only the visual ring is overridden. UART reception, raw speed validity, trip
distance/time and movement interlocks continue normally. After the displayed
zero endpoint, the ring waits for valid telemetry stamped strictly later than
that endpoint's acknowledgement. Cached old speed, stale data and non-speed
frames cannot release the gate. The first new speed resumes the normal150ms
interpolation from zero. No UART flush, blocking sleep, heap allocation or
synthetic telemetry publication is added.

IGN OFF cancels the sweep at every phase; the existing provisional1s hold still
preserves the visible frame. Warm OFF-delay/summary/AWAKE reversals do not
replay the sweep and retain the selected category. Footer/trip/reserve values
are not reset by cold-home selection.

## Code

- `App_Logic/UI/inc/SpeedHome_Startup.h`, `src/speed_home_startup.c`: bounded owner-state phases and packet fence; `g_speed_startup` SWD diagnostics.
- `speed_home_model.c`: presentation-only arc override, resetting the interpolation origin without changing raw validity.
- `ui_power.c`: select Home on cold/dark wake; preserve warm selection.
- `product_ui.c`: observe IGN before the freeze/compose gate, then use actual frame-presented and speed telemetry timestamps.
- `tools/services_build.py`: retain new source and Product Debug policy across regeneration.

No IOC, generated Core, BSP, vendor source, font/layout or resource package changed.

## Verification

- Product ARM C: O0/Os/Oz,24 test groups each;110644 UI assertions plus5697 speed assertions per configuration. Includes800-step monotonic legs, midpoint/endpoints, delayed scanout, no packet, old/equal/stale packet, fresh packet, existing150ms interpolation, tick/frame wrap, cancellation at every phase, warm return and cold-home selection.
- Existing power adapter ARM tests: O0/Os31 assertions each, preserving delayed shutdown/summary/wake behavior.
- Project-restoration fixture: Product/Integrated/Graphics passed. Actual GUI Cube regeneration was not performed.
- Release/Debug builds passed, with existing RWX LOAD segment warning. Release APP370740B/free88012B; Debug385868B/free72884B; SRAM free35776B; CCM free16320B. No heap/stack/queue reductions.

Actual board, after APP installation and reset with physical IGN ON and UART paused:

| Evidence | Value |
|---|---:|
| Sweep starts / completions / cancellations |1 /1 /0|
| Home-frame acknowledgement / upward start |4331ms|
| Peak acknowledgement / downward start |5149ms|
| Final zero-frame fence |5984ms|
| Later no-UART observation |65973ms, phase7 waiting, ring0, Home card0|
| First newly accepted UART telemetry |145483ms|
| After actual73km/h UART |phase0 LIVE, speed_valid1, arc3650/10000|

`waiting-packet` and `fresh-packet` contain original live SWD reads. These are
not direct speed RAM injections. The reset cold-boot path was exercised on the
board; a separate physical IGN sleep→ON cycle was not performed in this turn.
No claim is made to have captured a video of the panel animation.

The continuous0..200km/h driving scenario was resumed after the fixed-speed
test; `drive/status.json` reports progress, and creating `drive/stop` ends it
cleanly with a final0km/h frame. Existing simulated ODO continues from36576km.

## Installed identity

- APP: `d99f838e632236e875bb09aa8f1a4900ec4a36cbb406e89d139bf4167c82e265`
- ELF: `c0ea8f1f3976fd814bfd55b20295cc4f6b5fa73b03b066cb2e39462e6a9b4dc9`
- APP-only at0x08010000, complete readback matched. Original lower64KiB preserved. NOR/option bytes unchanged.
- Installation evidence: `install/result.json`, manifest and programmer logs.
