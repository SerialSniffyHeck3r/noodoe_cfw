# Display and status icons · 0.9.61

[Usage guide](README.en.md) · [한국어](23-Controls-Recovery.md)

The five menu icons have fixed positions; the selected HOME, Trip, Music, Phone or Map icon is larger. **Only HOME fades the strip after five seconds without menu activity.** Other riding pages retain it. Settings, warnings, installation and power overlays use their own layouts.

## GPS reception

The GPS icon is lit while phone location transmission is enabled. A fresh accepted position produces a **120ms fully gray pulse**, then it lights again. Redrawing the screen or observing the same sample does not restart the pulse. Rapid bursts are combined so each pulse has at least 480ms of lit time afterward. Disabled transmission, disconnection or an expired enabled-state report gives gray. A lit icon does not establish fix accuracy or completed map loading.

## Phone battery on the Bluetooth icon

This is the **connected phone battery**, not the vehicle or Noodoe supply. Charging takes priority over low-battery warnings.

| State | Indication |
|---|---|
| Not charging, 20% or above | Normal connected color |
| Not charging, 15% to below 20% | Yellow |
| Not charging, 5% to below 15% | Red |
| Not charging, below 5% | Alternating red/gray |
| Charging, below 75% | Alternating green/gray |
| Charging or full, 75% or above | Steady green |
| Battery information unavailable | Normal connected color |
| Phone disconnected | Gray |

Alternating colors use half-second phases. During a call the existing green call icon takes priority. Unread-notification colors belong to the central Phone menu icon and are separate from battery indication. Use the matching **0.9.61 APK and Product** to transmit charging state.

## Fuel warning

After the initial large blinking icon shrinks and its text appears, the icon/text group is centered vertically. Its shape, size, timing and full-screen dark veil remain as in 0.9.59. Low Fuel, Fuel Level Critical and sensor-error messages use this layout.

The user reported successful stock-to-CFW installation without the former 6/4 symptom and confirmed the warning veil on their device in 0.9.59. This is a user-observed result for that device, not validation of every revision or every CFW-to-CFW path. Positioning is unchanged from0.9.60; the0.9.61 controls and indicators have software validation.

![Fuel warning — current ARM/LVGL/EVE software rendering, not a hardware photograph](../images/fuel-centered-0.9.60-simulation.png)

## Manual ODO check

ODO checks no longer interrupt riding with an automatic popup. Open **SETTINGS → Vehicle → ODO check** on Noodoe, or the app Device settings ODO page. Keep saved distance and Use dashboard value require IGN ON and five seconds of valid stationary speed. The old prompt could appear after three seconds, before applying was allowed; Ask me later could already close it. This timing path is reproduced in software; the exact reported vehicle state was not logged.

Short UP/DOWN selects, short O applies. **Back or holding O exits without changing the value**, including after a rejected request or missing speed. A successful choice shows No change to confirm; press O to return. Unknown/invalid readings are not silently accepted. The original record format, offset logic and storage checks remain.

## Paused installation

The former Paused. See phone help. error screen now says:

```text
Please manually reset.
Hold UP + O together
for 3 seconds.
```

Stage and Code/phase remain visible. Release both buttons after restarting and let the app reconnect to the same device/operation. This is a manual recovery instruction, not a declaration that installation succeeded or that every underlying storage/reset failure is fixed. Export diagnostics if it repeats. Older running firmware still shows its own error text until the new Product executes.

![Current LVGL/EVE software simulation, not a hardware photograph](../images/restart-paused-0.9.61-simulation.png)

![Current LVGL/EVE software simulation, not a hardware photograph](../images/odo-dashboard-0.9.61-simulation.png)
