# NOR backup and gated storage service

An unprovisioned startup exposes the entire **134,217,728-byte (128MiB)** SPI5 NOR through USB CDC and performs only reads. The earlier stock USB volume was only127.5MiB and omitted the final512KiB. Explicit host authorization after verified full A/B backups can unlock filesystem/NVM writes and separately request formatting. Later valid Settings provisioning restores ordinary file/key persistence without needing a PC on every boot. Formatting always requires a fresh explicit host gate. USB does not export mass storage or arbitrary SPI commands. OTA APP staging has an independent internal capability; BL staging remains unwriteable through these APIs.

Initialization order: generated GPIO/DMA/SPI5/OTG_HS PCD → `StorageBackup_Init()` in a task. Forward HAL SPI TxRx completion and error callbacks to `BSP_NOR_OnTxRxComplete` and `BSP_NOR_OnError`. DMA2 streams3/4 priority4 callbacks only publish flags. Call `StorageBackup_Process()` from a single task at least every5ms during transfer. The service never calls graphics, changes task ownership, or starts another task itself.

`StorageBackup_Init()` checks JEDEC `C2 20 1B`, status WIP=0, and compares64byte polling/DMA reads at0,0x00FFFFE0,0x07FFFFC0. The middle sample crosses16MiB. Dedicated READ13 uses four address bytes even below16MiB. A DMA halfword is reordered into the original byte stream after reception. Independent full host dumps must still match: successful initialization does not prove every NOR byte.

`g_storage_backup.result`:0 ready; otherwise BSP_NOR status,0x101 polling comparison read failed,0x102 DMA read failed,0x103 byte comparison mismatch; bit0x10000 indicates USB startup failure. USB starts even when NOR verification fails so HELLO can report the status. No READ request is served unless `transport_verified=1`.

## Binary CDC protocol, version1

All integers are little-endian. Send exactly one request at a time. COM-port baud settings are virtual and do not change SPI/UART clocks. Request framing survives USB packet splits and bounded noise before the magic.

Request20bytes, Python format `<4sBBHIII`:

| Offset | Field |
|---:|---|
|0|`NDRQ` magic,4bytes|
|4|Version1,u8|
|5|Opcode:u8;1 HELLO,2 READ,3 CANCEL|
|6|Reserved/flags:u16, must0|
|8|Host sequence:u32|
|12|NOR byte address:u32|
|16|Requested byte count:u32|

Response24byte header followed by payload, Python format `<4sBBHIIII`:

|Offset|Field|
|---:|---|
|0|`NDRS` magic|
|4|Version1|
|5|Opcode|
|6|Status:u16|
|8|Request sequence:u32|
|12|This payload's NOR byte address:u32|
|16|Payload bytes:u32|
|20|CRC32 of payload:u32, exactly `zlib.crc32(payload)`|

Status0 success,1 invalid request,2 NOR not ready,3 another stream active,4 read error,5 unsupported opcode. Empty payload CRC is0.

HELLO returns opcode1,status0,48byte payload containing12u32 values:

`jedec_id, capacity_bytes, spi_clock_hz, initialization_result, max_chunk(4096), capabilities(bit0=raw export,bit1=gated storage control), filesystem_bytes(0x07F70000), nvm_address(0x07F70000), nvm_bytes(65536), bl_staging_address(0x07F80000), app_staging_address(0x07F90000), app_staging_bytes(0x70000)`.

READ serves consecutive opcode2 frames, each at most4096bytes, preserving sequence and exact byte address. The terminal frame is opcode0x82,status0,length0,address=request_end,CRC0. Check every CRC, sequence, address, length and final end marker; do not accept file size alone. An error returns opcode2 with nonzero status and stops the stream. A new connection or USB reset cancels outstanding transfers. CANCEL replies opcode3,length0 and stops further blocks; it does not undo bytes already delivered.

A full backup request is `(NDRQ,1,2,0,sequence,0,0x08000000)`. Store it into a new file, compute SHA256, and repeat the entire read into another new file. `tools/storage_backup.py backup --port COMx --directory <new directory>` performs fresh A/B reads, validates every frame, and compares the whole files. Failed partials and raw data are preserved. The manifest records actual USB serial/MCU UID when available.

## Additive explicit control opcodes

Request framing stays20bytes; there are no trailing request payloads.

|Opcode|Address / length fields|Response payload|
|---|---|---|
|4 IDENTITY|0 /0|32bytes:three LE UID words, then five actual internal boot metadata words|
|0x10 UNLOCK|0x07F70000 /0x42414B32|4byte BSP_NOR result|
|0x11 LOCK|0 /0|empty|
|0x12 FORMAT|0x07F70000 /0x464D5431|4byte FatFs result|

