# 0.9.71 · Ride Summary, retained settings and oil arc

Ride Summary keeps its original darkened photo and edge shading throughout the summary and exit. Only the following ignition-off photo standby and approach welcome light show the photo without shading. Ignition ON restores the usual page contrast. Fuel-warning shading, photos, clock/ODO layout and backlight/power timing remain unchanged.

The oil remaining arc keeps its existing green above30%, switches directly to yellow at30% or below, and to red at15% or below. At5% or below red alternates on/off every0.5seconds. At zero, the empty track flashes red. There is no colour interpolation. Unknown remaining oil shows the neutral track, not a false zero warning. This is maintenance remaining, not a measured oil level.

## Preserving preferences and maintenance across CFW updates

User preferences and brightness live in the external NOR file **CFWCFG.DAT (128KiB)**. Oil/belt/service reset references, trip records and accumulated ignition time live in **CFWRIDE.DAT (256KiB)**. These are audited files separate from the firmware slots; their physical addresses depend on the device FAT allocation. Updates do not format them or change their schema. Existing values are read during the candidate trial and retained after confirmation.

Starting with this firmware, routine CFW updates require a fresh UI snapshot and physically committed configuration and ride journals **before BEGIN and again before COMMIT**. An accepted save runs before an updater waiting on it. Readback/commit-marker completion is required; a save error or10-second timeout rejects installation before activation. The existing phone-side, checksummed settings/name snapshot remains an additional recovery copy. Maintenance references are retained as data; service-reset actions are never replayed.

The new device-side barriers take effect once0.9.71 is running, for updates started from0.9.71 onward. Installing0.9.71 from an older version uses that older sender firmware's save behavior plus the existing NOR journals and phone settings backup. No software can guarantee recovery from a physically failed NOR or an explicitly erased app/backup; errors remain visible instead of silently resetting values.

Install the **0.9.71 APK over the existing app without clearing its data**, then use the matching **0.9.71 ZIP**. Keep the phone app data through final reconnection. First installation remains for F4 stock5.14/5.16, without a model/PCBA/bootloader-version whitelist; hardware safety/identity checks remain. Follow manual Bootstrap forget/re-pair instructions. Stage6/4 may still request **UP + O together for3seconds**; reconnect afterward to verify installation and complete any pending settings restoration.

Software verification covers actual ARM rendering commands, persistence/reboot and rejected-save paths, updater/scheduler integration, Debug/Release builds and APK/ZIP binding. No physical LCD, NOR, phone or vehicle test.

[Installation and recovery](../Installation/README.en.md) · [Settings backup recovery](31-Settings-Catalog.en.md)
