# AK 550 Noodoe firmware and OQC audit

Date: 2026-08-31 KST

This note separates live server results, static APK evidence, and remaining
hypotheses. No firmware or OQC command was sent to the motorcycle during this
audit.

## Executive result

For the device tuple reported by the test AK 550, the still-running production
OTA service returned the following stable pair:

- Firmware: `s1_firmware_v5.16` (`5.16`)
- Resource: `s4_resource_v5.14` (`5.14`)
- Firmware series: `1`
- Resource series: `4`

This is the current stable target returned for this exact AK/Sunray tuple on
2026-08-31. It is not evidence that 5.16 is the newest firmware for every Noodoe
model or hardware generation.

The vehicle already reported firmware `5.16` and resource `5.14`. Nevertheless,
the server response said `Firmware update found`. A replacement app must compare
the returned version and hardware/series identifiers itself; it must not treat
the response message as permission to flash.

## Live OTA query

The official APK defines its OTA base URL as `http://nars.noodoe.com/` and uses
the v3 endpoints for this generation.

The live device-information payload was decoded as:

| Field | Value |
| --- | ---: |
| language packet | 2 |
| motor series | 1 |
| resource ID | 1 |
| default dashboard ID | 1 |
| firmware | 5.16 |
| resource | 5.14 |
| protocol | 0.0 |
| hardware | 0 |

`POST /v3/querySeries` request:

```json
{
  "language_packet": 2,
  "motor_series": 1,
  "resource_id": 1,
  "default_dashboard_id": 1
}
```

Response:

```json
{
  "result": "OK",
  "firmware_series_id": 1,
  "resource_series_id": 4,
  "original_resource_series_id": 4
}
```

`POST /v3/queryFirmware` request used channel `stable`, app version `1`,
firmware series `1`, current firmware `5.16`, and resource series `4`.

Relevant response fields:

```json
{
  "result": "OK",
  "message": "Firmware update found",
  "firmware": {
    "downloadUrl": "https://sunray-cdn.noodoe.com/firmwares/1657088080998-s1-SR1.5_ota_V516.bin",
    "firmware_series_id": 1,
    "majorVersionCode": 5,
    "minorVersionCode": 16,
    "versionName": "s1_firmware_v5.16",
    "channel": "stable",
    "resourceMajorVersionCode": 5,
    "resourceMinorVersionCode": 14,
    "resource_series_id": 4
  }
}
```

`POST /v3/queryResource` with both current and original resource series `4`
and version `5.14` returned the same common Pack B resource for both slots:

```json
{
  "result": "OK",
  "message": "Resource found",
  "resource": {
    "resource": {
      "downloadUrl": "https://sunray-cdn.noodoe.com/resources/1526957902822-s4-resource_common_Pack_B_v5.14.zip",
      "majorVersionCode": 5,
      "minorVersionCode": 14,
      "resource_series_id": 4,
      "versionName": "s4_resource_v5.14"
    }
  }
}
```

## Preserved files

The exact server files were downloaded without transmitting them to the meter.

| File | Size | SHA-256 |
| --- | ---: | --- |
| `1657088080998-s1-SR1.5_ota_V516.bin` | 458748 bytes | `3B64673054CA84B9CF504F4CC7EBCB2E6615B9FBF59DF79A37A92DCF14F037CA` |
| `1526957902822-s4-resource_common_Pack_B_v5.14.zip` | 2327968 bytes | `E36BE99A28614781951B2624390E671890B007628D30451BB2263CFE441B692E` |

The resource archive contains 1043 entries. Its `resource_config.json` identifies
resource `5.14`, `ResourceID` 1, `LangpackID` 2, and a per-file path/MD5 manifest.
It contains default dashboard, clock, speedometer, notification, navigation, and
weather assets. This package is therefore relevant beyond firmware recovery: it
is a primary format reference for the replacement app.

The firmware begins with values consistent with a Cortex-M vector table, but the
MCU, image base, signature, and trailer/checksum format are not yet proven.

## Modern firmware installation path

The documented user-facing procedure in the official app is:

1. Keep the scooter on and Bluetooth connected.
2. Open Noodoe and enter Profile, Settings, Scooter information.
3. Select Firmware update and Download now.
4. Wait for the download and Bluetooth transfer to finish.
5. Switch the scooter off, then do not switch it on again until the meter-side
   installation has completed.

KYMCO also published a separate Android `Special firmware update` video labelled
for firmware `0.75`. It should be treated as a version-specific recovery path,
not as the normal modern update or a general OQC entry procedure.

For Sunray 1.5, firmware update is integrated into the same install/file-transfer
state machine used for resources and user content:

1. The app downloads the binary URL returned by the OTA service.
2. `BTOtaApiHandler` adds the file at `LOCATION_ID.FIRMWARE` (`0x0800`).
3. `SunrayTransferTaskManager.startInstallTask()` starts a firmware install task.
4. The transfer negotiates file metadata, total size, content ID, file IDs, and
   MD5 values, then sends file blocks and transfer-control messages.