Unlock requires a verified read transport, stock metadata word0=`0x000E0000` and pending CRC word4=0. Successful older updates can leave slot/length values, so those are not a pending test. An erased/unknown pending CRC is refused. Host `unlock --manifest ...` and `format --manifest ...` freshly rehash/recompare the complete A/B files, require original streaming SHA/DONE proof, and match the current MCU UID. Firmware's explicit token is an accidental-call guard, not host authentication: proof of local backup files occurs in the host tool. USB reset immediately revokes this host gate, including while a worker waits for a physical operation. An already submitted page/erase cannot be undone. The host `format` command relocks after its result. No format/provisioning runs automatically merely because the firmware contains these APIs.

The BSP separates `storage_unlocked` (host permission) from `runtime_write_permit` (validated Settings provisioning). `BSP_NOR_IsStorageUnlocked()` reports only the former; ordinary filesystem/NVM code uses `BSP_NOR_CanWriteStorage()`. `BSP_NOR_EnableProvisionedStorage(BSP_NOR_PROVISIONED_TOKEN)` is called only after Settings validates its persistent marker's UID, schema, exact partition boundaries and CRC. A mount by itself grants nothing. Both flags start0 at NOR initialization. USB reset/LOCK leaves the runtime permit intact. MCU reset requires validating the marker again.

`StorageService_Format()` holds the service mutex across `BSP_NOR_BeginFormat()` and `EndFormat()`. During that scope every physical filesystem/NVM write requires the host gate, even on a provisioned unit. The scope remains active when USB reset clears the host gate; otherwise it would incorrectly fall back to runtime permission halfway through format. The same scope can protect an explicit provisioning journal write. After the scope ends, ordinary provisioned persistence resumes. OTA authorization and the protected BL-stage range are unchanged.

## Filesystem, NVM and OTA boundaries

`StorageService.h` provides mount, format, read/write/stat/remove/mkdir and NVM blob APIs. `StorageService_Init()` mounts/scans only; FR_NO_FILESYSTEM never causes automatic format. FatFs R0.12c is the unchanged package from the same pinned CubeF4 tag, with project config in Middlewares/Noodoe/Storage/inc. Logical sectors are4KiB, filesystem is a FAT16 superfloppy in `[0,0x07F70000)`, filenames currently8.3. No wear levelling or FAT power-loss transaction guarantee is claimed. Metadata/asset writes should remain infrequent. A service mutex serializes its public calls; competing direct NOR callers receive BUSY and must coordinate/retry explicitly.

NVM occupies `[0x07F70000,0x07F80000)` as sixteen4KiB rotating journal slots. Each record has magic,sequence,length,CRC,inverse length, up to2048payload bytes, and a final commit word at sector+4092. CRC covers sequence,length,payload. Header/payload are programmed with independent readback before commit. Interrupted next-record erase/program leaves the previous committed record available. This protects the NVM blob, not arbitrary FAT writes.

`BSP_NOR_OTAEnable(transaction_id)`, `OTAProgram`, `OTAErase4K`, `OTADisable` are internal C APIs restricted to APP staging `[0x07F90000,0x08000000)`. OTA authorization is separate from USB filesystem unlock and survives a USB reconnect; a device reset locks it. Page writes accept1..256bytes within one256byte page; erase is4KiB aligned. Dedicated four-byte opcodes12/21, WEL/WIP and readback are checked. No API can write `[0x07F80000,0x07F90000)` BL staging. Actual update verification/commit belongs to Middlewares/Noodoe/Update.

Host protocol tests: `tools/tests/test_storage_backup.py` (10cases). Actual journal C plus extracted production range guard runs under ARM Unicorn at O0/Os: `tools/tests/storage_journal_host/run.py`,155assertions each, including partial operation interruptions, circular journal recovery and the service-level format gate. `tools/tests/nor_gate_host/run.py` compiles the actual BSP against real STM32 HAL headers;35assertions at each optimization check permit separation, USB reset while WREN is in progress, protected staging boundaries and initialization revocation. These are software tests; actual chip program/erase/format remains unverified until root performs the authorized post-backup hardware stage.

## Source evidence and scope

- Stock SPI5 PF7/8/9, PF6 CS, mode0,42MHz, command8bit/bulk16bit and DMA MINC directions: `analysis/2026-09-12-full-ioc-map/display-audit.md`.
- Stock USB embedded Full-Speed PHY on OTG_HS PB14/15, DMA off and IRQ6: `analysis/2026-09-12-full-ioc-map/communications-audit.md`.
- [Macronix MX66L1G45G v1.5](https://www.macronix.com/Lists/Datasheet/Attachments/8734/MX66L1G45G%2C%203V%2C%201Gb%2C%20v1.5.pdf), dedicated4byte READ13. Chip family is supported by the stock JEDEC comparison; package marking has not been independently read here.
- Official STM32CubeF4 v1.28.3 commit `94cae6e83f00e276a11957e7833c01ac3d0bd7af`; unchanged USB Core/CDC files and original license in `Middlewares/Third_Party/USB_DEVICE`, with verified file hashes in `UPSTREAM.json`.

Standalone compilation is not hardware validation. Actual device ID, USB enumeration, polling/DMA comparison and independent128MiB host dumps must be reported from root's integration evidence.
