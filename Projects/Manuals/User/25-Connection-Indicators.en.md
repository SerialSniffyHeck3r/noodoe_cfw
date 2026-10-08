# 0.9.63 · Notification indicators and automatic reconnection

The top phone icon alternates red/inactive every 250ms for four flashes over two seconds. Notifications arriving during that interval are still stored and displayed, but neither extend the timer nor queue another flash sequence. The green music-playing indication is unchanged.

GPS stays gray without a valid fresh position. Fresh fixes retain the existing 120ms gray pulse.

Automatic riding keeps waiting when the vehicle is absent; retry delay is now capped at ten seconds. Connecting and identifying the device take additional time. A bounded 35-second CPU wake lease covers connection/initialization with the screen off and is released during retry waits. Socket timeouts and temporary UI startup backpressure can reconnect. Permission rejection, manual stop and unresolved updates remain respected. A location listener that previously worked and then stops delivering for 90 seconds is registered again, without relabeling old coordinates as fresh.

For background return, check companion-device association, Nearby devices permission and location Allow all the time. Closing the app screen is not manual disconnect. Android force-stop and manufacturer background restrictions cannot be overridden by this feature.

First-install admission with the matching 0.9.63 APK and ZIP accepts **stock 5.14 or 5.16**, without HW, bootloader-version, model or PCBA whitelists. Successful protocol replies, stationary IGN, original capture, exact device/image and storage checks remain. **Stock recovery uses the bundled 5.16 image even when installing from 5.14.** Older ZIPs retain their older policy; import/select the new ZIP too.

The developer reports stable physical operation of 0.9.62, with a manual reset still needed at Stage 6/4. That reset issue is not claimed fixed. When instructed, hold UP+O together for three seconds and wait for the app to reconcile the result. Software checks of 0.9.63 do not establish wireless installation or background-return behavior on every vehicle.
