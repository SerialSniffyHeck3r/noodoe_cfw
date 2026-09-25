# Noodoe preservation documentation plan

Date: 2026-08-30 KST

## Purpose

This project should produce enough documentation that another engineer can:

1. Understand the Noodoe system architecture.
2. Reproduce captures safely.
3. Build an independent Android or ESP32-class client.
4. Avoid damaging a working meter.
5. Preserve evidence before KYMCO cloud/app shutdown.

## Document set

| File | Purpose |
| --- | --- |
| `README.md` | Project overview and operating cautions |
| `docs/hardware-model.md` | Hardware entities and what the meter can report |
| `docs/transport-and-protocol.md` | Bluetooth/SPP/BLE/protocol notes |
| `docs/app-data-pipeline.md` | How the Android apps generate, queue, and transfer meter-ready content |
| `docs/content-format-notes.md` | Current findings about gallery files, creation bundles, and install metadata |
| `docs/next-actions.md` | Current home/lab work, next bike session order, and implementation milestones |
| `docs/strategy-reassessment.md` | Revised approach: static/Frida first, bike sessions only for validation |
| `analysis/2026-08-30-noodoe-tools-deep-dive/README.md` | Detailed Noodoe Tools APK analysis |
| `analysis/2026-08-30-tabactive3-design-speed-photo-001/README.md` | Tablet design/speed/photo capture analysis |
| `analysis/2026-08-30-ak550-repair-001/README.md` | S24 initial reconnect evidence |
| `capture-log-template.md` | Template for future capture sessions |
| `tool-notes.md` | Local tooling notes |

## Evidence policy

Every claim should be marked as one of:

- Confirmed by local capture
- Confirmed by static APK analysis
- Confirmed by official public source
- Strong inference
- Hypothesis

For each capture session, record:

- Date/time and timezone
- Vehicle state: ignition off/key on/engine running/riding/stopped
- Device used: phone/tablet/PC/ESP32
- App package and version
- Exact user action sequence
- Whether Frida was attached before the action
- Whether Android HCI snoop was enabled before the action
- Result visible on the meter
- Files saved under `captures/` and `evidence/`

## Safety classes

### Class A: read-only

Allowed early:

- Pairing observation
- `DEVICE_INFO`
- Passive Frida stream logging
- Passive dumpsys/logcat capture

### Class B: low-risk write

Use after read-only parser works:

- `MOBILE_STATUS` for time sync
- Benign `APP_NOTIFICATION`
- Benign `UPDATE_WEATHER`

### Class C: state-changing

Use only after backups and repeatable parsing:

- Dashboard/speedometer/clock/weather/gallery file transfer
- Preference setting writes
- OQC test mode

### Class D: hazardous

Avoid until specifically justified:

- `FACTORY_RESET`
- `OQC_DATA_ACCESS_WRITE`
- Firmware update
- Resource update
- Transfer reset/remove against unknown locations

## Near-term milestones

1. Build an independent SPP read-only client.
2. Decode `DEVICE_INFO` from the AK550.
3. Decode current `MOBILE_STATUS` time-sync frame and prove visible clock
   update.
4. Re-run one controlled official-app action with Frida attached before
   pressing the UI action.
5. Document the first successful independent command as a reproducible recipe.
6. Start asset-format analysis for dashboard/speedometer/gallery transfer.

## Questions to keep open

- Does AK550 expose BLE SPP LE service in addition to Classic SPP?
- Is the first 5-byte legacy command sequence required before Sunray 1.5
  commands?
- Does the meter require Android-level bonding before accepting SPP commands?
- Is the 6-digit pairing code only Classic Bluetooth SSP, or does the app also
  store the PIN in meter settings?
- Are dashboard/gallery files raw images, archives, signed packages, or
  precompiled resources?
- Which commands are accepted with ignition off, key on, engine running, or
  riding?
