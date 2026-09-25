# Memory map and resources

[한국어](memory-map.ko.md)

## Internal flash and RAM

| Address | Size | Purpose |
|---|---:|---|
| `0x08000000–0x0800FFFF` | 64 KiB | Original resident boot/install code, metadata and factory-related data. CFW settings are not stored here. |
| `0x08010000–0x0801FFFF` | 64 KiB | Independent RecoveryGate, the first application erase sector. |
| `0x08020000–0x0807FFFF` | 384 KiB | Product CFW, replaceable independently of Gate in ordinary updates. |
| `0x20000000–0x2002FFFF` | 192 KiB | Ordinary SRAM for DMA-accessible buffers, 48 KiB FreeRTOS heap, stacks, queues and diagnostics. Last 256 bytes are a reserved Gate/Product mailbox. |
| `0x10000000–0x1000FFFF` | 64 KiB | CCM, including a guarded 48 KiB LVGL pool. CCM must not be handed to a DMA peripheral. |

Gate and Product together consume the 448 KiB application allocation. The build rejects a Product that crosses its linker limit or violates the configured Release/Debug memory budgets. A representative 6.11.4 build retained about **67 KiB Release** and **35 KiB Debug** internal flash headroom; a future build must be measured again. These are link-time numbers, not free RTOS heap or an observed frame rate.

## External memory

The studied board has a **128 MiB NOR** and **64 MiB SDRAM**. NOR is nonvolatile storage; SDRAM is working memory and loses contents when power is removed. The OEM NOR includes a FAT volume and an update staging reservation. CFW creates named, fixed-size files only after checking both FAT copies, ownership of every cluster and conflicts with reserved regions. The OEM update reservation at NOR offset `0x07F80000–0x07FFFFFF` is retained for the original installer.

| CFW-owned FAT file | Size | Function |
|---|---:|---|
| `NOODOE.RSC` | 1 MiB | Font bitmap and CC256x patch resources; two 512 KiB validation/activation banks. |
| `CFWCFG.DAT` | 128 KiB | Versioned settings and pairing records in a rotating journal. |
| `CFWRIDE.DAT` | 256 KiB | Trip and maintenance counters/checkpoints. |
| `CFWPIC.DAT` | 1 MiB | Three custom picture slots with per-picture replacement safety. |
| `CFWREC.DAT` | 512 KiB | Pinned stock application used by local restore. |
| `CFWA.DAT`, `CFWB.DAT` | 512 KiB each | A/B validated Product images, each with header, 384 KiB padded body and identity record. |
| `CFWBOOT.DAT` | 64 KiB | Boot/update state journal. |
| `CFWLOG.DAT` | 256 KiB | Bounded on-device event log. |
| `CFWTEXT.DAT` | Optional, version-dependent | CJK/name cache; support is checked by capability and file audit. |

**A/B** means one bank can remain known good while the other is prepared. It does **not** mean two independent internal-flash Product partitions. Only one Product executes at `0x08020000`; Gate copies a verified selected NOR image into that address. CFW settings, stock pictures, stock FAT files and update staging have different ownership and retention rules. Restoring the stock APP with Gate normally leaves CFW files on NOR; an explicit data-erasure path is separate.

Large immutable assets are loaded from NOR into SDRAM before UI/BT consumers use them. The Product keeps font mapping/metrics in internal flash while the larger glyph bitmaps live in the external resource package. Named SDRAM allocations include assets, picture decode, a 480×480 RGB565 capture (460,800 bytes) and install workspaces; they are allocated once/reused rather than growing with every menu transition.

## EVE graphics memory

The FT81x has **1 MiB RAM_G**. The current design budgets a roughly 96 KiB front region for font/icon cache and alignment, two 450 KiB RGB565 picture banks for crossfades, and a roughly 22.5 KiB 480×24-row snapshot buffer. A full-screen RGB565 screenshot is assembled into SDRAM in strips, then SWD or another reader can download it without reserving a second full frame in RAM_G. GPU display-list completion and texture lifetime are checked before a bank is reused; that rule is essential to avoiding torn transition frames.

Sources: [STM32F429IE specification](https://www.st.com/en/microcontrollers-microprocessors/stm32f429ie.html), [FT81x RAM and display controller](https://www.brtchip.com/wp-content/uploads/Support/Documentation/Datasheets/ICs/EVE/DS_FT81x.pdf), [resource manifest](../STM32/Resources/manifest.json), [NOR file code](../STM32/Middlewares/Noodoe/Storage/src/bootstrap_storage.c).
