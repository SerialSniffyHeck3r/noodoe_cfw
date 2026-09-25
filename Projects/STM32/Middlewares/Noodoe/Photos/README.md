## Wallpaper overrides / import ABI2

`StorageService_FindWallpaper` reads root `WALL0.JPG`..`WALL2.JPG` first. Only a missing override falls back to the stock album. Decode errors are reported, never hidden by overwriting or deleting the file.

`PhotoImport.slot = PHOTO_IMPORT_WALLPAPER | slot` requests create-only installation. All published pixels stay immutable until reboot. The decoder validates the entire JPEG while discarding tile output, using its existing scratch/input memory. `FA_CREATE_NEW` and readback protect existing files. This initial FAT creation is not power-loss atomic: retain the verified preimages/journal and stable power during installation. No format or automatic retry/write occurs.

The old slot-only legacy import remains restricted to a missing original slot. Host tools must check ABI2 before using the new flag. `tools/tests/wallpaper_install` runs the real ARM FatFs/decoder on a current, provenance-checked donor image, audits every existing file and reserved tail, then reboots the emulated CPU to prove persistence. This is not hardware confirmation.

# Stock album JPEG service

PhotoService loads the original NOR album0..2 files without a USB host connection.
The PC-visible USB disk is a logical view of the same NOR; the CFW currently exposes
its existing USB CDC bridge, **not** a USB mass-storage copy of the stock disk.

## Public API and ownership

- PhotoService_RequestLoad(slot): nonblocking request, slots0..2, idempotent once ready.
- PhotoService_Get(slot,&image): acquire a completed immutable RGB565 descriptor.
- PhotoService_Process(): the single storage/I/O worker. At most8 JPEG MCUs per
  call after a bounded file read. Explicit import writes/readback also run here.
- WallpaperRuntime_Process(): UI owner registers ready images, source priority and
  central shading. Upper layers select with Wallpaper_SelectPhoto/SetEnabled.

Images are baseline JPEG (ChaN decoder supported formats), maximum480x480 and
128KiB compressed. Up to three460800-byte RGB565 buffers plus one128KiB input,
4KiB decode workspace and control structures are allocated once from external
SDRAM. No incomplete pixels are published, no image-sized internal SRAM or
FreeRTOS/LVGL heap allocation is used, and no per-frame JPEG decoding occurs.
Inputs/filesystem may fail without disturbing other ready slots. There is no
replacement/eviction API yet; ready pixels remain immutable until MCU reset.

PhotoDiagnostics g_photos: version1,72bytes, pending/active/ready_mask/failed_mask,
last slot/result/decoded MCU count, per-slot file length/width/height. Result ranges:
0x100 allocation/size;0x200+FRESULT discovery;0x300+FRESULT read;
0x400+JRESULT prepare;0x500+JRESULT decode;0x600 import policy;
0x601 CRC;0x602 existing/unreadable slot;0x700+FRESULT permanent create/readback.
Failures remain in diagnostics; they are not claims of a successful import.

## Stock layout and write boundary

The verified donor backup is analysis/2026-09-12-integrated-bringup/nor-full-backup-02.
Both128MiB A/B images have SHA256:
970af11e42f6c148c59f2ca99dec8fba1f9552a2def1405a43f50d3d1c98156f

Stock NOR has adjacent byte pairs reversed relative to the USB disk. StorageDisk
validates the BPB before selecting that conversion. Sector size4096; FAT16;
8sectors/cluster; root0x5000; data0x9000; logical volume ends0x7F80000. This overlaps
the CFW NVM journal reservation0x7F70000..0x7F7FFFF: stock mounting therefore skips
journal scanning/writing. Raw NOR backup and update staging never byte-swap.
Unknown media is not silently treated as writable. Stock formatting and normal
file/NVM writes are blocked. Product format is compiled out; the separate
Integrated formatter retains its existing explicit policy for non-stock media.

FAT long names are not required: discovery enumerates a single JPG using its8.3
alias. In this backup album0/1 are empty; album2 already contains the original
photo. The earlier USB extraction has all three user-selected images.

## Explicit restoration of missing photographs

The development SWD PhotoImport mailbox is version1,44bytes. Its magic appears
only after initialization; wait for idle sequence==ack and a nonzero advertised
SDRAM buffer. Host uploads the exact JPEG to that buffer and verifies readback,
then writes slot/length/CRC32/arm(BAK2) and **sequence last**. Only one outstanding
request is supported. The worker verifies CRC, empty slot and a complete JPEG
decode before entering a scoped storage write. It creates albumN/CFW.JPG with
FA_CREATE_NEW, syncs, reads all bytes back and releases the write scope. Existing
files cannot be overwritten. ack/result are published only when complete.

The low-level StorageService_CreateAlbumPhoto helper requires the storage owner
and backup token; empty-slot discovery is enforced by its PhotoService caller.
The helper itself also refuses an existing CFW.JPG. It is not a public arbitrary
file upload/format tool. Ordinary StorageService operations remain write-protected.

Run tools/stock_photo_import.py only with the current ELF, verified full A/B backup
and tools/tests/stock_photos/output/import-plan.json. The tool verifies live APP
identity, donor UID, actual IGN_ON, all FAT/directory/planned data regions before
writing; saves fresh copies of those regions; compares them with the ARM-emulated
plan; and verifies all regions after import. Any mismatch aborts before the next
write. It uses the service mailbox, never direct NOR programming. The backup token
is a development interlock, not authentication or proof by itself.

FAT create/sync is not power-fail atomic. Keep bench power stable during explicit
import. The full A/B backup and fresh affected-region snapshots are the recovery
material; no erase/format is an automatic retry. This is an initial missing-slot
restore, not a general replacement/update/BT transfer protocol.

## Decoder provenance / build

The original pinned LVGL9.5.0 src/libs/tjpgd/tjpgd.c is unchanged. photo_jpeg.c
includes it using the derived Photo_Jpeg.h ABI/config header (ChaN copyright kept):
RGB565,512byte input buffer, no scale, basic decode, no large clipping table.
The global LVGL JPEG widget integration stays disabled. The project runs decode
steps directly in the storage worker rather than blocking the GUI renderer.

Product photo/FAT and selected pure UI sources use -Oz/LTO; Debug keeps symbols.
services_build.py restores the per-file exceptions after Cube regeneration. Heap,
stack, vendor source and the APP/lower64KiB bootloader boundary are unchanged.

## Verification scope

Actual ARM tests: tools/tests/stock_photos/run.py, O0/Os/Oz-LTO. They use the real
FatFs, StorageDisk, StorageService, PhotoService and pinned ChaN decoder against
the full physical NOR backup. Tests cover read-only guards, CRC rejection without
writes, three480x480 images, create-only import/readback, no overwrites, and a fresh
CPU reload from the resulting emulated NOR. The generated exact NOR plan backs
the host tool. See analysis/2026-09-15-ign-wallpaper for current device status;
passing emulation does not imply actual flash/import completed.
