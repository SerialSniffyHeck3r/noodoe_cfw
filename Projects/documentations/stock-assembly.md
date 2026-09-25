# How I found the hardware in the stock firmware's assembly

The question came before “how does the updater work?”: **Which MCU and peripherals are on this board, what external chips are wired to them, and how do I bring them up safely in a new BSP?** This is how I combined V5.16 Thumb instructions, restored initial RAM, stock communications code and bench observations. [한국어](stock-assembly.ko.md)

## First, make sure the bytes really are instructions

The stock OTA APP is 458,748 bytes loaded at **0x08010000**. Its first two words give initial MSP **0x20025318** and reset vector **0x08076719**. The vector's low bit signals Thumb state, so instruction execution begins at 0x08076718. `SystemInit` at 0x08073E90 sets VTOR to 0x08010000. That establishes the APP link origin and interrupt table. 0x08000000–0x0800FFFF isn't part of this OTA APP.

For each candidate function I worked in roughly this order:

1. Follow vectors, direct calls and branches to find boundaries. Thumb mixes 16- and 32-bit instructions with PC-relative literal pools between them. Disassembling straight through from a random address is a nice way to turn data into imaginary code. I tried not to do that twice.
2. Match literal addresses to STM32F4 peripheral bases: **0x40013000 = SPI1**, **0x40000C00 = TIM5**, and so on.
3. Compare constants written at offsets in HAL handle structures with the actual HAL layout to reconstruct modes, data bits, prescalers and DMA streams. For GPIO I needed the port base, pin mask, alternate function, output level **and caller**.
4. Identify external chips from JEDEC ID, vendor commands, firmware patch bytes or display-chip `REG_ID`, then check the manufacturer's documentation.

Interrupt vectors were another trap. Many point inside the APP but end in IAR's default endless-loop instruction **FF F7 FE BF**. A vector address alone does not prove that SPI1 or I2C3 IRQ is used. Non-default handlers I could trace included UART5, relevant DMA, TIM2/TIM5, several EXTI lines and OTG_HS. That told me which first BSP paths needed polling, byte IRQ or DMA.

## Narrowing down the MCU

This was one of the big clues that made me think, “Okay, I can actually build firmware for this thing.” STM32? Honestly, that's almost unfairly convenient.

Vector order, Cortex-M4 FPU instructions, GPIO/USART/SPI/FMC register addresses and ST HAL handle layout all pointed to the **STM32F429 family**, a line I'd used before. Initial stack address and internal flash size fit too. Later, two complete 512 KiB physical flash reads matched byte for byte, and the APP region matched the preserved V5.16 OTA image.

## Peripherals, one clue at a time

| Device / bus | Clues in the assembly | How I brought it up |
|---|---|---|
| **FT81x EVE / SPI1** | Handle setup 0x08033C1A uses instance 0x40013000, master, 8 bits, mode 0, software NSS. Callers 0x0801FD2E and 0x0801FF84 pass DIV8 and DIV4. GPIO MSP 0x080373F2–0x08037426 matches PB3/PA6/PB5 AF5. EVE `REG_ID` at 0x302000 returns 0x7C and command-FIFO addresses match FT81x documents. | Read REG_ID at low SPI speed, then enable EVE, 480×480 timing, RAM_G and display lists. The exact FT810/811/812/813 suffix can wait while the screen works. |
| **EVE select/reset candidates** | 0x08033D00–0x08033D0E drives GPIOA mask 0x10 LOW just before a SPI transaction. 0x08034140–0x0803419A toggles GPIOB mask 0x02 LOW, delays, then HIGH; EVE init caller 0x0801FD22 passes 1. | PA4 is supported as CS; PB1 was narrowed down as PDN/reset. |
| **External NOR / SPI5** | PF7/8/9 AF5, PF6 GPIO select; 8-bit master, mode 0. Driver sends 9F RDID and checks **C2 20 1B**, then assumes 128 MiB, 4 KiB erase and 256-byte pages—consistent with Macronix MX66L1G45G. SPI5 MSP 0x0803750A–0x08037616 attaches DMA2 Stream4 TX / Stream3 RX, Channel2. | Match RDID before any storage writes; honor page and sector geometry. |
| **LCD control / SPI4** | PE2/PE5/PE6 AF5, 16-bit master mode 0 DIV16 and commands 10/11/28/29/0A. | Keep panel commands separate from the EVE pixel renderer. |
| **Vehicle data / UART5** | PC12 TX, PD2 RX AF8, 115200 8N1. UART5 IRQ 0x08070E68 → HAL IRQ 0x0804C25C → RXNE 0x0804C55C → completion 0x08043C08 → F5 parser 0x0804EA14. DMA1 S0/S7 is configured too, but **this parser receives through RXNE byte IRQ**. | Parse incoming F5/command/length/payload/XOR first; compare speed and ODO with the real cluster. |
| **Bluetooth / USART1** | PA9/10/11/12 AF7 with RTS/CTS, DMA2 S5/S7 Channel4. Stock includes TI CC256x HCI vendor commands, service-pack bytes and Bluetopia/SPP code. USART1 baud changes after the initial setting. | Initialize and patch the controller through HCI UART, then run SPP at the host. |
| **Ambient light / I2C3** | PH7 SCL and PC9 SDA AF4 open drain, 400 kHz, 7-bit address 0x45. Manufacturer/device IDs **0x5449/0x3001** and register use point to OPT3001. | Ask for ACK and IDs first; then verify lux and brightness policy. |
| **Backlight / TIM5** | 0x0804351A–0x0804363C selects TIM5 0x40000C00, ARR 99, roughly 50 kHz counter target, CH4 PWM1 active HIGH and bounded duty. GPIO MSP 0x08038224–0x08038240 points to **PI0 AF2**. | Make TIM5_CH4 a separate backlight service; start low and check polarity on hardware. |
| **Buttons / IGN / selector** | GPIO port/pin initialization, EXTI callers, TIM2 periodic reads and stock input-allow conditions together identify PD12/PI6/PA15 active-low buttons, PG13 IGN and PH9 selector. | Raw GPIO → debounce → duration event → PH9/IGN policy → screen action. |

An ID and command set agreeing with a datasheet make the FT81x and NOR family calls strong. Initialization alone cannot tell me the exact SPI4 panel part, SDRAM package marking, MCU package or Bluetooth BGA revision. I2C1 also shows a path consistent with an MFi authentication chip; CFW leaves it alone. I am not a devoted Apple customer, I don't own the expensive iPhone required for this detour, and I don't know iOS app development. Three good reasons to spend my time elsewhere.

Addresses, prescalers, DMA streams and pins above belong to the V5.16 image studied here. For the instruction-level trail see the [SPI/PWM cross-check](research-journal/2026-09-12-cube-bsp-crosscheck/README.md), [MCU peripheral notes](research-notes/ak550-sr15-mcu-peripheral-settings.md), [UART/clock corrections](research-notes/2026-09-09-ak550-firmware-uart-boot-review.md) and [full-flash/OTA comparison](research-journal/2026-09-11-full-flash-ab-verification/README.md). Boot/update flow has its own [follow-up study](research-notes/2026-09-11-noodoe-bootloader-and-bluetooth-update.md).
