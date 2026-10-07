# Display and status icons · 0.9.60

[Usage guide](README.en.md) · [한국어](22-Status-Display.md)

The five menu icons have fixed positions; the selected HOME, Trip, Music, Phone or Map icon is larger. **Only HOME fades the strip after five seconds without menu activity.** Other riding pages retain it. Settings, warnings, installation and power overlays use their own layouts.

## GPS reception

The GPS icon is lit while phone location transmission is enabled. A fresh accepted position produces a **120ms fully OFF pulse**, then it lights again. Redrawing the screen or observing the same sample does not restart the pulse. Rapid bursts are combined so each pulse has at least 480ms of lit time afterward. Disabled transmission, disconnection or an expired enabled-state report gives gray. A lit icon does not establish fix accuracy or completed map loading.

## Phone battery on the Bluetooth icon

This is the **connected phone battery**, not the vehicle or Noodoe supply. Charging takes priority over low-battery warnings.

| State | Indication |
|---|---|
| Not charging, 20% or above | Normal connected color |
| Not charging, 15% to below 20% | Yellow |
| Not charging, 5% to below 15% | Red |
| Not charging, below 5% | Alternating red/white |
| Charging, below 75% | Alternating green/white |
| Charging or full, 75% or above | Steady green |
| Battery information unavailable | Normal connected color |
| Phone disconnected | Gray |

Alternating colors use half-second phases. During a call the existing green call icon takes priority. Unread-notification colors belong to the central Phone menu icon and are separate from battery indication. Use the matching **0.9.60 APK and Product** to transmit charging state.

## Fuel warning

After the initial large blinking icon shrinks and its text appears, the icon/text group is centered vertically. Its shape, size, timing and full-screen dark veil remain as in 0.9.59. Low Fuel, Fuel Level Critical and sensor-error messages use this layout.

The user reported successful stock-to-CFW installation without the former 6/4 symptom and confirmed the warning veil on their device in 0.9.59. This is a user-observed result for that device, not validation of every revision or every CFW-to-CFW path. The new 0.9.60 positions and indicators have software validation.

![Fuel warning — current ARM/LVGL/EVE software rendering, not a hardware photograph](../images/fuel-centered-0.9.60-simulation.png)
