# 0.9.68 · Smooth colours, sharing and settings retention

Install the **0.9.68 APK over the existing app without clearing data**, then update CFW using the matching **0.9.68 ZIP**. Both the app and Product change in this release.

## Speed ring

Colour transitions now cover adjoining broad ranges: at a 200 km/h full scale, sky → green over **30–80**, green → yellow over **80–130**, and yellow → coral over **130–180 km/h**. Each transition is five times wider than before. At other full scales the ranges change proportionally. Green is slightly richer; the other anchor hues remain the same. CIELCh reference lightness stays L*=68. Ring timing, alpha and ambient/backlight controls are unchanged; IGN OFF remains gray. This is an sRGB reference calculation, not physical LCD calibration.

## Quick replies

Slots **1–7** retain editable text. **8** sends location, the current ignition session’s driving information and music together. **9** sends only the location paragraph; **10** sends only the music paragraph. Use the existing notification reply menu: open a reply-capable message, hold O, select the item and press O to send. Existing motion and recipient checks still apply.

Example of slot 8 in English:

```text
37.566500 126.978000 / https://www.google.com/maps/search/?api=1&query=37.566500%2C126.978000

Avg. 62.5 km/h Max. 123 km/h / Dist. 12.35 km

Music: Title - Artist

Sent from my Noodoe dashboard.
```

Labels follow the selected app language: English, Korean, Japanese, Spanish, Simplified or Traditional Chinese. Numbers use that locale; coordinates and the clickable Maps URL keep universal decimal syntax. Speed/distance units follow the device setting. Statistics are the device’s current ignition-session counters, not Trip A/B or phone estimates. Missing, closed or stale session information is shown as —. A recent accurate real GPS fix is required for slots 8–9; synthetic/test locations are not sent.

Slots 8–10 **always include a signature**: your saved signature, or the localized default when blank. When music is stopped/paused or metadata is unavailable, the music paragraph reads **No Media Info** in the selected language. Old text saved in slots 8–10 remains stored but is no longer used. Notification text, coordinates, songs and rider names are not added to diagnostic logs.

## Settings across a CFW update

Before resource/firmware transfer, the app saves a checked copy of the current numeric/choice preferences and rider name in private phone storage, bound to the device UID and candidate Product. Keep app data until the update completes. After that exact candidate confirms healthy boot on the same unit, the app reads settings again, restores differences and waits for device save acknowledgements and readback. Normally the existing NOR values already match, so no settings writes are needed.

The copy survives app/process or Bluetooth interruption. If restoration is interrupted, remain stopped, reconnect with the same ZIP and use **Check current CFW / installation result**. An incomplete restoration is not reported as completed installation. Absolute setting assignments may be reconciled and resumed; reset, pairing, clock and maintenance-reset actions are never replayed. Settings are not applied to a different unit or a rolled-back candidate. Existing photos, trip history and maintenance baselines remain under their existing NOR preservation mechanism; the temporary copy is not a full NOR backup. First stock installation is unchanged.

## Installation and recovery

F4 stock **5.14 / 5.16** are admitted without a hardware, bootloader-version, model or PCBA whitelist. This does not physically qualify every vehicle. Bootstrap still requires the documented manual unpair/reset/re-pair steps. Gate, Bootstrap, stock recovery, resources, Diagnostic and Uninstall images are unchanged.

When Stage 6/4 requests it, hold **UP + O together for 3 seconds**, release and let the app reconnect and verify. The manual-reset limitation is not fixed by this release. The previously running firmware can still show older wording during the first handoff. Follow the [installation guide](../Installation/README.en.md) for first installation and recovery.

Validation is software-only: ARM colour/session checks, focused Android format/protocol/settings-interruption tests, Debug/Release/APK builds, signer and matching ZIP/Product checks. No physical phone, radio, LCD or vehicle test.

![sRGB palette](../images/speed-ring-0.9.68.png)
