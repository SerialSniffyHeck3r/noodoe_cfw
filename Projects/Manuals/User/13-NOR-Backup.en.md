# External NOR backup and file browser

Available in0.9.18: **Customize → NOR file browser / backup** (the new controls are labelled in Korean). Install the matching APK and Product update, then keep riding linkage connected.

## Extract individual files

Read the FAT/file list, enter a directory, select a file and choose Back up. The FONT shortcut locates the stock font directory. IGN may be on or off; normal music, GPS and notifications take priority. The connection service continues when this dialog closes or the phone screen turns off. Cancelling an individual file does not reboot Noodoe. Normal device sleep/disconnection or Android force-stop can interrupt it.

The output contains the logical `file.bin`, physical `clusters.raw` including allocation slack, both FAT copies, directory evidence and a manifest recording UID, original path, cluster numbers/addresses, byte order and hashes. Content is compared in two passes; allocation metadata and the saved file are checked too. A changing live journal may fail this check. Such a failure remains incomplete.

This browser uses FAT short names and is strictly read-only. It will not format, repair, delete or overwrite anything. Cross-linked, cyclic or damaged chains are reported. Use a full raw snapshot to preserve damaged filesystem evidence.

## Copy the full128MiB external NOR

Start only with **IGN OFF**, a confirmed idle Product and at least144MiB of free phone storage, plus room for export. Choose Full NOR backup, then Start / verified resume. This is a lengthy backup; vehicle transfer time has not been measured.

The device finishes saved settings/ride data, drains writes and freezes NOR writers. Normal companion content pauses and a dedicated screen shows stage and copied bytes. The phone displays percentage and throughput, syncs partial data and verifies saved-file readback plus the device SHA-256. On success, `nor.raw` and the verification manifest are retained and Noodoe restarts.

Release O, then hold it freshly for3seconds to cancel, or use the app's cancel button. PH9 does not block this. IGN ON/unknown,60seconds without contact, five minutes without progress, preparation exceeding30seconds or a read failure also end at a safe storage boundary followed by a system reset. Existing watchdog behavior is unchanged.

A reconnect to the same frozen session checks the saved prefix hash and resumes. After a device reboot, verification starts at byte0 against a new snapshot. Incomplete data remains `.partial`; it is never labelled complete or silently combined with a different boot's snapshot.

Export the latest backup/evidence ZIP through Android's document picker. Internal originals remain. This includes **external NOR only**, not the MCU's internal bootloader/factory flash. Raw physical byte order can differ from logical extracted files; keep the manifest with the dump.

## Validation

Software tests cover128MiB transfer, interruption/resume/hashes, FAT damage, cancellation, IGN and deadlines. Screen previews use actual firmware LVGL/EVE commands in software. Real vehicle RF throughput and power consumption were not measured.


## Software simulation captures

![NOR backup — EVE software simulation](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.9.18-nor-backup/NOR-Backup-EVE-simulation.png)

![Android browser — software simulation](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v0.9.18-nor-backup/NOR-Browser-Android-simulation.png)
