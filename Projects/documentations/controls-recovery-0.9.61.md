# 0.9.61 controls and recovery display validation

Companion versionCode91 / versionName0.9.61 and its matching Product implement
gray status pulses, explicit ODO checking in Settings and a manual-reset error
instruction. No hardware, radio, ignition or vehicle tests were available.

## Gray inactive phases

The120ms GPS inactive phase now uses the normal muted gray0x666666 instead of
transparent pixels. The Bluetooth battery animation uses red/gray below5% and
green/gray while charging below75%, replacing the previous white alternate
phase. Thresholds,500ms color phases, GPS duplicate/burst handling and480ms
minimum lit time remain. Active calls keep their existing phone-glyph priority.

## ODO: explicit settings entry and usable exit

The owner only renders ODO check when SETTINGS > Vehicle > ODO check is open.
Pending anomalies no longer summon a popup during riding or boot. This local
menu action is outside persisted SettingKey values and the remote catalog;
the existing phone Device settings ODO endpoint remains available. The original
record format, candidate quarantine, offset arithmetic and storage path are
unchanged. Successful choices remain visible as No change to confirm until O
returns to Settings. Back and long O return without applying, even with unknown
speed, rejected requests or invalid readings. Closing or leaving Settings also
removes the child.

The user clarified that only Ask me later had worked. The old guard can mark an
anomaly pending after three seconds, while accepting either value requires five
seconds of valid stationary speed with IGN ON. The automatic popup could thus
open before either value was actionable. This state is reproduced in the ARM
test, and both values work after the gate becomes ready. The exact historical
vehicle state was not logged, so this is a reproduced explanatory path, not
proof of every field failure. The new settings entry already uses the stationary
gate. If readiness is lost, the child says IGN ON; stop for 5 sec. without blocking
exit; unknown speed or out-of-range data are not accepted as a workaround.

Separately, the view redundantly required80ms after the common router had already
classified a SHORT gesture. A40ms normalized release was accepted by ordinary
menus but ignored here. The pre-change test fails at the dashboard-selection
assertion (line52); the view now consumes the common SHORT/LONG contract without
reclassification. Raw PRESS/RELEASE, stale candidates, PH9 transitions and held
keys still cannot approve a value.

## Installation error text

The INSTALL_ERROR branch previously showed Paused. See phone help. It now shows
the following centered three-line instruction, retaining Stage and Code/phase:

```text
Please manually reset.
Hold UP + O together
for 3 seconds.
```

A dedicated error layout omits the progress bar and unrelated cancel/pairing
hints so the instruction fits the round480px display. The existing Health-owned
physical UP+O3s reset path is unchanged. The change does not bypass image/key/NOR
checks, declare installation successful, or fix every underlying reset failure.
The first handoff from an older firmware continues to execute that older code
and show its original text until the new Product boots.

## Verification

- Product Debug and Release builds pass. Free flash:1,388B Debug and33,456B
  Release; minima remain1,024B/16,384B. SRAM free49,296B/48,320B; CCM16,320B.
  Compiler options, fonts, icons, heap, stacks and reserved memory are unchanged.
- ARM indicator policy:180 assertions at O0 and Os.
- ARM ODO guard/service/view:72 assertions at O0 and Os, including both value
  choices,3s/5s readiness, blocked exits, busy/range failures, persistence,
 40ms normalized input, held/stale/PH9 input and explicit manual reopening.
- Actual ProductUI/ButtonEvents/SettingsUI:10,000 scenarios,240,001 assertions;
  explicit ODO settings entry and leaving-state checks join the existing route
  and emergency-input regressions.
- Actual LVGL/EVE software rendering: six install/recovery and three ODO frames
  pass the8,192-byte display-list bound. Manual inspection confirms the reset
  instruction and ODO choice/readiness text fit. The old install fixture lacked
  the current0xC0000000 SDRAM mapping; its emulator mapping was corrected to the
  existing hardware address, without changing firmware memory allocation.
- Android554 tests pass with no failures/errors/skips; six help languages have
  paired ten-topic arrays and current0.9.61 recovery/ODO text. Lint has no errors
  or fatal findings. Existing signing identity and APK/Product hash match.
- All non-Product payloads, including Gate/Bootstrap/uninstall/diagnostic/stock/
  resources, match0.9.60 byte for byte. Relink objects pass D8/zipalign/signature
  reconstruction without application source or signing keys in the public kit.

Manuals distinguish the reproduced timing/gesture problems from unobserved
hardware causes and retain code/phase instructions. Software correctness and
rendered frames do not establish real LCD frame rate or wireless installation
success. Source remains private; public release tags refer to documentation.
