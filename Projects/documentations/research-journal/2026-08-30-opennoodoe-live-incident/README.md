# OpenNoodoe 0.1.0 live incident analysis

Date: 2026-08-30 KST

Preserved evidence:

- `captures/opennoodoe-live-2026-08-30/opennoodoe.log`
- `captures/opennoodoe-live-2026-08-30/logcat-full.txt`
- `captures/opennoodoe-live-2026-08-30/bluetooth-manager.txt`

## Result

The Android process did not crash and no ANR or `FATAL EXCEPTION` for
`io.opennoodoe.app` was present. The operational failure had two independent
causes in OpenNoodoe 0.1.0.

### 1. Unbounded reconnect loop

The service attempted SPP connection 1,521 times and completed 502 failed
three-attempt cycles between 19:54 and 23:39. The first saved target was
`78:33:AC:7A:BB:B6` (SmartRemote), not the dashboard. After the dashboard was
selected at 22:40, the same recurring retry policy continued whenever the
dashboard was unavailable.

Version 0.1.1 removes boot-time and automatic recurring reconnect. Selecting a
device or pressing `Connect saved` starts one three-attempt cycle. Failure then
stops until another explicit user action.

### 2. Invalid validation of outer-frame byte 8

The dashboard completed raw bootstrap four times and returned full framed
traffic. Version 0.1.0 incorrectly required outer-frame byte 8 to be zero.
Live ACK and data frames used values including `DE`, `DC`, `FF`, `02`, `04`,
and `DA`. The official Noodoe parser starts payload at byte 9 but does not
validate byte 8.

Consequences:

- 21 valid sequence frames were rejected.
- ACKs were not recognized consistently.
- `DEVICE_INFO`, `MOBILE_STATUS`, and `BREATHING_LIGHT` were retransmitted.
- The dashboard repeated its `DEVICE_INFO` and `C1` notification payloads.
- SPP was eventually closed while the phone and dashboard disagreed about
  sequence progress.

Version 0.1.1 treats byte 8 as opaque on receive and continues writing zero on
phone-originated frames, matching the official implementation.

## Confirmed successful exchange

The first successful session included:

```text
TX bootstrap: 05 00 00 00 00
RX bootstrap: 85 27 00 00 00 ...
TX DEVICE_INFO: 5A FF 14 00 ... A5 5A 05 01 ... FB
RX command: 0x05, attr 0x09, payload 82 bytes
RX command: 0xC1, attr 0x10, payload 67 bytes
RX command: 0x02, attr 0x0A, payload 2 bytes, status 0
```

Parsed dashboard identity:

- firmware: 5.16
- transitional protocol fields: protocol 0.0, hardware 0
- model: `SAA1AA(KR)`
- maximum speed: 200
- series: `[redacted factory serial]`
- PCBA: `SR0701`
- dashboard ID: 1
- Bluetooth MAC bytes are little-endian and resolve to
  `98:07:2D:XX:XX:XX`

Using the corrected decoder, all 23 preserved `RX SPP` chunks decode without
error. This is direct live proof that the AK550 accepts the raw bootstrap and
modern sequence/command framing implemented by OpenNoodoe.

## Safety changes in 0.1.1

- no boot receiver;
- no automatic recurring reconnect;
- no automatic clock or breathing-light write after connection;
- only read-only `DEVICE_INFO` is sent automatically;
- clock and breathing-light commands require explicit button presses;
- real AK550 nonzero byte-8 ACK vector added to Java and Python tests.
