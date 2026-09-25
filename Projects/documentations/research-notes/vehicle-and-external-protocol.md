# Noodoe vehicle and external-device protocol map

> **2026-09-09 재검증:** SR1 CRC 재현, 하위 flash의 16KiB shadow/4KiB program 구분, UART5 byte-IRQ 수신 및 USART1 baud 재설정에 대한 정정은 [최신 BIN·APK 대조 보고서](2026-09-09-ak550-firmware-uart-boot-review.md)를 우선한다. 아래는 당시 분석 기록이며, 충돌하는 설명을 실물 작업의 확정 근거로 사용하지 않는다.

> **UART payload update 2026-09-09:** See [firmware-only payload map](2026-09-09-ak550-uart-payload-map.md). CMD A1 carries an ambient-light classification index; CMD 01 is an enum-to-one-hot request, and the derived time counter accumulates below speed 5. The older APK/live-observation context below must not be treated as firmware-only field identification.

Date: 2026-09-01 KST

This note separates three layers that must not be confused:

1. **Vehicle-side link**: the motorcycle/meter-side board sends raw vehicle
   state into the Noodoe STM32 over UART5.
2. **Noodoe internal processing**: the STM32 consumes some fields for its own
   dashboard/UI logic and stores some larger blobs.
3. **Phone-side link**: the STM32 exposes selected state to the phone through
   Classic Bluetooth SPP command frames.

The vehicle-side protocol is not the same byte stream as the Android SPP
protocol.

## Answer: does the vehicle really send speed and fuel?

Yes, with the current evidence level:

- **Speed**: very strong. `0x21/0x22` payload byte `[0]` is consumed as current
  speed. The firmware uses it for stopped/riding threshold logic around value
  `5`, below-threshold stop-time accumulation, max-speed tracking, and display consumers.
- **Fuel**: very strong. `0x21/0x22` payload byte `[2]` is consumed as a
  fuel-like gauge input (a calibrated percentage and an upper bound of 100 are not proven), then quantized through threshold logic
  around `20/40/60/80` for the stock segmented display.
- **Odometer / accumulated distance**: very strong. `0x21/0x22` payload
  `[4..7]` is little-endian u32 and is later decomposed into decimal digits.
- **Temperature**: strong. `0x21/0x22` payload byte `[8]` is decoded as
  `raw - 40`, which is a common compact signed-temperature encoding. Whether
  this is ambient, coolant, or another temperature source remains unconfirmed.

What is not yet proven: the exact physical source of each value. It may come
from the main meter board, ECU, ABS/TPMS module, or another vehicle controller
before it reaches the Noodoe UART5 line. The firmware proves the Noodoe STM32
receives and consumes these values; it does not by itself name the upstream
vehicle module.

## Vehicle-side UART5 transport

Confirmed transport:

| Item | Value |
| --- | --- |
| STM32 peripheral | `UART5` |
| Pins | `PC12 TX`, `PD2 RX`, AF8 |
| Baud | `115200` |
| Format | `8N1` |
| Flow control | none |
| DMA | `DMA1 Stream0 Ch4 RX`, `DMA1 Stream7 Ch4 TX` |
| IRQs | `UART5`, `DMA1_Stream0`, `DMA1_Stream7` |

Frame format:

```text
F5 | CMD | LEN | PAYLOAD[LEN] | XOR
```

Checksum:

```text
checksum = XOR(all bytes from F5 through the final payload byte)
```

Example:

```text
F5 01 01 02 F7
```

The parser waits for `0xF5`, then reads command, length, payload, and XOR. On
checksum mismatch it resets/resynchronizes.

## Vehicle-side command map

