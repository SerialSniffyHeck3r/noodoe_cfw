# USB CDC retired

2026-09-21: no USB device/CDC implementation is compiled in any CFW profile.
The remaining module manifest selects **zero** sources in the preserved vendor
USB_DEVICE tree, so Cube synchronization cannot accidentally restore CDC.
IOC disables OTG_HS and leaves PB14/PB15 analog; the user regenerates Core.

NOR diagnostics/backup/import use StorageSWD and the existing verified-file
tools. `g_storage_backup` keeps its original read-only diagnostic layout with
reserved CDC counters zero; it no longer owns a stream or USB command parser.
Historical PC backup verification remains available for existing evidence and
updater authorization. This does not change the stock APP's USB implementation.
