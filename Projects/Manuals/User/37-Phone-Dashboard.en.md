# NewDoe 0.11.9 — phone dashboard

Swipe horizontally between **Device · Home · Personalize**. The app opens on Home. The top **Start / Pause** control starts or pauses the existing riding connection. There is no separate riding connection page. Pausing does not reset any Noodoe counters.

Home has a flat instrument display containing speed, total ODO, the selected offline map, trip readouts and maintenance bars. The position marker uses the existing phone location feed; it is hidden when the fix is over 30 seconds old. Choose or import a map using the Map button or Personalize → Riding & maps. No additional location subscription is started for this dashboard. The map renderer stops when Home is hidden.

Swipe the trip readout, or use the arrow buttons below the display, to see **TRIP A, TRIP B, TODAY and THIS SESSION**. A/B/TODAY come from Noodoe’s existing trip computer; the app does not integrate phone GPS speed into substitute mileage. The session distance uses Noodoe’s existing current-ignition-session value. After disconnecting, the last received counters remain visible with a timestamp. Current speed becomes unknown rather than displaying an old speed as live. An incomplete trip remains marked incomplete.

The **Oil, Belt and Service** rows show distance since the corresponding service baseline and remaining service interval. Oil does **not** measure physical oil quantity. The phone considers all enabled distance, ignition-on-hours and calendar-day limits, using the earliest limit for the bar. Missing or backward counters remain unknown. When a configured limit is reached, Android posts a service reminder if notifications are permitted. Repeated packets and reconnecting do not repeat the same reminder. Confirming service through the existing device settings establishes the next baseline.

Detailed counters use a 196-byte read-only response on the existing Bluetooth connection: every 2 seconds on Home, 5 seconds during visual-resource traffic, and 10 seconds while Home is hidden. Speed reuses the existing status feed. Missed intervals do not build a queue or replay later. This feature adds no device accumulator, timer, alert policy or storage writes.

Device contains the existing installation wizard, progress, language resources, connection help and recovery tools. Personalize groups app language/theme, device settings, riding/maps, phone/notifications and wallpapers. A filled button with a check mark indicates an enabled toggle. Installation approvals, trial-boot confirmation, resume and pairing behavior are unchanged.

Install the matching **0.11.9 APK and firmware ZIP** to receive the new counters. Existing 0.11.4–0.11.8 installations use the normal same-Gate CFW update. Gate, Bootstrap and language resources are unchanged; working language resources need no resend. Older mismatched-Gate devices retain the installer’s existing stock-return/reinstall route. The accepted limitation in already-installed old stock-return code still applies: a power cut there is not guaranteed to recover automatically.

Validation is software-based. Wireless APK installation and physical Noodoe/radio operation are recorded separately in the release validation notes.
