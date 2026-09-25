# 6.11.2 implementation and evidence

## Notification delivery and reply

`NotificationHistory` resolves freeform RemoteInput in ordinary and WearableExtender actions, preferring semantic reply when available. `NotificationDelivery` retains the exact displayed entry while newer incoming history remains bounded. A reply validates current grant, live notification key, content and PendingIntent/input keys; an old card cannot reply to a replaced or removed message. The Android native renderer produces larger A4 text at27/36/42/30px with an independent24px received timestamp. Native glyphs are not upscaled. General empty-state text uses the largest already available32px Lato face (previous24px). Reply rows use26px. Current UI uses288x144 or256x162 packed masks,20,736B per bank, capability0x40000; old devices retain the legacy layout.

Timestamp is the Android notification post time in the phone's local time zone, HH:mm. Notification samples contain synthetic content. The sample icon falls back to an envelope because Discord is not installed in the test runtime; actual source small icon/application icon loading uses the notification package context.

`Notification_Preview` owns the temporary page switch and restores only page/selection, not trip/fuel state. It observes new revisions even while disabled, ignores existing history on reconnect, coalesces consecutive arrivals, and cancels on accepted manual input. Calls, settings, warnings and IGN policy retain priority. Stable settings fields0x1070/0x1071 use the existing fixed-file journal and phone settings protocol. Default disabled, duration5s bounded3–30s.

## Update presentation

`MaintenanceService` captures and delivers terminal progress before handing the service to automatic riding. The completed immutable snapshot survives Activity rebind until device selection/reset clears it. Epoch fencing still rejects old callbacks. The test supplies device7/8 progress, completes between periodic ticker updates and verifies8/8 before and after rebind. This is a presentation fix; durable candidate verification, journal, health interval and rollback are unchanged.

## Frame integrity

Existing full-shell stress reached8,092/8,192B. An adversarial overlapping GPS-page fixture reached8,796B. The latter is a command-budget reproduction, not a recorded real vehicle frame. Repeated per-line contexts were the dominant cost. Project-owned `product_gps_line_batch.c` emits the same clipped solid LINE_STRIP segments with one context per style; all48 route samples and fade bands remain. Vendor code, geometry/detail, photo quality and GPU memory boundaries are unchanged.

The project EVE adapter waits for physical DLSWAP completion before starting a new list and validates completed REG_CMD_DL before CMD_SWAP. Unexpected oversize/fault stops publication through the existing graphics error path. This protects publication but is not a claim that arbitrary hardware faults can never disturb a panel.

Current visible phone panel bytes are immutable. A changed complete header/card revision is composed in the other page bank only after its physical scanout retirement. The probe blocks the fence and confirms zero new texture writes, then releases it and confirms the new revision appears. Injected oversize and stuck swap requests publish no new list. No live SPI or actual scanout timing is exercised by this mock-I/O test.

## Memory and fault contract

Two page banks grow by4,608B in GPU cache total and bounded phone-panel storage grows in SDRAM. No RTOS heap, task stack, queue, CCM allocation or memory budget is reduced. Actual all supported ordinary glyphs, used icons and two page-bank allocator test remains below114,688B; unused removed OBD/remote masks and former raster fuel warning are not runtime cache consumers.160px digits reuse the page bank.

The embedded `__assert_func` now records file/function/expression/line for SWD and invokes existing `BSP_FaultRecord` with a retained fault reason. It does not invoke an unconnected stderr/abort path or pull buffered stdio into flash. It still stops normal execution and watchdog feeding through the established recovery hook. SRAM pointer evidence is not claimed to survive reset; the existing retained fault record does. Actual Release/Debug ELF routing is executed in ARM emulation. Product flash reserves remain above64KiB Release and32KiB Debug.

## Verification limits

See `verification-summary.json`, render guard and ARM result files, Android lint/test logs, packaged ELF memory reports, source hashes and changes.diff. Native Android PNG previews are under `phone-preview`; they are not real LCD photographs. No ST-LINK, bench flash, vehicle, physical reset, real Android telecom/Discord or RF session was accessed in this turn. Runtime FPS and phone background policy need real-device confirmation. The optional question about Discord's available Reply action was not answered; unsupported notifications are not represented as universally replyable.
