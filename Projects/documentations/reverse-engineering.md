# How I figured out the stock system

I wasn't only trying to draw a new screen. I needed the original vehicle data to keep working, and I wanted a way back to stock when something went sideways. The machine-code detective story is in [stock firmware assembly](stock-assembly.md). [한국어](reverse-engineering.ko.md)

## How the evidence accumulated

1. **Preserve the original.** Before changing anything, I read the MCU internal flash and external NOR and recorded hashes. The V5.16 stock APP, lower boot code and factory data were kept separately.
2. **Listen to the bike.** Passive UART captures showed 115200 8N1 and frames built from F5, command, length, payload and XOR. I compared the bytes with the cluster's actual ODO and fuel display, then found the outbound A1 brightness command.
3. **Read the stock APP.** Command dispatch, GPIO tables, EVE initialization and update/boot calls led to UART5, the three buttons, IGN on PG13, FT81x, NOR, Bluetooth HCI and the ambient-light path.
4. **Cross-check the original phone app.** Its pairing, file-transfer and device-information commands exposed the stock Bluetooth update path. CFW uses the same Classic SPP transport but speaks its own NDCP messages above it.
5. **Bring things up one at a time.** LCD and backlight, buttons, vehicle UART, NOR and phone connection each got their own tests. Where possible I compared EVE captures with photos of the actual screen.

## Three guesses that deserved the bin

| What I was pretty sure of | What the evidence actually said |
|---|---|
| “Obviously the STM32 draws the LCD with LTDC and a full framebuffer.” | Nope. Stock sends graphics commands over SPI to an FT81x EVE. CFW therefore uses display lists and GPU RAM_G. |
| “Command 01 must contain button states.” | The buttons are separate active-low GPIO inputs. Command 01 belongs to a request/initialization control flow. |
| “Key OFF means power is completely gone.” | The board has an always-on supply as well as PG13 IGN. Screen, backlight and Bluetooth have to transition separately, and recovery cannot assume somebody unplugged the battery. |

The V5.16 vectors and command branches, captured UART frames, matching full-flash and OTA APP bytes, initialization code and bench bring-up are observations. Other connector cavities and several status bytes haven't received a useful name yet. I don't see much value in poking every last unknown merely for the pleasure of poking it.

Dated research notes deliberately retain the hypotheses I held on those dates, including ones I later corrected. A lot of the `research-notes` files began as AI-generated scratch notes for me; managing every scrap by hand sounded even less fun, so I published the lot. Some work predates 30 August by quite a while. Read the dated notes as a lab notebook, not as the final word.

The raw trail: [UART payload map](research-notes/2026-09-09-ak550-uart-payload-map.md), [both UART directions](research-notes/2026-09-10-upper-lower-uart-confirmed.md), [fuel at zero and one bar](research-notes/2026-09-10-fromdash-gas1.md), [boot/Bluetooth update research](research-notes/2026-09-11-noodoe-bootloader-and-bluetooth-update.md), and the [stock APP assembly audit](research-journal/2026-09-09-ak550-boot-update-audit/README.md).
