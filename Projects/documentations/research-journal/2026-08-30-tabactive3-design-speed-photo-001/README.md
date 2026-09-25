# 2026-08-30 Tab Active3 Noodoe design/speedometer/photo session

## User action sequence

The user reported this sequence on the rooted Samsung Galaxy Tab Active3:

1. Connect to Noodoe.
2. Send dashboard/instrument-cluster design.
3. Send speedometer design.
4. Send photo.

## Captured artifacts

- `captures/logcat/2026-08-30-tabactive3-design-speed-photo-001.log`
- `captures/hci/2026-08-30-tabactive3-design-speed-photo-001-bugreport.zip`
- `evidence/bluetooth/2026-08-30-tabactive3-design-speed-photo-001-dumpsys-bluetooth_manager.txt`
- `evidence/bluetooth/2026-08-30-tabactive3-design-speed-photo-001/btsnooz_hci_2026-08-30-tabactive3-design-speed-photo-001.log`
- `evidence/bluetooth/2026-08-30-tabactive3-design-speed-photo-001/bugreport-btsnooz_hci.log`
- `evidence/bluetooth/2026-08-30-tabactive3-design-speed-photo-001/bugreport-btsnooz_hci.log.last`
- `evidence/appdata/com.noodoe.sunray-data-2026-08-30-tabactive3-design-speed-photo-001.tgz`
- `evidence/bluetooth/private/bluetooth-private-2026-08-30-tabactive3-design-speed-photo-001.tgz`
- `analysis/2026-08-30-tabactive3-design-speed-photo-001/logcat-filtered.txt`
- `analysis/2026-08-30-tabactive3-design-speed-photo-001/bluetooth-dump-filtered.txt`
- `analysis/2026-08-30-tabactive3-design-speed-photo-001/noodoe-classic-timeline.txt`
- `analysis/2026-08-30-tabactive3-design-speed-photo-001/ble-scan-filter-candidates.txt`

The private Bluetooth archive can contain pairing/link-key material. Keep it local and do not publish it.

## Device/app state

- Device: Samsung Galaxy Tab Active3, `SM_T575N`, product `gtactive3kx`.
- Android: 13 / SDK 33.
- Root: available through Magisk `su`.
- Installed Noodoe package found on tablet: `com.noodoe.sunray`.
- Version observed by `dumpsys package`: `versionName=2.1.14`, `versionCode=579`, Play Store installer.
- `noodoe.com.navigations` / Noodoe Tools was not found in the package list for this tablet session.

## Confirmed Bluetooth facts

The Noodoe endpoint in this session was:

- Name: `KYMCO Noodoe [device suffix]`
- Address: `98:07:2D:XX:XX:XX`
- Type in Android Bluetooth dump: `[ DUAL ]`
- UUIDs:
  - `00001101-0000-1000-8000-00805f9b34fb` (Serial Port Profile / RFCOMM)
  - `00000000-deca-fade-deca-deafdecacaff`

The Bluetooth manager dump shows `com.noodoe.sunray` opening a classic Bluetooth socket to the SPP UUID:

- `2026-08-30 04:07:18.206`: `BluetoothSocket -- connect()` for `98072D_6` / `00001101-0000-1000-8000-00805f9b34fb`, called by PID `32579 @ com.noodoe.sunray`.
- `2026-08-30 04:07:19.137`: `BluetoothSocket` connected to the same SPP UUID.

This strongly confirms that the active Noodoe data/control path is Bluetooth Classic RFCOMM/SPP, at least for this pairing/session.

The classic link timeline from the same dump:

- `2026-08-30 04:07:10.956`: classic ACL connection successful, local initiated.
- `2026-08-30 04:07:16.903`: disconnected, remote user terminated connection.
- `2026-08-30 04:07:17.368`: classic ACL connection successful again, local initiated.
- `2026-08-30 04:09:19.439`: disconnected, connection timeout.

The btsnooz file also contains only command/event level material for the same classic handle/address, not RFCOMM payload frames.

## BLE scan candidates

The dump also contains BLE scan filters active through Samsung Beacon Manager (`com.samsung.android.beaconmanager`, PID `12543`), including:

- Service UUID `0000fd6c-0000-1000-8000-00805f9b34fb`
- Service UUID `0000fd59-0000-1000-8000-00805f9b34fb`
- Service data UUID `0000fff6-0000-1000-8000-00805f9b34fb`
- Service data UUID `0000fddb-0000-1000-8000-00805f9b34fb`
- Manufacturer ID `75`
- Manufacturer ID `7500`

These are BLE discovery/nearby-beacon candidates. They are not yet proven to be Noodoe payload transport. Treat them as discovery evidence until a capture ties them directly to `com.noodoe.sunray` or the Noodoe module.

## Logging caveat

Before this user action sequence, Android Bluetooth HCI snoop was not active:

- `mSnoopLogSettingAtEnable = empty`
- `mDefaultSnoopLogSettingAtEnable = empty`

The setting was changed afterward:

- `settings put global bluetooth_btsnoop_log_mode full`
- `settings get global bluetooth_btsnoop_log_mode` returned `full`

However, Android applies this when the Bluetooth stack starts. Bluetooth must be toggled off/on or the tablet rebooted before the next experiment if full HCI payload logging is desired.

## Next capture plan

Do not repeat a combined sequence as the next step. Restart Bluetooth logging first, then capture one action per run:

1. Connection only.
2. Dashboard/instrument-cluster design only.
3. Speedometer design only.
4. Photo transfer only.
5. Clock sync only.
6. Notification injection only.

For protocol recovery, Frida hooks on `BluetoothSocket` input/output streams will likely be more valuable than passive RF sniffing. HCI snoop is still useful as a second independent record.
