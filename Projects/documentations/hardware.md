# Hardware: what is inside a Noodoe module?

[한국어](hardware.ko.md)

This describes the AK 550 / SR1.5-family hardware studied for this project. A peripheral seen in stock firmware is not automatically proven present, connected or working on every motorcycle. The development module also has a damaged Bluetooth/ambient-light path. Numbers below distinguish the [STM32CubeIDE target](../STM32/FuckNudo_Noodoe_CFW_Project.ioc), decoded stock behavior and actual board observations.

## Main signal path

```text
AK 550 instrument cluster ── UART5 / ignition / button signals ── STM32F429IE-class MCU
                                                            ├── SPI1 ── FT81x EVE ── 480×480 LCD
                                                            ├── SPI4 ── panel control
                                                            ├── TIM5 ── backlight PWM
                                                            ├── USART1 ── TI CC256x Bluetooth controller
                                                            ├── I2C3 ── OPT3001 ambient-light sensor
                                                            ├── SPI5 ── external NOR / FAT volume
                                                            └── FMC ── external SDRAM
```

| Part | Observed or implemented role | Confidence / limit |
|---|---|---|
| STM32F429IE-class Cortex-M4F | Hosts the application, vehicle protocol, Bluetooth HCI host and graphics command stream. The Cube target is `STM32F429IET6`. | MCU family and 512 KiB internal flash agree with the vector map and build; the selected Cube suffix must not be mistaken for a photographed package marking. [ST product page](https://www.st.com/en/microcontrollers-microprocessors/stm32f429ie.html). |
| FT81x Embedded Video Engine | SPI graphics coprocessor with 1 MiB graphics RAM. CFW uses display lists, textures and its snapshot command instead of an STM32 framebuffer. | FT81x family is supported by stock-code and register behavior; the exact FT810/811/812/813 variant is not asserted here. [Bridgetek datasheet](https://www.brtchip.com/wp-content/uploads/Support/Documentation/Datasheets/ICs/EVE/DS_FT81x.pdf). |
| LCD and backlight | 480×480 raster behind a circular visible area; separate panel-control and TIM5 PWM paths. | The firmware deliberately renders inside the circular active region. The panel's vendor part number is not established by this repository. |
| External NOR | 128 MiB, 4 KiB erase sectors, with an OEM FAT volume and an OEM update staging region. | Capacity/geometry were read from the examined NOR and file-system image; another PCB revision needs its own check. |
| External SDRAM | 64 MiB board memory used for decoded assets, pictures, capture and working buffers. | Size is the tested configuration assumed by the allocator; exact chip marking is not claimed. |
| TI CC256x | HCI UART radio controller; the STM32 hosts Classic SPP. B/C service packs are selected by detected revision. | The controller family and host protocol are supported by stock/CFW behavior. The bench radio fault does not identify its exact failed component. [TI CC2564C](https://www.ti.com/product/CC2564C). |
| OPT3001 | I2C ambient-light measurement feeding automatic brightness and an outbound brightness step. | Register IDs and stock path identify the device family. Physical light response is unverified on the damaged bench module. [TI OPT3001](https://www.ti.com/product/OPT3001). |

The STM32 has 192 KiB ordinary SRAM and 64 KiB CCM in this memory layout. CCM is CPU-accessible but not a general DMA buffer; CFW places its LVGL pool there and leaves UART/SPI DMA buffers in ordinary SRAM. See [memory-map.md](memory-map.md) and the in-tree [memory contract](../STM32/MEMORY.md).

## Physical controls and power

The original three buttons are active-low MCU inputs: **UP PD12, DOWN PI6, ENTER PA15**. Each has an interrupt edge and a timed/debounced event path. **PH9** is a separate vehicle-versus-Noodoe control selector, not a fourth UI button. **PG13** is the examined ignition-state input: LOW corresponds to key ON, HIGH to key OFF. These are MCU pin names, **not connector cavity numbers**. The power arrangement includes an always-supplied path; turning the key off is not the same as removing all module power.

On a donor cluster, user measurements identified two 12 V lines, three button-related active-low lines and a 5 V-level UART on the main Noodoe connection. Several connector positions and the exact mapping from connector cavity to MCU pin remain unresolved. The small detached 0402 parts found near MCU package pins 118/119 on one damaged bench board are a repair observation, not a validated reference schematic.

The CFW exposes raw peripheral state in its diagnostic paths but does not infer a healthy board from the presence of a driver alone. Bench Bluetooth and ambient-light failures are reported as unverified hardware bring-up, while buttons, display, backlight, UART and NOR have separate evidence histories.

## Reproduction pointers

- Current pin declarations and button timing: [`BSP_Buttons.h`](../STM32/Drivers/BSP/inc/BSP_Buttons.h), [`BSP_Buttons.c`](../STM32/Drivers/BSP/src/BSP_Buttons.c).
- OEM-oriented peripheral notes: [MCU settings](research-notes/ak550-sr15-mcu-peripheral-settings.md), [IOC cross-check](research-notes/2026-09-12-cube-ioc-bsp-bringup.md).
- Connector observations and unresolved cavity numbering: [dash interconnect](research-notes/2026-09-10-noodoe-dash-interconnect.md).
- NOR and USB observations: [storage acquisition](research-notes/2026-09-09-noodoe-usb-acquisition.md).
