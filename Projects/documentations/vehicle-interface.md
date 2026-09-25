# Vehicle interface: buttons, ignition and the cluster

The AK 550 cluster already has a numeric speedometer. My outer **speed ring** and center pages sit alongside it. Speed, ODO and fuel arrive from the cluster over UART; music, notifications, calls and the GPS trail arrive from the phone. The vehicle reading, not phone GPS speed, drives this UI. [한국어](vehicle-interface.ko.md)

## The AK550 SR1.5 / V5.16 vehicle UART

Stock code and captures from both directions give **UART5 at 115200 baud, 8N1**. Each frame is `F5 | command | payload length | payload | XOR`. For the actual bytes and measurements, see the [vehicle UART notebook](vehicle-uart.md).

| Direction / command | What the bytes say | CFW behavior |
|---|---|---|
| Cluster → Noodoe 0x21 (9 bytes), 0x22 (11 bytes) | Payload 0 is integer speed; 4–7 form little-endian ODO; 8 looks temperature-related but lacks a confirmed unit. 0x22 adds an unresolved 16-bit value. | Validate frame and freshness, then update vehicle and trip models. |
| Payload byte 3 in those frames | At measured zero and one fuel bar, its low nibble changed 0 → 1 while the high nibble stayed 5. The stock gauge has five bars. | Use the low nibble for low/critical fuel and Reserve behavior. |
| Payload byte 2 | I don't have a useful interpretation yet. | Unused. |
| Cluster → Noodoe 0x41 / 0x42 | Large 250-/71-byte records cached and relayed in stock code. | Seen by parser; no invented RPM/TPMS meaning. |
| Noodoe → cluster 0xA1 | `{0x01, brightness step 0…9}` from local ambient-light policy. | Treat cluster brightness step separately from Noodoe's own LCD PWM. |
| Noodoe → cluster 0x01 | A control/request flow with values seen during startup and shutdown. | No vehicle UI field assigned. |

CFW integrates sampled speed for its trips, so a tenth of a kilometer can differ from the cluster's distance calculation. It neither writes an invented ODO back to the cluster nor has a path to do so. When UART ODO suddenly becomes dashes or an implausible value—the sort of bug reported on older AK550s—the app keeps the last believable value and tells the rider.

## Three buttons, one selector, one ignition signal

| Function | MCU pin | Observed behavior |
|---|---|---|
| UP | PD12 | Active low; edge and timed debounce feed a button event. |
| DOWN | PI6 | Active low, same event path. |
| O / ENTER | PA15 | Active low; page decides short versus long action. |
| Cluster/Noodoe selector | PH9 | With IGN ON, HIGH allows Noodoe control and LOW blocks it in the examined stock condition. |
| Ignition | PG13 | LOW = IGN ON, HIGH = IGN OFF on this board. Not the same as removing the always-on supply. |

BSP reports press/release/duration; middleware debounces and applies PH9/IGN policy. If PH9 flips to blocked while a button is held, that press is canceled. Flip it back and the rider must release and press again. Ordinary UI blocking must never intercept the emergency key+O Gate gesture or install/recovery confirmations. Key OFF won't light the backlight just to explain a blocked button.

## Light and sleep

OPT3001 readings pass through ten stock factory thresholds into a 0–9 step, which Noodoe sends in A1. Its own backlight PWM, the cluster-bound step and the light/dark UI theme are three different controls. The receiving cluster appears to divide those steps into day and night modes; I haven't spent an afternoon finding the exact switching point. Laziness? Perhaps. A functioning dashboard seemed like the better use of the afternoon.

IGN OFF holds the prior display briefly, confirms session end, shows Ride Summary and then enters configured sleep stages. A quick OFF→ON does not reset an unconfirmed ride; I have an unfortunate habit of flicking the key quickly, so this one got exercised. Screen hold, backlight OFF, panel OFF/Bluetooth alive and all-off each have different clock, sleep and radio conditions.

Sources: [UART payload map](research-notes/2026-09-09-ak550-uart-payload-map.md), [two-way captures](research-notes/2026-09-10-upper-lower-uart-confirmed.md), [fuel measurement](research-notes/2026-09-10-fromdash-gas1.md), [button BSP](../STM32/Drivers/BSP/inc/BSP_Buttons.h), [power-state notes](research-notes/2026-09-10-noodoe-power-state-and-sleep.md).
