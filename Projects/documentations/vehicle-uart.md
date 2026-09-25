# Cluster ↔ Noodoe UART: the bytes that actually went past the probe

I checked the V5.16 stock code against captures in both directions from an AK550 cluster. They communicate through **UART5, 115200 baud, 8N1, no hardware flow control**. At the MCU this is PC12 TX and PD2 RX (AF8). Harness signals measured around 5 V; that is not an invitation to feed 5 V straight into an MCU pin. Nor do MCU pin names give you connector cavity numbers. [한국어](vehicle-uart.ko.md)

## Frame format

```text
F5 | CMD | LEN | PAYLOAD[LEN] | XOR
```

The final byte XORs all bytes from F5 through the last payload byte. XORing a complete valid frame gives **00**. LEN counts payload bytes only. A PC serial log can split one frame across multiple receive rows; reassemble the byte stream before judging it. In CFW, `Vehicle_Service.c` seeks F5, collects `4 + LEN` bytes, and drops one F5 to resynchronize after a bad XOR. Partial-frame timeout is 100 ms; a vehicle snapshot goes stale after 1500 ms.

## Cluster → Noodoe

| Command | Payload size | Evidence |
|---|---:|---|
| 0x21 | 9 | Speed, ODO and fuel-related state; 116 valid frames in the stationary capture. |
| 0x22 | 11 | Same first nine bytes plus an extra little-endian 16-bit value whose meaning I haven't pinned down. |
| 0x41 | 250 | Large block cached by stock code and relayed to the phone in message 0x16. |
| 0x42 | 71 | Another stock cached block, relayed in message 0xC6. |

Payload positions start at **zero**:

| Offset | Reading |
|---|---|
| 0 | Integer speed in km/h. The stationary cluster displayed 0 and sent 00. |
| 1 | Unresolved. |
| 2 | A six-step generic gauge path exists in stock code, but the measured AK550 fuel changes did not move this byte. Don't call it the fuel gauge on that evidence. |
| 3 | Split into high and low nibbles. At physical zero fuel bars it was 0x50; at one bar it was 0x51. Low nibble 0/1 is measured. High nibble 5 may be the gauge's total bar count; that part is an interpretation. |
| 4–7 | Little-endian 32-bit ODO. `7B 8E 00 00` is **36,475 km**, exactly what that cluster showed. |
| 8 | Temperature-like candidate: stock code subtracts 40. Raw 00 does **not** prove the bike was at −40 °C. |
| 9–10 | Only in 0x22; unresolved LE16. |

These are real captured frames, not proposed packet layouts:

```text
Stationary: F5 21 09 | 00 00 00 00 7B 8E 00 00 00 | 28
0 fuel bars: F5 21 09 | 00 00 00 50 7B 8E 00 00 00 | 78
1 fuel bar:  F5 21 09 | 00 00 00 51 7B 8E 00 00 00 | 79
```

The zero- and one-bar files yielded 23 and 33 valid frames respectively. I haven't directly captured bars two through five. And no, the big 0x41/0x42 blocks do not magically become RPM, TPMS or battery voltage just because those would be nice features. What I found is caching and relaying in stock code, plus frame recognition in CFW.

## Noodoe → cluster

| Command | Captured / code evidence | Reading |
|---|---|---|
| 0xA1 | `F5 A1 02 | 01 07 | 50` | `01` and ambient-light class `07` on a 0…9 scale. It isn't 70% PWM or raw lux. Stock classifies against ten factory thresholds. |
| 0x01 | Request `F5 01 01 04 F1`; stop `F5 01 01 00 F5` | Startup, re-request and shutdown control. The stock mapping of input 1…4 to 01/02/04/08 does **not** make these button-state bits. |

There were 30 valid A1 frames, median interval about **401 ms**. I excluded the first candidate because its checksum ended in `79`, not `50`. The other direction had 116 valid 0x21 frames with a typical interval near **99 ms**. Those logs were captured at different times, so they do not prove a literal four-in, one-out exchange on a shared timeline. Both directions began making sense only after the capture's ground connection was fixed. All those apparent `55` bytes from the earlier attempt? Ground was the culprit, not a new framing protocol. Of course it was.

Stock 0x01 flow also shows a 3000-tick startup wait, a 400 gap and 800 retry, and A1 queueing after four valid inputs. Actual timing depends on state, so I keep that code path separate from the capture intervals.

## The CFW implementation

[Vehicle_Service.c](../STM32/Middlewares/Noodoe/Vehicle/src/Vehicle_Service.c) validates the input stream; [Dash_Protocol.c](../STM32/Middlewares/Noodoe/Vehicle/src/Dash_Protocol.c) constructs outbound frames. [DashService.c](../STM32/Middlewares/Noodoe/Vehicle/src/DashService.c) owns UART access and async TX, distinguishing queue acceptance from physical completion. [BSP_Dash.c](../STM32/Drivers/BSP/src/BSP_Dash.c) handles UART5 pins and transfer. Noodoe does not write a freshly invented ODO back into the cluster.

For the raw trail: [both directions](research-notes/2026-09-10-upper-lower-uart-confirmed.md), [fuel zero/one](research-notes/2026-09-10-fromdash-gas1.md), [payload map](research-notes/2026-09-09-ak550-uart-payload-map.md), [oscilloscope decode](research-notes/2026-09-10-agilent-scope-uart-decode.md) and [stock-code input cross-check](research-notes/2026-09-10-input-observations.md).
