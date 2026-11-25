# Connecting a third-party app to NewDoe

## Reading the installed version over Bluetooth Classic

The released dashboard accepts one paired SPP client at a time. On Android, open Device → Device connection / app theme → Use another app. In the independent [Android test app](https://github.com/SerialSniffyHeck3r/noodoe_cfw/releases/download/cfw-v1.0.1.1/NewDoe-Interop-Probe-Android.apk), tap **Read dashboard version**, grant Nearby devices access if prompted and choose the paired dashboard. The test app opens the secure SPP service, makes three inquiries, displays the result and closes the socket. To resume riding with NewDoe, choose **Reconnect with NewDoe**. Firmware installation and phone re-pairing are unnecessary for this handoff.

Open the SPP UUID `00001101-0000-1000-8000-00805f9b34fb` using the paired device’s secure RFCOMM service. Issue one request at a time. A response may span several socket reads. The wire header is 16 bytes: ASCII NDCP, version u8, opcode u8, flags u16, sequence u32, payload length u16, reserved zero u16. Integers are little-endian. Append CRC-32/ISO-HDLC as u32 over header and payload. The maximum payload is 1,024 bytes. For these queries flags and payload length are zero.

| Inquiry | Complete request in hex | Successful reply payload, including result |
|---|---|---|
| HELLO | `4e44435001010000010000000000000070231300` | 72 bytes. Offset 4 is NDCP protocol version 1. |
| Capabilities | `4e444350010d0000020000000000000054b68684` | 16 bytes. Schema 2 at offset 4, Product role 2 at offset 8, feature bits at offset 12. |
| Product boot status | `4e444350015b00000300000000000000ada127d8` | 80 bytes. Schema 2 at offset 4. Bytes 48–79 are the active image SHA-256 from the boot journal. |

Match version, opcode and sequence, check reserved bits, length and CRC, then read the u32 result at payload offset 0. Success flags are 1 with result 0. Error flags are 3 with a nonzero result. Event frames have flags 0 and a high-bit sequence. Match the response to the outstanding inquiry. Discard state on disconnect and start a fresh inquiry. Never replay a write after losing its acknowledgement.

The app version and NDCP protocol version describe different things from the running dashboard firmware. Compare the journal hash against a released Product image. Unknown SHA-256 values stay unknown. These hashes identify the riding images of firmware 1.0.1.0 included in Companion 1.0.1.1.

| Firmware profile | Active image SHA-256 |
|---|---|
| A-en | `7a3a40c0ab2d01ebeb2510f28456cd42476491450aaa42fe772da43c8abeea85` |
| A-ja | `9e795ddd2e94eda8342a6d086bddf777451fa2b151032b5c6f3a915f0e337cb1` |
| A-zh-Hans | `d4b26c886b2b4aac7207adf48d7c52d96517f7289f6b3ca742dbfeaad49f620e` |
| A-zh-Hant-TW | `eb62dd9b97a6f7d68e91ebcec08786c2bef7f2f3396357869f0ef01a2d9a4c68` |
| A-zh-Hant-HK | `d6fc497446e6fb9f5d352805c60355960ea6e40fb30764c4fc6176a9224667a4` |
| B-en | `beb0d576db13fa58201951614e1163990cdf4a473ff1cecdcbab0c49b096a8b1` |
| B-de | `ab0ddd2c1ac3a0bf9530bd58eb7439fc4a991a408764a35a8697642e81a9e03e` |
| B-es | `069a6beb9fa83fbdce0e8cd2536d932fe71c747cb10196da1f57b86b757c2394` |
| B-fr | `fdb8fa9be6ae2220109ef7aa4ce2b93ec59aea1ab55fabc858d5f0b9cb957f46` |
| B-it | `64e30fd164f58411e17c8753684551daf189c697126b1a0f3484b85813340974` |
| B-el | `1f2847adbbe93c150608e60a165c6839f925b8e7234403d8b713f05abf0db649` |
| B-ko | `fc05c55827b50900e2b4f424718021c2837f71490899bc38ba2c664c63025727` |
| B-vi | `261cde98e9c0bf0647b4cbb4b4b0e5501967ab96cd0b566247bdec08c026ecab` |
| B-ja | `d962ea9fd1cad182cec37ccc981a19ad02e1bb43c8b12c8070788ec42e7dba47` |

## Riding features

Start a new connection generation with Product role and capability discovery. A read-only inquiry can stop after boot status. An app implementing riding features must also recognize the exact Product contract, obtain the current generation from 0x70, and negotiate extended features through 0xA8. Drop cached session handles on reconnect. Keep the dashboard’s existing installation, confirmation and recovery flows for maintenance.

| Feature | Data an independent client can supply |
|---|---|
| Clock, location | The phone’s local time and timezone, and permissioned location with valid fix and units. |
| Navigation | The client’s own maneuver, turn distance, road name and route summary. Send fields only when available. |
| Weather | Current conditions and forecasts that the client can obtain with the rider’s permission. |
| Media | Metadata, playback state and artwork the client can access. |
| Notifications | Only content the client is allowed to receive and forward. |

Availability of other apps’ notifications, navigation and media on iOS needs separate device testing. Android notification-listener behavior does not establish equivalent iOS access. [NDCP protocol notes](ndcp-protocol.md).

## iPhone lab test

NewDoe on Android and independent Android clients use paired Classic SPP. The iPhone prototype is for External Accessory over authenticated iAP2. Its first milestone is to open one authorized session, query the three frames above and match the journal hash to the dashboard.

An accessory provider must supply both the authorized protocol string and an authenticated session on the accessory. The prototype accepts application bytes from that session. The released dashboard implements Classic SPP and has no integrated MFi driver or iAP2 backend. Installing an iPhone app alone cannot enable this path. [Apple External Accessory](https://developer.apple.com/documentation/externalaccessory) · [MFi FAQ](https://mfi.apple.com/en/faqs).

Record the controller revision, dashboard SHA, phone/iOS build, transport, last confirmed stage and elapsed time to first valid reply. Repeat after app handoff, radio toggle, screen lock, ignition off/on and moving out of range. Check that a second client cannot claim the same session, that the original Android client can reconnect, and that idle retries stop. Measure transfer throughput and idle power before enabling riding features. The iPhone path awaits physical qualification.