| Direction | CMD | Payload | Current meaning | Confidence |
| --- | ---: | --- | --- | --- |
| vehicle -> Noodoe | `0x21` | live state | basic telemetry | very strong |
| vehicle -> Noodoe | `0x22` | live state + extra u16 | extended telemetry | very strong |
| vehicle -> Noodoe | `0x41` | exactly 250 bytes | large meter/profile blob | confirmed path, AK content currently all zero through phone read |
| vehicle -> Noodoe | `0x42` | exactly 71 bytes | large battery/status/event blob | confirmed path, field meaning still open |
| Noodoe -> vehicle | `0x01` | 1-byte enum-to-one-hot value | argument 1..4 maps to `01/02/04/08`; confirmed upper paths send `04` for link re-request and `00` on teardown | confirmed code path; peer meaning open |
| Noodoe -> vehicle | `0xA1` | 2 bytes, generated shape `01 xx` | `xx` is local ambient-light classification index 0..9 in the traced update path | source-to-wire confirmed; peer effect open |

## `0x21` / `0x22` telemetry field map

| Payload offset | Current meaning | Firmware consumer | Confidence |
| --- | --- | --- | --- |
| `[0]` | current speed | riding threshold around `5`, max speed, UI speed consumers | very strong |
| `[1]` | unknown byte | copied into state structures; final semantic consumer not closed | open |
| `[2]` | fuel percentage-like value | segmented fuel thresholds around `20/40/60/80` | very strong |
| `[3] low nibble` | status nibble A | split into 4-bit field and reused | open |
| `[3] high nibble` | status nibble B | split into 4-bit field and reused | open |
| `[4..7] LE u32` | odometer / accumulated distance candidate | decimal digit decomposition and display consumer | very strong |
| `[8] - 40` | temperature | offset decode with negative range possible | strong |
| `[9..10] LE u16` | `0x22`-only extra field | parsed only in extended command | open |

Derived ride/status candidate:

```c
struct RideStatusCandidate {
    uint8_t  status;          // open
    uint32_t odometer;        // from telemetry
    uint16_t stop_duration;   // tick/1000 units accumulated while speed < 5
    uint8_t  max_speed;
    uint8_t  current_speed;
};
```

This structure appears to feed phone-side message type/command family `0x10`
in some paths, but not every raw telemetry field is necessarily forwarded to
the phone.

## Bridge into phone-side SPP

The STM32 bridges only selected vehicle-side state into phone-visible messages.

| Vehicle input | STM32 storage/logic | Phone-side result | Current observation |
| --- | --- | --- | --- |
| `F5 0x21/0x22` | live state parser; selected field crosses threshold `5` | `C2 NOTIFY_RIDING`, payload `1/0` | path confirmed; C2 not yet observed in captures |
| `F5 0x41` 250 bytes | RAM buffer around `0x20021A40` | command `0x16 GET_METER_PROFILE`, status u16 + 250 bytes | live phone read returned status `0`, all 250 bytes zero |
| `F5 0x42` 71 bytes | RAM buffer around `0x200227D4`, event path | `C6 UPDATE_BATTERY_DATA`, 71 bytes | firmware and APK parser agree; not yet observed live on AK captures |

This is important for custom firmware:

- Phone-side SPP logs alone do not reveal every vehicle value.
- A value can reach the Noodoe MCU and be used on the dashboard without being
  forwarded to Android.
- To unlock hidden features, firmware patching or direct UART5 sniffing may be
  more valuable than only extending the Android app.

## Other external systems around the STM32

| System | Link | Protocol/signature | Role |
| --- | --- | --- | --- |
| Vehicle/meter-side board | `UART5` | `F5` framed XOR protocol | speed/fuel/ODO/temp/state input; some outbound control |
| Bluetooth controller | `USART1` | HCI UART plus TI CC256x service pack | Classic SPP/iSPP/phone link |
| External NOR | `SPI5` | Macronix commands, JEDEC `C2 20 1B` | resource/theme/photo/firmware staging storage |
| EVE graphics controller | `SPI1` | FT81x register/command FIFO, `REG_ID=0x7C` | display list, image rendering, RGB scan-out |
| LCD panel control | `SPI4` | MIPI-DCS-like commands `0x10/0x11/0x28/0x29/0x0A` | panel sleep/display/power-mode control |
| Ambient light sensor | `I2C3` | TI OPT3001 IDs `0x5449/0x3001`, addr `0x45` | automatic brightness |
| MFi authentication | `I2C1` | Auth coprocessor-style register flow, addr `0x11` | iAP/iSPP authentication |
| Backlight | `TIM5` | PWM/ramp logic | display brightness |

