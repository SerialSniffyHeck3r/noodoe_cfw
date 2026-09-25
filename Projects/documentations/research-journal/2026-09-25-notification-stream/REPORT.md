# Companion / CFW 6.11.1 — notifications and visual speed markers

## Changes

The peak marker is recorded in the actual LVGL ring draw callback. Updating the model with 90% and then 40% before a render leaves a 40% marker, not 90%. The same frame contains the interpolated arc and its marker. The recorded color is the arc color at that maximum; deceleration does not recolor it. Startup sweep and invalid speed cannot create a peak. Confirmed ignition-session generation resets it; provisional OFF retains it. Raw ride peak/average data remains in IgnitionSession.

Average and peak use one radial-bar geometry: 12px average, 6px peak. Rounded centerlines are shortened to keep both within the 22px ring. Angle uses interpolated LVGL sine values rather than integer-degree jumps. No EVE/vendor driver change.

## Notification root causes and correction

1. `NotificationHistory.refresh()` was re-admitting every active Android notification every second.25 active notifications continually evicted/re-added one another through the10-entry cache, assigning new IDs/revisions forever. The regression failed on the original source (baseline-reproduction.log). Polling now only reconciles removal/reply availability; live callbacks alone admit notifications.
2. Listener/app activation creates a fresh cutoff and grant token. Old tray messages are not imported; disabled/re-enabled sources and late transfer completions cannot resurrect old content. Identical reposts ignore postTime-only changes; removal is idempotent. Own service notifications, group summaries and ongoing/foreground service updates are excluded. Explicit notification test remains supported.
3. Metadata previously pointed at images before their transfer/CRC/decode completed. Continuous revisions could keep the screen referencing unavailable images. New NotificationDelivery freezes one active transfer, coalesces pending metadata to ten, and publishes only receiver-READY content. Delivered readable pages stay available during bursts. Chronological revision delivery keeps newest-first order with old firmware too, without a protocol extension.
4. Battery changes used to reset every notification revision and regenerate/reupload ten panels. A single completed header is now composed into the selected notification destination buffer.14 fixed SDRAM cache slots and existing GPU frame fences are retained; cached bodies and outgoing page textures are not modified.

Burst overload deliberately drops intermediate pending entries beyond ten; it does not claim lossless unlimited notification history. Disabling access clears delivered messages at the next companion update. No notification text is added to persistent logs.

## Stock font decision

The existing USB/NOR investigation (`Reversing/docs/2026-09-09-noodoe-usb-acquisition.md` and its filesystem/content reports) found no verified reusable font file; the font directory target contains JPEG data and no intact TTF/OTF was recovered. An undocumented proprietary format is not ruled out. This patch retains the validated phone-rendered CJK path and makes no NOR/font repair claim.

## Validation

- 152 targeted Android tests pass, including2000 arriving messages during50 immutable transfers,100 edits of one in-flight message, disabled-source completion, same-content reposts, tray replay regression, battery changes, old text-only firmware, notification test and native CJK/icon raster checks.
- Android lint: zero errors; same APK signer; existing warnings remain.
- Cortex-M4 source execution at O0/Os/Oz: interpolation, visual peak/color, startup exclusion and session lifecycle. Actual draw-event regression additionally covers multiple unrendered model updates.
- Actual ARM notification receiver: CRC, RLE decode, buffer immutability, header-only updates, body/reply/call preservation and bounded allocation. Companion command tests pass.
- 14 trial boot and98 updater/rollback ARM scenarios pass.
- Full LVGL/EVE dark/light GPS/page cross-fades remain within 8192-byte display-list capacity; final frame count and maximum are in gps_frames-arm.json. This is simulated physical I/O, not measured LCD/FPS.
- Release/Debug regenerate/link/address/resource/memory checks pass. Final budget numbers and APK/ZIP hashes are in verification-summary.json. FreeRTOS heap, stacks, communication queues and CCM policy are unchanged.
- Existing APK certificate and old/new production ZIP importer checked. Bootstrap, Gate, stock, resources, Uninstall and Diagnostic payloads remain identical to6.11.0; only Product and APK change.

No hardware, ST-LINK, actual S24 Bluetooth, physical power-cycle or optical display test was performed for this patch. The connected target was not flashed. Public release contains APK/package/docs/verification only, not private source. Existing remote ODO documentation edits were preserved.
