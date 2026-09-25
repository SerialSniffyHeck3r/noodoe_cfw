# OpenNoodoe 0.2.0 live AK 550 validation

Date: 2026-08-31 KST

## Executive result

OpenNoodoe's modern Classic Bluetooth RFCOMM/SPP transport and every command
exposed in version 0.2.0 were recognized and answered by the test AK 550 running
firmware 5.16 and resource 5.14. Valid-state requests succeeded. Notification
display and breathing-light ON/OFF were also confirmed visibly by the user.

The read-only riding-status and OQC-data candidates are no longer merely static
hypotheses: each returned a correctly framed command reply with `SUCCESS(0)`
twice. Clock/mobile-status writes returned `SUCCESS(0)` three times, although a
separate visual clock comparison was not recorded.

## Preserved evidence

Primary capture directory:

`captures/opennoodoe-live-2026-08-31-0339/`

| File | SHA-256 |
| --- | --- |
| `20260831-033323-024/protocol.log` | `09E08585C8D2C4E235BCA09DE458FF137A283626A2868E66B6763C134AA1CB98` |
| `20260831-033459-580/protocol.log` | `C53C770C35522D217C8331FCB6A8A79B2AF5B786C0A6039BAD3D358756D8F2CA` |
| `opennoodoe.log` | `E9328E60E3CD26C906FFF212F888FE843DC6D3D85B46EA82974FCC811F72142F` |
| `bluetooth-manager.txt` | `0EF58A21BE27A0D3428015C69F5701133EFD39CFCB2C93D3856487108F18AB6C` |

The app-specific logs preserve application payloads before Bluetooth radio
encryption and are sufficient to validate command framing and replies.

## Command results

Counts below include only commands that reached connected SPP. Button presses
made while disconnected were logged as skipped and did not reach the meter.

| Command | Sent and ACKed | Command reply | Result |
| --- | ---: | --- | --- |
| `DEVICE_INFO 0x05 READ` | 4 | four complete 82-byte replies | Confirmed |
| `MOBILE_STATUS 0x02 WRITE` | 3 | `status=0` three times | Protocol confirmed; visual clock change not recorded |
| `BREATHING_LIGHT 0x0E WRITE` | 15 | eight success, seven invalid-state replies | Confirmed and state-dependent |
| `RIDING_STATUS 0x0C READ` | 2 | `status=0`, 11-byte data twice | Confirmed |
| `OQC_DATA_ACCESS_READ 0x11 READ` | 2 | `status=0`, 139-byte data twice | Confirmed |
| `APP_NOTIFICATION 0x15 WRITE` | 10 | `status=0` ten times | Fully confirmed, including visible display |

No connected command required a sequence retry. There were no no-ACK failures,
packet-index resynchronizations, duplicate frames, checksum/frame rejections, or
payload parse failures. Mean notification ACK latency was 78.2 ms. OQC read and
riding-status mean ACK latencies were 47.5 ms and 84.5 ms respectively.

## Device and OQC data

Running-device information remained identical across replies:

- model: `SAA1AA(KR)`
- firmware: `5.16`
- resource: `5.14`
- boot: `0.15`
- PCBA: `SR0701`
- MAC: `98:07:2D:XX:XX:XX`
- serial: `[redacted factory serial]`
- maximum speed field: `200`
- motor series: `1`
- default dashboard ID: `1`

The OQC production block returned:

- part number: `0037150-LGC6-E01`
- assembly number: `0037130-LGC6-B80`
- serial, PCBA, MAC and model matching `DEVICE_INFO`
- PIN: `1`
- backlight thresholds: `8103, 2981, 1097, 403, 148, 55, 20, 7, 3, 0`
- maximum speed: `200`
- language: `12`
- OQC firmware field: `1.62`
- panel version: `4`
- unit/type/language-pack: `1/1/2`
- motor/resource/dashboard IDs: `1/1/1`

The OQC `1.62` value is a stored production-data field, not the currently
running firmware. The live `DEVICE_INFO` command independently and repeatedly
reported running firmware 5.16.

## Riding status

Both reads returned `keyOn=true`, odometer `25392`, speed `0`, and maximum speed
for the current session `0`. The deprecated stop-duration field changed from 19
to 16; its unit is not established by the static API. The values are internally
consistent with a stationary, key-on vehicle and the owner's approximately
25,000 km odometer statement.

## Breathing-light state rule

Reply status 5 is the official `ERROR_INVALID_STATE`, not a transport failure.
The raw `NOTIFY_KEY_ON 0xC3` payload defines `0=OFF` and `1=ON`.

- With key ON, light commands were ACKed but returned `status=5,state=0`.
- After `C3 payload=0` (key OFF), light ON/OFF returned `status=0` and the
  requested state `1/0`, matching the user's visible observation.
- After `C3 payload=1` (key ON), a light-OFF write again returned invalid state.

The welcome light is therefore a key-off function. The app should present
invalid-state as a normal precondition failure rather than a protocol error.

## SPP ownership conflict

The long connection-failure period was not caused by command framing. Android's
Bluetooth manager history proves that the official app and OpenNoodoe competed
for the meter's single SPP endpoint:

- At 03:34:01, `com.noodoe.sunray` opened UUID `00001101-...` while OpenNoodoe
  was connected; OpenNoodoe's reader then closed.
- During 03:35-03:36, the official app repeatedly opened or retained the same
  RFCOMM service while pairing was also being changed.
- At 03:36:46 the official app connected; OpenNoodoe's first 03:36:49 attempt
  failed, then its second attempt connected at 03:36:51 and remained healthy.

Only one app can own this SPP connection. Future tests must force-stop the
official Noodoe app before connecting OpenNoodoe. Re-pairing is not needed for
ordinary reconnects and made this session harder to interpret.

The final disconnect at 03:39:56 was a controller-reported link supervision
timeout at RSSI -76 dBm, not a protocol parse or command failure.

## Additional meter notifications

The meter emitted `NOTIFY_KEY_ON 0xC3` with both OFF and ON values, proving the
unsolicited state-notification path. It also emitted `NOTIFY_RUNNING_CREATION
0xC1` with a 67-byte all-zero body. In the official parser this represents
default-dashboard foreground/background state with no active group/navigation
payload. OpenNoodoe currently ACKs this correctly but does not decode it in the
UI.

## Revised status

- Fully confirmed: SPP framing, device information, notification display,
  welcome-light ON/OFF under key-off conditions.
- Protocol confirmed: clock/mobile status, riding-status read, OQC-data read.
- Still requiring a dedicated visible test: actual clock correction.
- Not tested and still blocked: OQC mode/write, factory reset, firmware/resource
  installation, user-content file transfer.
