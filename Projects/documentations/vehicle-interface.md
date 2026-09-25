# Vehicle interface: buttons, ignition and cluster data

Noodoe sits beside the main AK 550 instrument cluster. The cluster already has its own numeric speedometer; CFW uses the Noodoe display for a speed **ring** and secondary information. Vehicle speed and ODO come from the cluster's serial link. The phone supplies media, notifications, calls and optional GPS trail points; its GPS is not substituted for the cluster's UART speed.

## UART5 frame observed on the examined SR1.5/V5.16 system

```text
0xF5 | command (1 byte) | length (1 byte) | payload[length] | XOR checksum
115200 baud · 8 data bits · no parity · 1 stop bit
```

The XOR covers the prefix through the last payload byte. “Cluster → Noodoe” and “Noodoe → cluster” are logical directions confirmed by bidirectional captures; they are not a guess at physical connector cavity numbers.

| Direction / command | What the original application handles | CFW implication |
|---|---|---|
| Cluster → Noodoe `0x21` (9-byte payload), `0x22` (11-byte payload) | Core vehicle state. Payload byte 0 is speed; bytes 4–7 are little-endian ODO; byte 8 is transformed by subtracting 40 and remains a temperature-like candidate. `0x22` adds an unknown 16-bit field. | Validate frame and freshness before updating the displayed speed/ODO and trip model. Do not invent meaning for unknown bytes. |
| Same frame, byte 3 | In real 0-bar and 1-bar captures, the low nibble changed from 0 to 1; the high nibble remained 5. | Low-fuel/Reserve logic may use the bar count with debounce. Full 2–5 bar mapping is not independently measured by the cited captures. |
| Same frame, byte 2 | Stock firmware feeds a generic six-level widget. | Do not label it an exact fuel percentage on this motorcycle without further observation. |
| Cluster → Noodoe `0x41` / `0x42` | 250-byte and 71-byte records are cached/forwarded through stock phone commands. | Contents and all vehicle warning signals are not decoded; the CFW does not claim RPM, TPMS or voltage from these records. |
| Noodoe → cluster `0xA1` | Payload `{0x01, brightness step 0..9}` derived from the local light-sensor thresholds. | Preserve the step-oriented cluster link; local PWM brightness is a separate policy. |
| Noodoe → cluster `0x01` | Observed re-request/re-initialization and termination values in stock flow. | Keep control traffic distinct from button events. |

The 0.1 km discrepancy between cluster and a computed trip can arise from source sampling, integration and cluster rounding; the UART ODO remains the primary cumulative distance. The CFW stores the last plausible ODO to survive a transient `----`/reset-like sample, but an anomaly is surfaced rather than secretly changing the vehicle's real odometer. Ride and maintenance counters are CFW data, not a write to the cluster's ODO.

## Buttons and mode switch

| Function | MCU input | Electrical interpretation |
|---|---|---|
| UP | `PD12` | Active LOW, edge interrupt plus timed debounce. |
| DOWN | `PI6` | Active LOW, same event pipeline. |
| Center / ENTER | `PA15` | Active LOW; short/long meaning belongs to the current page. |
| Dash/Noodoe selector | `PH9` | Separate input. The stock-derived `HIGH + IGN ON` condition allows ordinary Noodoe controls; LOW blocks them while a persistent dashboard-mode indicator may be shown. |
| Ignition state | `PG13` | LOW = key ON, HIGH = key OFF on the examined firmware. This is a state input, not guaranteed removal of the always-on supply. |

The BSP reports presses, releases and duration. Middleware filters contact bounce and applies the PH9/IGN gate before app actions. An input already held when the selector changes is cancelled; it must be released before it can become a new UI action. Emergency recovery/installer confirmation is a separate input path so the selector cannot accidentally make a failed CFW impossible to recover. On key OFF, normal button prompts do not wake the backlight merely to report a blocked action.

## Light and power relationship

The local OPT3001 path measures ambient light. Stock code classifies it against ten calibration thresholds from factory data and transmits a 0–9 step to the main cluster. Those ten words are thresholds, **not ten PWM percentages**. The Noodoe LCD's TIM5 PWM/backlight curve, chosen theme's dark/light text and the outbound cluster step are separate controls. CFW manual brightness and automatic offset cannot prove that the cluster applied the same brightness unless the cluster-side result is observed.

IGN OFF is handled in stages: retain the old frame briefly, complete the ride summary, then enter the configured screen-held/backlight-off, panel-off/Bluetooth-held and full-off states. The MCU's clock/sleep and radio retention depend on which stage is active. A key cycle must not reset a ride session before the OFF transition is committed.

Evidence and implementation: [UART payload map](research-notes/2026-09-09-ak550-uart-payload-map.md), [live UART direction/ODO comparison](research-notes/2026-09-10-upper-lower-uart-confirmed.md), [fuel 0/1-bar capture](research-notes/2026-09-10-fromdash-gas1.md), [button BSP](../STM32/Drivers/BSP/inc/BSP_Buttons.h), [power-state observations](research-notes/2026-09-10-noodoe-power-state-and-sleep.md).
