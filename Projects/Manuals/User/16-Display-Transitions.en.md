# 0.9.22: display transitions, maps and shading

[한국어](16-Display-Transitions.md) · [User guide](README.en.md) · [Latest downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Update the APK and Product ZIP together. The existing Gate, settings, photographs and maintenance records stay in place.

Every non-HOME page now uses the previous TRIP shading formula: 20% at the default photo brightness, multiplied by a lower user photo-brightness setting where applicable. Backlight brightness is unchanged. LOW/CRITICAL fuel warnings add a separate dark background pass.

The maximum map scale is **3km**. Complete tile geometry stays selected while moving instead of competing for a fresh per-frame road quota. Road classes retain their colors and widths, with a minimum visible width for thin roads. Connected roads are selected before their curves are generalized to fit the fixed drawing budget. Source data and scale still determine map generalization.

![3km map — EVE software simulation](../images/map-3km-0.9.22-simulation.png)

Renderer primitive-state restoration and presentation acknowledgement have been corrected. Finishing command transmission is distinguished from completing a physical frame swap. Tests overlap transitions, delayed swaps, warnings and notification popups.

![TRIP — EVE software simulation](../images/trip-0.9.22-simulation.png)
![Fuel warning — EVE software simulation](../images/fuel-0.9.22-simulation.png)

These are **software simulations** of actual firmware/LVGL/EVE commands, not new LCD photographs. The intermittent horizontal tearing reported on the vehicle still requires physical validation.