## Custom-feature opportunities

If the current telemetry interpretation survives live UART diff tests, a custom
firmware or patch can add features the stock Noodoe UI does not expose:

| Feature idea | Required source | Current feasibility |
| --- | --- | --- |
| exact numeric fuel percentage | `0x21/0x22[2]` | high, needs live fuel-diff confirmation |
| continuous fuel gauge instead of segmented bars | same | high |
| current speed overlays or alternate speed UI | `0x21/0x22[0]` | high |
| max speed / ride summary | derived ride/status structure | high |
| odometer/trip-derived stats | `0x21/0x22[4..7]` plus time/speed | medium-high |
| temperature display | `0x21/0x22[8]-40` | medium; sensor identity needs closure |
| range estimate | fuel + odometer/speed history | medium; needs calibration |
| warning/indicator/TPMS/RPM/voltage pages | likely `0x41/0x42` or status nibbles | speculative until blob diff |
| debug telemetry recorder | UART5 passive capture + NOR/logging | high for bench/custom builds |

The attractive part is that several features require no new sensors. They only
need the STM32 firmware/UI to stop throwing away detail that the vehicle already
sends.

## Validation plan

Do this before claiming a field is final:

1. Capture UART5 directly at `115200 8N1`, passive only.
2. Decode `F5` frames and group by command.
3. While stationary, change one variable at a time:
   ignition state, side stand, high/low beam, turn indicators, brake, buttons,
   brightness/light exposure.
4. For speed, record wheel-speed/road-speed with a separate source and compare
   `payload[0]`.
5. For fuel, record stock segmented fuel display and long-term consumption; look
   for smooth changes in `payload[2]`.
6. For temperature, compare cold soak, warmed engine, and ambient weather.
7. For odometer, compare the u32 before and after a known ride distance.
8. For `0x41` and `0x42`, save raw blobs and diff them after one controlled
   state change.

Until those captures exist, the correct language is:

- speed/fuel/odometer: **very strong firmware-derived interpretation**
- temperature: **strong interpretation, source unknown**
- warning/RPM/TPMS/voltage: **possible but not yet proven**

## Decoder tooling

The repo now contains a small UART5/F5 decoder for raw captures:

```powershell
$env:PYTHONPATH = (Resolve-Path .\src).Path
$python = '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'

# Raw binary capture from a logic analyzer/export.
& $python .\tools\decode_vehicle_uart.py .\captures\uart5.bin

# Text/log file containing hex bytes such as "F5 21 09 ...".
& $python .\tools\decode_vehicle_uart.py .\captures\uart5.txt --hex-text

# JSON lines for later diff scripts.
& $python .\tools\decode_vehicle_uart.py .\captures\uart5.txt --hex-text --json

# Compare two controlled captures, for example before/after one switch change.
& $python .\tools\diff_vehicle_uart.py .\captures\before.txt .\captures\after.txt --hex-text

# Same comparison as JSON for field notebooks.
& $python .\tools\diff_vehicle_uart.py .\captures\before.txt .\captures\after.txt --hex-text --json
```

Implementation files:

- `src/noodoe_protocol/vehicle_uart.py`
- `src/noodoe_protocol/vehicle_diff.py`
- `tools/decode_vehicle_uart.py`
- `tools/diff_vehicle_uart.py`
- `tests/test_vehicle_uart.py`
- `tests/test_vehicle_diff.py`

## Source pointers

- `AK550_Noodoe_Firmware_Reverse_Engineering_Report_2026-09-01_v1.1_BSP.docx`
- `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`
- `analysis/2026-09-01-custom-firmware-hardware-map/IRQ-MAP.md`
- `analysis/2026-09-01-custom-firmware-hardware-map/hardware-map.json`
