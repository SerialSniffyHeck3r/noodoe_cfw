# 2026-08-30 AK550 Repair 001

## User Action

The phone-side Noodoe connection was removed and the phone was reconnected to Noodoe from the beginning.

## Captured Files

- `captures/logcat/2026-08-30-ak550-repair-001-after-reconnect.log`
- `captures/hci/2026-08-30-ak550-repair-001-after-reconnect.zip`
- `evidence/bluetooth/2026-08-30-ak550-repair-001-dumpsys-bluetooth_manager-after-reconnect.txt`
- `evidence/bluetooth/2026-08-30-ak550-repair-001-btsnoop-setting.txt`
- `analysis/2026-08-30-ak550-repair-001/bluetooth-kymco-lines.txt`
- `analysis/2026-08-30-ak550-repair-001/logcat-kymco-bluetooth-lines.txt`
- `analysis/2026-08-30-ak550-repair-001/logcat-pairing-connection-events.txt`
- `analysis/2026-08-30-ak550-repair-001/dexdump-callsite-hits.txt`

## HCI Snoop Status

The phone setting `global bluetooth_btsnoop_log_mode` was `full`, but the Samsung/Android 16 bugreport did not include a visible `btsnoop_hci.log`.

Checked accessible locations:

- `/sdcard/btsnoop_hci.log`: not found
- `/sdcard/Download/btsnoop_hci.log`: not found
- `/sdcard/Android/data/com.android.bluetooth/files/btsnoop_hci.log`: not found
- `/data/misc/bluetooth/logs/btsnoop_hci.log`: permission denied to non-root shell

Conclusion: a rooted phone or a vendor-specific logging path may be needed for raw HCI snoop extraction on this S24 Ultra.

## Bluetooth Manager Findings

The Bluetooth manager dump contains Noodoe-related devices:

- `KYMCO Noodoe [device suffix]`
  - Transport/classification: `BR_EDR` in the shim record
  - UUIDs: `00000000-deca-fade-deca-deafdecacaff`, `SPP`
  - Bond type: persistent
- `KY AK550`
  - Transport/classification: `LE`
  - User identified this as the AK550 smart key, not the Noodoe infotainment endpoint
- `AK550_INFOTAINMENT`
  - Transport/classification: `DUAL`
  - UUIDs include audio/phone profiles and custom UUIDs, likely a separate infotainment/audio endpoint

Strong working conclusion: the Noodoe endpoint for this AK550 is not BLE-only. The main Noodoe control/data channel is very likely Bluetooth Classic SPP/RFCOMM. The visible `KY AK550` LE device should be excluded from Noodoe analysis unless later evidence links it to the cluster.

## APK Static Findings

The official Noodoe app package backed up from the phone is `com.noodoe.sunray` version `2.1.14`.

`dexdump` found:

- `com.noodoe.fwk.ble.v02.service.bt.uuid.BTServices$SPP`
  - `SERVICE = 00001101-0000-1000-8000-00805F9B34FB`
  - `SERVER_SERVICE = 00006BFF-0000-1000-8000-00805F9B34FB`
- `com.noodoe.fwk.ble.v02.service.connection.handler.SPPHandler.onCreateSocket()`
  - Calls `BluetoothDevice.createRfcommSocketToServiceRecord(UUID)`
- `com.noodoe.fwk.ble.v02.service.connection.NDBTService$GattMainHandler`
  - Uses `connectGatt` with an explicit transport argument

Conclusion: the app contains both a GATT path and an SPP/RFCOMM path. The naming uses `ble` for the framework package, but SPP is explicitly implemented inside it.

## Current Hypothesis

Noodoe likely uses a hybrid Bluetooth design:

1. LE/BLE may be used for discovery, identification, or side-channel behavior.
2. Bluetooth Classic SPP/RFCOMM is likely used for the primary Noodoe session or at least some major data transfer operations.
3. The six-digit pairing code is consistent with Classic Secure Simple Pairing, but it is not proof by itself.

## Next Capture

Do one feature at a time while the phone remains paired:

1. Clock sync only
2. Dashboard change only
3. Small gallery/photo transfer only
4. Single phone notification only
5. Weather refresh only
6. Navigation route/guidance only

For the next capture, prefer a rooted or older test phone if available, because it may allow direct access to `btsnoop_hci.log`.
