# KYMCO Noodoe hardware model

> 2026-09-01 update: 이 문서는 초기 APK/실차 관찰 모델이다. 해시가 고정된
> SR1.5 V5.16 firmware의 MCU, Bluetooth, display, NOR, bus, UART5-SPP 교차분석은
> `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`가 우선한다. 커스텀
> 펌웨어 bring-up용 주변장치 주소 증거표는
> `analysis/2026-09-01-custom-firmware-hardware-map/README.md`를 함께 본다.

Date: 2026-08-30 KST

## Confidence labels

- Confirmed: directly observed in local captures, APK code, or official public
  source.
- Strong inference: follows from multiple confirmed facts, but not directly
  measured on the motorcycle PCB.
- Hypothesis: plausible, must be tested.

## Known hardware entities

### Noodoe meter module

Status: Confirmed

Observed Bluetooth identity:

- Name: `KYMCO Noodoe [device suffix]`
- Address observed in captures: `98:07:2D:XX:XX:XX`
- Android device type: dual-mode Bluetooth in dumpsys evidence
- SDP/GATT-facing UUID evidence includes Classic SPP UUID
  `00001101-0000-1000-8000-00805F9B34FB`

Official certification reference:

- FCC ID: `2AM4E37130-LGC6`
- Applicant/manufacturer: Kwang Yang Motor Co., Ltd.
- Equipment class/name: Noodoe
- Frequency range listed by FCC ID index: `2402-2480 MHz`
- FCC listing: https://fccid.io/2AM4E37130-LGC6/Label/Label-Label-Location-3610675

Interpretation:

- The Noodoe module family is a 2.4 GHz Bluetooth-capable module.
- For the AK550 work so far, the app-layer meter data path is Classic Bluetooth
  SPP/RFCOMM.
- BLE/GATT code exists in the Android libraries and may be used for discovery,
  older protocol modes, "SPP LE", or firmware/DFU paths, but it is not the main
  confirmed data path for dashboard/photo/design transfer in current evidence.

### Smart key endpoint

Status: Confirmed as separate Bluetooth entity, function inferred from user
knowledge

Observed Bluetooth identity:

- Name: `KY AK550`
- Android device type: LE

Interpretation:

- Treat this as the motorcycle smart-key related endpoint unless evidence later
  links it to Noodoe payload transfer.
- Do not mix this with Noodoe meter captures.

### Infotainment/audio endpoint

Status: Confirmed as separate Bluetooth entity, role inferred from profile UUIDs

Observed Bluetooth identity:

- Name: `AK550_INFOTAINMENT`
- Android device type: dual-mode Bluetooth
- UUID/profile evidence included audio/phone style profiles and custom UUIDs.

Interpretation:

- This is likely separate from the Noodoe meter SPP endpoint.
- It may handle headset/audio/phone integration, not the dashboard asset
  transfer path.

## Hardware information retrievable from Noodoe

The test APK documents two important information blocks.

### DeviceInfo

Status: Confirmed in code, should be read from the AK550 next

The `DEVICE_INFO` command reply is parsed as at least 90 bytes and exposes:

| Field | Meaning |
| --- | --- |
| Firmware major/minor | Meter application firmware version |
| MAC address | Bluetooth MAC returned by the meter |
| Bootloader major/minor | Bootloader version |
| Resource major/minor | Installed resource version |
| Supported language bitfield | Up to 128 possible language flags; code defines 18 labels |
| Resource ID | Installed resource package identity |
| Language pack ID | Installed language pack identity |
| Model name | 10 ASCII bytes |
| Bike max speed | unsigned 16-bit value |
| Bike series number | 18 ASCII bytes |
| Bike type | 1 byte |
| PIN code | unsigned 16-bit value |
| Motor series | unsigned 16-bit value |
| PCBA hardware version | 6 ASCII bytes |
| Default dashboard ID | 1 byte |

The code also treats firmware lower than `4.0` as deprecated for the newer
Sunray 1.5 command path.

### OQC/production data

Status: Confirmed in code, riskier than DeviceInfo because write commands also
exist nearby

The `OQC_DATA_ACCESS_READ` reply is parsed as at least 147 bytes and exposes:

| Offset | Field |
| --- | --- |
| 10..25 | Part number, ASCII |
| 26..43 | Serial number, ASCII |
| 44..49 | PCBA hardware version, ASCII |
| 50..61 | MAC string, ASCII |
| 62..71 | Model, ASCII |
| 72..73 | PIN, unsigned 16-bit |
| 74..113 | Ten backlight thresholds, unsigned 32-bit each |
| 114..115 | Max speed, unsigned 16-bit |
| 116..117 | Language, unsigned 16-bit |
| 118..119 | Firmware major, unsigned 16-bit |
| 120..121 | Firmware minor, unsigned 16-bit |
| 122..137 | Assembly number, ASCII |
| 138..139 | Panel version, unsigned 16-bit |
| 140 | Unit |
| 141 | Type |
| 142 | Language pack |
| 143..144 | Motor series, unsigned 16-bit |
| 145 | Resource ID |
| 146 | Dashboard ID |

This is probably the richest single hardware identity read available without
opening the vehicle.

## Physical design inferences

Status: Strong inference

The Android app and test tool imply the meter has at least:

- A Bluetooth controller supporting BR/EDR SPP.
- A BLE-capable stack or mode, based on code and FCC evidence.
- Nonvolatile storage for dashboard/gallery/resource files.
- A bootloader or DFU/update mode.
- Installed resource and language packs.
- A production/OQC data area containing serial, model, MAC, PIN, and calibration
  values.
- A light sensor and a backlight control path.
- Physical button inputs: up, enter, down; plus power on/off state reporting in
  OQC mode.
- A dashboard runtime that can render speed, clock, weather, gallery image,
  navigation assets, notifications, and POI/group overlays.

## What could not be concluded from the initial evidence

- Exact MCU package or exact CC256x module/silicon variant.
- PCB layout and exact FT81x submodel.
- The firmware now confirms that the STM32 application owns the SPP command
  dispatcher and the FT81x display path, while a TI CC256x controller supplies
  the Bluetooth HCI transport.
- Whether the BLE "SPP LE" service is active on AK550 firmware.
- Whether OQC read is safe on every vehicle state.
- Whether firmware/resource update files are signed, encrypted, compressed, or
  plain packaged assets.

## Next hardware experiments

1. Use an independent SPP client to read `DEVICE_INFO`.
2. Save the raw reply bytes and decoded fields.
3. Compare decoded MAC/PIN/model/firmware/resource IDs with Android Bluetooth
   pairing records and app data.
4. Only after `DEVICE_INFO` is stable, consider `OQC_DATA_ACCESS_READ`.
5. Avoid OQC write, factory reset, firmware update, and resource update until
   read-only protocol handling is proven.
