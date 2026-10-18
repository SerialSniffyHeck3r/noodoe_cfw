# NewDoe 0.11.10 — phone dashboard

Swipe between **Device · Home · Personalize**. Start / Pause at the top controls the connection. The original installation wizard and confirmations remain on Device.

Home has three separate cards: **map with speed/ODO**, **trip computer**, and **maintenance**. Speed and ODO use OpenNoodoe’s bold system monospace font, with dim leading zeros (three/six digits). Average uses the Ø icon and maximum uses an upward arrow. Swipe the trip card or use the arrows to switch A, B, TODAY and THIS SESSION. Average includes stopped time, matching the device.

Oil has a large gauge on the left; belt and service have separate pill bars and remaining distance on the right. Oil means the remaining service interval, not a measurement of physical oil quantity. Earliest enabled distance/hour/day limit controls each bar. Unknown data remains unknown and disconnected counters display their last receipt time. Service reminders still come from Android, using Noodoe’s counters.

**Stock KYMCO:** the trip and maintenance sections are replaced by original vehicle information. Speed, ODO, stop duration and maximum speed use stock read commands, followed by model/firmware/series/PCBA information. CFW-only counters and maintenance reminders are not invented. If the app has not identified the unit yet, use Device → Check current device first, then Start. Stock dashboard mode only reads; it does not start an installation or send CFW settings.

**Correct a service counter:** on Noodoe open Main menu → Maintenance → Oil / Belt / Service → Current distance. Enter kilometres travelled since that service. The phone exposes the same fields under Personalize → Current device settings. Total ODO, other services, hour and date baselines remain unchanged. A valid current ODO and restored ride state are required, and the value cannot exceed ODO. Save completion uses the existing NOR ride checkpoint; no new storage partition is introduced. Ordinary updates preserve it. With older language resources the new “Current distance” menu label can fall back to English.

**UI test screen:** first pause the connection and finish or stop any installation task. Open Personalize → UI test screen. Its permanent banner marks all data as simulated. Choose CFW, stock KYMCO, no data or disconnected presentation. “Edit test values” changes speed, ODO, trips, average/maximum, service percentages, map coordinates and units. Swiping still changes the trip card. These values are local to this screen and never sent over Bluetooth, used for service alerts or written into riding records. Leaving discards them; starting a real connection/work closes the preview. It cannot be used while connected. Map previews use the selected offline map without starting GPS.

The map uses the existing file and location feed; rendering stops while hidden. Counter schema 2 adds existing moving/stopped times and peaks to the read-only snapshot (256 bytes including result), at the unchanged 2/5/10-second cadence. Android still accepts old schema-1 cached counters.

Install the matching **0.11.10 APK and CFW ZIP**. 0.11.4–0.11.9 use normal same-Gate CFW update. Gate, Bootstrap, resident bootloader and language archives are unchanged; working fonts do not need retransmission. Older mismatched-Gate devices retain the installer’s stock-return/reinstall route. The previously accepted old-code stock-return power-interruption limitation remains.

Software validation is separate from physical Noodoe/RF/vehicle qualification. See the release validation record for actual test and phone installation results.