5. Completion of the firmware location transaction is the observed application
   path into installation.

Known modern command IDs include:

| Command | ID |
| --- | ---: |
| file-transfer negotiate | `0x0A` |
| file-transfer control | `0x0B` |
| file-transfer data | `0x0D` |
| basic firmware upgrade enum | `0x10` |

Although `BASIC_FIRMWARE_UPGRADE` exists in the enum, the decompiled outgoing
processor does not expose it as the normal modern update path. Sending a lone
`0x10` command is therefore not justified. The remaining unknown is the exact
meter-side transition after a completed firmware install; capture one official
update session or recover the exact handler bytecode before implementing writes.

## Test and OQC mode

The modern 1.5 command set exposes:

| Operation | Command | Notes |
| --- | ---: | --- |
| OQC data read | `0x11` READ | Read production/configuration block |
| OQC data write | `0x11` WRITE | Potentially destructive; do not expose |
| OQC test state | `0x12` WRITE | `START=1`, `STOP=0` |
| OQC result notification | `0xC4` NOTIFY | Five-byte result payload |

There is also a legacy raw `ENTER_OQC_MODE` path (`0x31` request, `0xB1` reply)
and legacy scooter-information commands. These belong to the older protocol and
must not be mixed with framed Sunray 1.5 commands.

No confirmed dashboard-button sequence for entering OQC mode was found. In the
modern app path, OQC entry is a protocol action: the connected service app sends
`0x12` with state `START`. The documented Up plus orange-button power-on sequence
is a hardware reset/restart procedure, not evidence of OQC or bootloader entry.

The replacement app should initially expose only OQC read. OQC START/STOP belongs
behind an explicit developer guard and must issue STOP on normal exit, timeout,
disconnect, and exception cleanup. OQC write should remain unavailable until
the complete field map and recovery procedure are proven.

## SPP versus BLE correction

The active Sunray connection path constructs `SunraySPPHandler`, creates an
RFCOMM socket with `createRfcommSocketToServiceRecord()`, and reads/writes its
streams. The same SPP handler forwards old-device input into classes whose names
contain `Gatt` or `Ble`.

The APK contains a generic GATT service and GATT connection helpers, but the
observed Noodoe/Sunray dashboard path does not establish that old dashboards use
a BLE radio transport. The strongest current model is:

- Modern device: Classic Bluetooth SPP plus Sunray 1.5 framed protocol.
- Old device: Classic Bluetooth SPP plus legacy parser/OTA state machine.
- Actual BLE/GATT transport for the old dashboard: not proven.

Therefore dropping old support avoids the legacy protocol and OTA state machine;
it does not presently save a proven BLE implementation.

## Version-gate policy

A modern-only replacement app cannot upgrade a device that it refuses to speak
to. The safe first release policy is:

1. Pair and open SPP.
2. Run only the bootstrap/device-information exchange.
3. If the device selects Sunray 1.5, compare the exact firmware, resource,
   hardware, language, and series tuple before enabling features.
4. If it selects the legacy protocol, block feature use and direct the owner to
   the official KYMCO Noodoe updater or an authorized dealer while those remain
   available.
5. Keep firmware flashing out of the main app until an official update capture,
   power-loss recovery behavior, and image validation rules are known.

If the preservation project must update unsupported old units after the official
service disappears, it needs a separate recovery utility implementing the
minimum legacy SPP OTA state machine. This is much smaller than implementing all
legacy Noodoe features, but it cannot be skipped entirely.

## Next validation

1. Preserve all OTA responses, firmware variants, and resource packs reachable
   for known model tuples before the service closes.
2. Capture one official firmware installation with Bluetooth HCI snoop and app
   logs, ideally on a sacrificial or recoverable meter rather than the primary
   vehicle.
3. Confirm the bootstrap version discriminator on at least one genuinely old
   dashboard.
4. Add a read-only version gate and OQC-data reader to OpenNoodoe.
5. Implement modern firmware writing only after checksums, acknowledgements,
   finalization, reboot, rollback, and low-voltage behavior are documented.

## Static source anchors

- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/ServerConstant.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/ota/OTAInterface.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/connection/SunrayBTConnection.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/connection/handler/spp/SunraySPPHandler.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/SunrayTransferTaskManager.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/SunrayOqcCommandHandler.java`
- `analysis/jadx/com.noodoe.sunray_2.1.14/sources/com/noodoe/sunray/cmu/service/task/utils/Sunray_1_5_Commands.java`

## Official operating references

- `https://www.kymco.es/themes/kymco/assets/img/temp/noodoe/manualusuario-noodoe-24681.pdf`
- `https://sites.google.com/view/kymconoodoe30-en/video`
- `https://sites.google.com/view/kymconoodoe/問與答`
