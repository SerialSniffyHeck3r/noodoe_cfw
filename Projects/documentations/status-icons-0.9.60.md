# 0.9.60 display, phone status and manual validation

Companion versionCode90 / versionName0.9.60 and its matching Product add the
requested display refinements. The user reported that the previous0.9.59
stock-to-CFW installation completed directly without the6/4 symptom, and that
the ignition fuel warning finally dimmed the background on their device. That
report is recorded separately from this release's software verification. No
physical LCD, ignition, Bluetooth or vehicle access was available for0.9.60.

## Fuel warning positions

Only element coordinates change. During the existing shrinking animation the
icon/accessory/text group moves down, reaching a24px offset from0.9.59. The
initial large blinking icon retains its original position. The settled Low Fuel
orange-ink bounding box moves from y48..382 to y72..406: center239 on a480px
screen, within half a pixel of the display center. Rounded vector geometry,
icon/text size,32px font, timing and final EVE full-screen veil remain unchanged.
The same placement applies to critical-fuel and sensor-error accessories.

Current production ARM/LVGL/EVE software rendering passes45 warning policy
frames:36 dimmed and9 expired, across all warning levels and riding/welcome/
parked-return states. Eighteen injected alpha/vertex, blend/color-mask and
stencil/scissor faults preserve the expected pixels. The preview is generated
from this renderer, not a photograph of hardware.

## Persistent menus and deliberate GPS pulses

The five central page icons auto-hide after5seconds only on HOME. Trip, Music,
Smartphone and Map retain their strip. Full-screen warnings/settings, power and
parked-use layouts keep their existing priority; this changes idle hiding, not
modal layout ownership.

Each new valid GPS sample gives a120ms completely transparent OFF pulse.
Repeated UI observations of the same sample cannot extend the pulse. Fast
bursts are coalesced with at least600ms between pulse starts, leaving at least
480ms lit. Invalid fixes or a disconnect clear the pulse state. Existing location
subscription/freshness semantics are preserved; a lit icon is not a claim of
map readiness or position accuracy. Unsigned timing handles tick wrap.

## Phone battery and protocol

The upper Bluetooth icon now follows phone battery status. Discharging below20%
is yellow, below15% red and below5% red/white alternating. Charging takes priority:
below75% green/white, at least75% steady green. Alternation uses500ms per color.
Unknown battery keeps the normal connection color; disconnected stays gray.
An active call retains the existing green phone-glyph priority. Unread-message
colors remain on the central Smartphone menu icon.

Android was not sending the available charging status. Phone summary command0x9d
now uses schema2 and flag bit2 for CHARGING/FULL, preserving its24-byte payload
and existing epoch framing. Cable presence alone is not considered charging.
Schema1 remains accepted and clears charging; unknown versions/flags are rejected.
Invalid battery level/scale remains unknown; arithmetic clamps without overflow.
The current exact APK/Product hash contract prevents new riding payloads from
being sent to old Product. Installation/recovery access to old Product remains.

## Manuals

The public manual has current Korean and English indexes, status-icon guides,
English first-install/update/recovery/troubleshooting procedures, and an updated
English usage guide. The former docs/manual URL points readers to the current
indexes. Full manual opens Korean for a Korean app selection, English otherwise.
All six bundled help locales have the same nine topics and current map/recovery
controls. Existing versioned release chapters remain historical records.

Instructions distinguish transfer completion from candidate confirmation,
compatibility rejection from an unknown operation outcome, MCU reset from
display-only recovery, and IGN OFF from permanent-power removal. Current5minute
trial/5second health checks replace stale180second advice in the current flow;
older running firmware still supplies its own deadline. Stage6/8 Code6/Phase4
is not described as proof of either a brick or a successful install.

## Validation

- Android:554 tests, zero failures/errors/skips; six-locale help pairing and
  language routing, battery encoding, and actual current/previous bundle import.
  Lint: zero errors/fatal findings; pre-existing warnings are recorded in JSON.
- ARM indicators/warning policy:180 assertions at O0 and Os, including battery
  thresholds, charging priority, GPS duplicate/burst/disconnect/tick wrap.
- ARM phone-summary parsing: O0 and Os pass schema1/schema2 and flag rejection.
- ARM dashboard pages: six O0/Os/Oz × DATA_DEBUG0/1 runs pass existing navigation
  and new non-HOME idle visibility checks.
- EVE display stress:1,203 frames, peak4,984/8,192 display-list bytes, zero GPU
  or shade overwrites. Warning45frames/18faults pass as above.
- APK certificate matches0.9.59; APK/Product hash and padded Release image match.
  Gate, Bootstrap, uninstall, diagnostic, stock and resources are byte-identical
  to0.9.59. The LGPL object kit is rebuilt through D8/zipalign/signature validation
  and checked for application source and signing-key exclusions.

| Product build | Flash free | SRAM free | CCM free | Hard flash minimum |
|---|---:|---:|---:|---:|
| Debug |1,592B|49,296B|16,320B|1,024B|
| Release |33,676B|48,320B|16,320B|16,384B|

Both budget gates pass. Release also meets the32KiB development target; Debug
does not. Compiler flags, memory reservations, stacks, heaps, fonts and icon
assets were not reduced. The existing map/cache/transfer and reset behavior is
unchanged. Software frames do not establish physical30fps, wireless latency or
qualification of every F4 revision. The complete measured results, source
snapshot and publication receipts are retained in the private repository.
