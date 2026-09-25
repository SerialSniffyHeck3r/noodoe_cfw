# CFW architecture: who runs what, and when?

This follows the [installer story](installer.md). I spent more time thinking about these boundaries than about most of the pixels. Using your head helps, but it is exhausting. lol

I split the system by **what still has to work after another part dies**, not just by source-code directory. If the riding UI can erase arbitrary NOR addresses, or stock recovery needs the UI that just crashed, sooner or later I'll be in a parking garage with tools, peeling my motorcycle apart to reach SWD. No thanks.

The [STM32CubeIDE project](../STM32) and [Android project](../Android) talk over Classic Bluetooth SPP using NDCP. [한국어](architecture.ko.md)

## Boot stages

| Component | When it runs | Job |
|---|---|---|
| Stock resident bootloader | MCU startup and stock APP installation | The original install path. This project leaves its code alone. |
| Bootstrap | First installation, entered through the stock updater | Identify board and bootloader, run SPP, audit FAT, prepare CFW files, receive and physically reread data, ask for installation approval. |
| RecoveryGate | Before Product, or on a failure | Validate/copy Product, manage boot journal and watchdog, restore stock APP using buttons alone. |
| Product CFW | Normal riding | Vehicle state, display, settings, Bluetooth, assets, photos, persistence and trial-boot health reports. |
| Uninstall/Diagnostic images | Explicit temporary operations | Remove CFW data or probe peripherals more deeply. Neither is the day-to-day riding firmware. |

Gate occupies its own 64 KiB internal flash sector. With Product broken, it still uses EVE primitives and ROM fonts; it needs no Product FreeRTOS, LVGL, external font, SDRAM or Bluetooth. Its exact stock APP comes from a verified NOR file. Do I want those 64 KiB back for shiny features? Hell yes. Am I giving them up? Hell no. That sector is the difference between a failed update and taking the bike apart.

## Product layers

**App_Logic** decides what data means and what a button does in the current state. Riding/settings/power are the broad state machines; the middle of the dashboard has smaller page machines. Trip, maintenance, notification and vehicle rules belong here. No application page should need to know a SPI byte order.

**Middlewares/Noodoe** owns bounded queues and asynchronous state: UART parsing, button events, BT/NDCP, brightness policy, assets and photos, FAT ownership checks, settings and ride journals, updates and health. “Request accepted” and “written permanently to NOR” are two different answers.

**Graphics/UI and Graphics/Port** turn the circular screen model into EVE display lists: rings, text, images and animation. Texture ownership lasts until the relevant display-list swap has really completed. The occasional screen-tearing mess I saw from bad EVE address handling made that rule much less theoretical.

**Drivers/BSP** owns MCU pins, UART/SPI/I2C, EVE/LCD, backlight PWM, NOR, SDRAM and watchdog hardware. Cube regenerates some Core files, so project-owned code is kept distinct. **Middlewares/Third_Party** contains pinned LVGL, FreeRTOS and BTstack sources under their own licenses.

## Data while riding

The cluster sends speed, ODO and fuel over [vehicle UART](vehicle-uart.md). The ring, clock and ODO make the common frame around a selected center page: trip, music, calls, notifications or phone GPS. GPS draws a trail; it does not replace the vehicle's speed reading.

One Android service owns the Bluetooth socket and outbound queue. Riding sync and installation are distinct modes; installation pauses music, notifications and photos so they cannot crowd out installation traffic. The phone renders some CJK and variable-width text into bounded images with generation and integrity data. Capability negotiation keeps new screen formats away from older CFW versions.

On IGN OFF the last screen holds briefly, then the confirmed session end shows Ride Summary. Configured stages follow: screen hold/backlight off, panel off/Bluetooth alive, then all off. LCD drive, PWM, radio and MCU sleep are separate controls, not one magic power switch.

## Failure boundaries

A received file is not an installed APP. Length, target, UID, vector, assets, hash and physical NOR reread come before a boot-journal change. A trial Product needs **30 seconds of healthy owner-task progress** plus phone/device version confirmation; an SPP socket alone proves little. Completed journal entries survive interrupted writes, and Gate recopies from verified NOR rather than running a half-written APP. NOR A/B keeps the previous working Product until the new one sticks. Repeated failure stops in recovery instead of rebooting forever. The watchdog can restart a stalled CPU, but it cannot certify that a write completed or that a human saw “success.”

See the [Product state machine](../STM32/App_Logic/STATE_MACHINE.md), [Gate contract](../STM32/RecoveryGate/README.md), [NDCP notes](technical-notes/ndcp-protocol.md) and [installation session](technical-notes/install-session-v2.md).
