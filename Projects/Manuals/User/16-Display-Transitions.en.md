# 0.9.26: background shading and Bluetooth validation

[한국어](16-Display-Transitions.md) · [User guide](README.en.md) · [Latest downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Update the APK and Product ZIP together. The existing Gate, settings, photographs and maintenance records stay in place.

**0.9.26 restores the pre-map 0.9.5 background-brightness policy.** TRIP, map, smartphone/calls and other non-HOME pages use the saved photo-brightness setting × 20%; music/artwork uses × 32%. A 50% user setting therefore produces 10% and 16%, respectively. This is not a fixed 25% override.

All eight HOME modes and panel backlight settings stay unchanged. Fuel-warning attenuation, map-road/text brightness and GPU retirement checks remain in place.

![Album art — 0.9.26 EVE software simulation](../images/album-0.9.26-simulation.png)

The maximum map scale is **3km**. Complete tile geometry stays selected while moving instead of competing for a fresh per-frame road quota. Road classes retain their colors and widths, with a minimum visible width for thin roads. Connected roads are selected before their curves are generalized to fit the fixed drawing budget. Source data and scale still determine map generalization.

![3km map — EVE software simulation](../images/map-3km-0.9.22-simulation.png)

The 0.9.22 primitive-state restoration and presentation acknowledgement changes remain. Software tests overlap transitions, delayed swaps, warnings and notification popups. The reported animation-time horizontal tearing on the vehicle still has no confirmed cause or fix.

![TRIP — 0.9.26 EVE software simulation](../images/trip-0.9.26-simulation.png)
![Audio — 0.9.26 EVE software simulation](../images/audio-0.9.26-simulation.png)
![Fuel warning — 0.9.23 EVE software simulation](../images/fuel-0.9.23-simulation.png)

These are **software simulations** of actual firmware/LVGL/EVE commands. The previous warning already looked dark in software, so the revised implementation does not establish that the vehicle warning-backdrop problem is solved.

High-speed Bluetooth compares three post-patch version/address pairs. Comparing patched replies directly against the ROM identification was incorrect and has been corrected. If `High-speed Bluetooth didn't start` appears, the device falls back to 921,600baud. Reduced waits and pipelined transfers can still make that path faster than older releases. Actual 3,686,400baud validation and sustained radio operation remain untested on hardware.

Before an update reset, the device now waits for newly received pairing keys to become durable. Failed persistence does not force a reset. The first update into this version is still reset by the old firmware; the new barrier cannot apply retroactively.
