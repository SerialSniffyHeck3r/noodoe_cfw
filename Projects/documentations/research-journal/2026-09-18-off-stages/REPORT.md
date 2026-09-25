# Three configurable ignition-OFF stages — 2026-09-18

The installed APP is `candidate.elf`, programmed only at0x08010000. Full APP
readback and unchanged stock lower64KiB were verified. NOR and option bytes
were not written. APP SHA256:
`c4a69c5376a9c58eca8b399d1a4de4ab64b4e2ad96f2cd09a5cf1593f41a0b60`.

## Behavior

The fault in the prior behavior was policy coupling: AWAKE used a per-minute
wake/render/sleep cycle, so a request to turn off just the lamp also slept the
LCD/EVE. The renderer now keeps the panel and EVE continuously scanning the
retained frame in OFF_DISPLAY_HOLD. It only sets PWM0, disables continuous UI
composition, and submits one clock/ODO update at entry and on a valid minute.
The next state explicitly owns actual panel/EVE sleep.

| State | Panel | Lamp | BT | CPU | Default time |
|---|---|---|---|---|---|
| OFF_DISPLAY_HOLD / Screen hold | ON | 0% | retained | tickless Sleep |60min|
| OFF_BT_HOLD / Bluetooth hold | OFF | 0% | retained | tickless Sleep |60min|
| OFF_DEEP_SLEEP / Deep sleep | OFF | 0% | stopped | coordinated STOP |until IGN ON|

System → Power sequence contains independent Enabled/Skipped fields for all
three stages and0..1440minute, one-minute-step times for the first two. The
last stage has a read-only Until IGN ON duration. Disabled stages are skipped;
zero-minute intermediate stages are skipped. The last enabled stage is held
until ignition, including when its stored time is zero. Disabling the final
remaining enabled stage is rejected. Empty/invalid masks fall back to screen
hold rather than forced deep sleep.

The1s exact previous-frame/brightness hold and5s Ride Summary remain. A short
OFF/ON retains the existing ride; only the committed end closes a session.
Time/link/button events never extend OFF timers. IGN from screen hold requires
a fresh ON frame before PWM and runs the Home ring sweep without Welcome.
Actual sleeping-panel wake keeps the existing settling/scanout/Welcome gates.

Deep sleep is not a demonstrated physical board power cut. No unknown
PD13/PG14/PI9 rail control is asserted. Actual STOP waits for IO/storage/
graphics/BT acknowledgements and inactive DMA. SDRAM self-refresh and clock
restore remain owned by BSP. RTC and inherited IWDG bound sleep intervals.
The first two stages retain UART/DMA clocks and Bluetooth sessions; only the
explicit final state issues controller stop.

These preferences follow existing **RAM session** AppSettings lifetime. This
change does not add NOR settings persistence, an automatic format, or a new
resource-container schema. AppSettings/SettingsUI RAM layout versions are2;
PowerUI diagnostic values4/5/6 retain their old numbers and DEEP is appended7.

## Code ownership

- `App_Logic/UI/src/ui_off_stages.c`: bounded pure stage selection.
- `ui_power.c`: IGN/summary/deadline transitions; `power_ui.c`: hardware policy.
- `App_Logic/Settings/src/settings_power.c`: feature-specific catalog and mask.
- `settings_catalog.c`, `settings_ui.c`: parent lookup and retained selections.
- `Graphics/UI/src/settings_view.c`: minute unit in numeric editor.
- `tools/services_build.py`: regeneration-safe source/optimization selection.
- Cube-generated Core/IOC, vendor files and peripheral pin configuration were
  not edited. Main's existing `LCDTest();` contract is unchanged.

## Verification

- Real STM32CubeIDE Product Release and Debug: both build successfully. Existing
  RWX LOAD-segment linker warning remains; no new compilation error.
- Release APP372,996B / free85,756B; Debug APP388,428B / free70,324B.
- General SRAM free35,736B(Release)/35,728B(Debug); CCM free16,320B.
  FreeRTOS48KiB and guarded LVGL48KiB pools are unchanged; budgets pass.
- Pure UI ARM O0/Os/Oz:110,946 state checks plus5,697 speed checks each. Includes
  all7 enable masks ×4 zero-duration combinations, deadline edges/tick wrap,
  last-stage retention, and exactly-once ride end.
- Power UI actual C with mock hardware:1,151 checks each O0/Os, including100
  minute updates with panel awake/PWM0 and100 interrupted sleep/wake cycles.
- LowPower/PowerService ARM register model:445 checks each O0/Os; owner/RTC/DMA/
  watchdog/clock/SDRAM contracts. Does not measure actual power consumption.
- Settings ARM O0/Os:223 checks each, including new subtree Back parent,
  draft/apply/cancel, range validation and refusing an empty stage mask.
- BT transport/controller O0/Os: all scenarios pass; retained scenario now408
  checks, including two phones+OBD retained housekeeping, no premature deep
  ACK and exactly one controller restart. Not real RF validation.
- Cube regenerated-metadata fixture: Product/Integrated/Graphics source,
  include and linker policy restored; idempotent; Core/IOC unchanged. GUI
  regeneration was not executed in this turn.
- Actual installed ON sample:29.9FPS, CPU56.8%, graphics/SPI/UI errors0, no
  valid fault record, RTC ready. Date is unset and remains invalid; no
  arbitrary clock date was written. Initially UART was not receiving because
  the previous PC drive process had ended despite a stale running status file.
- Actual UART0km/h and timed ButtonEvents enter the new menu and verify a
  duration edit59 then cancel back to60. `settings-bench` contains capture and
  final navigation result when completed. Test events are not physical GPIO.

## Limits

Physical IGN transitions of this installed candidate, current draw and real
Bluetooth retention still require a bench check. Host tests are not presented
as proof of those electrical/wireless behaviors. New configuration defaults
are60min+60min, not a temporary short test timing. Final verified observations
and menu result are linked in `verification-summary.json`.

## Final bench completion

The new Power sequence page was captured from the actual EVE as480x480 pixels;
all460,800RGB565 bytes were downloaded and verified. The Screen hold Enabled /
Screen hold time60min layout is readable and centered. Timed ButtonEvents
successfully navigated System → Power sequence and edited60 to59, then canceled
and reopened to verify60. No preference was changed by this bench test.

While navigating Back after the completed capture, ST-LINK failed to read the
core ID (3.30–3.32V reported). Read-only reconnect also failed. This does not
invalidate the earlier full APP/BL readback or completed screenshot, but final
normal Back exit and physical OFF-state measurement were not confirmed. The
UART driving process was restarted on COM11; PC TX success alone is not proof
of device reception. The final status is in verification-summary.json. No
additional flash/reset action was used to hide the interrupted diagnostic.

After the SWD failure, real UART replies continued: RX 3749 bytes, latest sampled reply `f5 a1 02 01 00 57`. This proves the UART responder remains active, not full OFF-state or wireless correctness.
