# Memory map and where the assets live

These are the STM32F429IE-class Noodoe I examined and the current CFW linker contract. Noodoe seems to have changed little over the years; several stock images have similar layouts. Still, addresses beginning 0x0800… or 0x2000… are MCU addresses, whereas the 0x07F… NOR values are offsets *inside the external chip*. Mixing those up makes for a very bad afternoon. [한국어](memory-map.ko.md)

## Internal flash and RAM

| Address | Size | Purpose |
|---|---:|---|
| 0x08000000–0x0800FFFF | 64 KiB | Stock resident boot/install code and preserved factory data: device number, vehicle model, board revision and related identifiers. KYMCO's server maps the device number to the VIN. |
| 0x08010000–0x0801FFFF | 64 KiB | Independent RecoveryGate, in the first sector of the original APP area. |
| 0x08020000–0x0807FFFF | 384 KiB | Running Product CFW; ordinary updates replace this region. |
| 0x20000000–0x2002FFFF | 192 KiB | Ordinary SRAM: DMA, 48 KiB FreeRTOS heap, stacks, queues and diagnostics. The final 256 bytes are the Gate/Product mailbox. |
| 0x10000000–0x1000FFFF | 64 KiB | CCM, including a guarded 48 KiB LVGL pool. |

Gate plus Product occupy the 448 KiB internal APP area. That's the whole trick: the executable Product is **one** 384 KiB region, not two parallel internal partitions.

## External NOR: named files inside stock FAT

The board has 128 MiB NOR and 64 MiB SDRAM. NOR survives power loss; SDRAM is working memory. Stock NOR already holds FAT files and an updater staging area. CFW checks free-cluster ownership, both FAT copies and reserved-space overlap before creating its own fixed-size named files. NOR offset **0x07F80000–0x07FFFFFF** remains reserved for the stock updater.

| CFW-owned file | Size | Contents |
|---|---:|---|
| NOODOE.RSC | 1 MiB | Font bitmaps and CC256x patches, in two 512 KiB verification/activation banks. |
| CFWCFG.DAT | 128 KiB | Versioned settings and pairing journal. |
| CFWRIDE.DAT | 256 KiB | Trip and maintenance checkpoints, ignition hours. |
| CFWPIC.DAT | 1 MiB | Three photos with replacement-safe space for each. |
| CFWREC.DAT | 512 KiB | The exact stock APP used for button recovery. |
| CFWA.DAT / CFWB.DAT | 512 KiB each | Product candidates and previous working build, including header, 384 KiB body and identity record. |
| CFWBOOT.DAT | 64 KiB | Install/boot-state journal. |
| CFWLOG.DAT | 256 KiB | Bounded device event log. |
| CFWTEXT.DAT | Version-dependent | Name/text cache; presence and format are checked during capability and file audits. |

A/B are two **external NOR Product images**, not an excuse to claim the STM32 suddenly grew two executable 384 KiB partitions. Gate verifies one and copies it to 0x08020000. Stock photos and files remain separate. Restoring only the stock APP usually leaves CFW's FAT files behind; removing the data is a different operation.

Character mapping and metrics remain internal, while large font bitmaps and Bluetooth patches are checked and loaded from the external asset file to SDRAM. Text rendering doesn't read NOR on every glyph. SDRAM also holds decoded pictures, assets, a 480×480 RGB565 capture (460,800 bytes), and fixed-purpose installation buffers, allocated once and reused.

## EVE graphics RAM

The FT81x has 1 MiB RAM_G. Roughly 96 KiB goes to fonts, icons and alignment; two photo banks get 450 KiB each; a 480×24-line partial-capture buffer gets about 22.5 KiB. Two banks let one picture fade into another without uploading over the texture currently on screen. Full RGB565 captures are assembled in SDRAM from 24-line slices. A referenced GPU region is not reused until display-list swap actually finishes. The screen made that rule painfully clear.

Evidence lives in the [linker files](../STM32/Linker), [Gate ABI](../STM32/RecoveryGate/include/gate_abi.h), [memory contract](../STM32/MEMORY.md), [asset manifest](../STM32/Resources/manifest.json), [NOR storage code](../STM32/Middlewares/Noodoe/Storage/src/bootstrap_storage.c) and [FT81x datasheet](https://www.brtchip.com/wp-content/uploads/Support/Documentation/Datasheets/ICs/EVE/DS_FT81x.pdf).
