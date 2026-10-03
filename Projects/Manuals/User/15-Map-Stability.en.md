# 0.9.21: map and display transitions

[한국어](15-Map-Stability.md) · [User guide](README.en.md) · [Latest downloads](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/latest)

Update the APK and Product ZIP together. An existing CFW installation can update directly, keeping its Gate, settings, photos and maintenance records.

## Hold O on HOME

| HOME view | Hold O |
|---|---|
| Auto / Speed+Auto / Home Map | Advance information and restart its timer |
| Date / Compass / Phone / Music / Dual | Cycle the enabled ODO/TRIP footer modes |

An active Reserve override keeps ownership of the footer. Short O still moves to the next main menu. Popups, replies, warnings and settings retain their own button handling.

## Maps and shading

Waiting for a map and displaying it now use the same 25% photo shading pass. Roads remain bright, with the shared upper/lower gradient and clock/ODO exclusion. Completed geometry stays visible while replacement data is prepared; music and notification images no longer invalidate it. Nearby tiles stay cached while off-screen tiles are replaced.

![Map — EVE software simulation](../images/map-0.9.21-simulation.png)

## Warnings and popups

Fuel warnings keep their dark backdrop above a simultaneous key-on transition. Notification popups are centered using the actual displayed ink height, including the packed row layout, rather than the storage-buffer height. Font sizes and line spacing are unchanged.

![Notification — EVE software simulation](../images/popup-0.9.21-simulation.png)
![Fuel warning — EVE software simulation](../images/fuel-0.9.21-simulation.png)

Frame fences and GPU buffer reuse checks have been tightened. These pictures and repeat tests are **software simulations** driven by actual firmware commands. Whether the intermittent horizontal tearing has disappeared on the physical LCD still needs a vehicle test.
