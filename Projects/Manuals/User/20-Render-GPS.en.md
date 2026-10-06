# 0.9.42 GPS testing, parked controls and rendering

Install the matching APK and Product ZIP. A compatible 0.9.40-or-newer Gate uses the ordinary CFW update; this fix does not require another Gate installation.

## Show a map using test coordinates

1. Import an offline Mapsforge `.map` file and wait for **Map ready**.
2. Enable maps in both the app and Noodoe, then connect riding sync. Stationary IGN ON or explicit parked use is sufficient.
3. Open **GPS test position**, enter coordinates inside the imported region, and start/apply. With the South Korea map, try Seoul City Hall at `37.5665, 126.9780`.
4. Open Noodoe's map page. Synthetic coordinates work without real location permission or a location foreground service. Bluetooth permission and connection are still required.
5. Stop the test to return to real GPS, which retains its normal permission/service checks.

Map-file errors now remain visible while test mode is active. Test coordinates do not replace a missing regional map, disabled map setting or disconnected session. They never fake vehicle speed or IGN.

Phone fixes expire after five seconds on both sides. A four-second provider interval no longer produces the former one-second invalid gap. Actually missing fixes still expire.

![Seoul test coordinates — EVE software simulation](../images/gps-test-seoul-0.9.42-simulation.png)

## Parked use and held-key icons

Short O works normally during explicit IGN-OFF use. Holding O through the subsequent key-ON transition for two seconds retains the Gate recovery gesture. Arming that future gesture while OFF no longer owns ordinary menu input.

O+DOWN shows both held keys while opening quick settings. Physical feedback remains separate from consumed actions. UP+O for three seconds still resets the MCU; quick-settings UP long restores the display and DOWN long enables Super Nite.

## Rendering

Dedicated EVE SPI exchange reduces CPU work; smaller timed map slices yield to the UI. Completed-frame validation and actual swap fences remain. The target is 30fps, but instruction models and software frames do not establish physical LCD FPS.

[Validation and memory report](../../documentations/validation/0.9.42-render-cadence.md)
