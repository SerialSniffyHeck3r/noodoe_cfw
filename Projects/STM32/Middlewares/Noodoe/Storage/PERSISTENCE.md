# Product CFW persistence

Implementation contract, 2026-09-18. Device installation and stock round-trip status
are recorded separately in `../../../../../analysis/2026-09-18-persistent-store/REPORT.md`.
Passing a host or ARM emulator test is not evidence of a successful hardware round trip.

## Ownership and protected regions

`App_Logic/Settings/app_persistence.c` owns the meaning of settings, trip snapshots,
maintenance origins and session checkpoints. `ConfigStore`, `CfwStore`, `CfwFiles`
and `PhotoStore` own serialization and asynchronous storage. `StorageTask` is the
only NOR writer. Graphics consumes immutable decoded image generations and does
not open files or write NOR. `BSP_NOR` checks each physical mutation against the
specific audited file's cluster map, including a second check after write-enable.

Product no longer writes the retired raw NVM interval `0x07F70000..0x07F7FFFF`.
The lower internal 64 KiB and external update staging `0x07F80000..0x07FFFFFF`
remain protected. `NOODOE.RSC`, original albums and `WALL*.JPG` are unchanged.
Runtime does not format, repair, create files, change timestamps or update FAT.

| File | Bytes | Runtime structure |
|---|---:|---|
| CFWCFG.DAT | 131072 | 32 sectors of 4096 bytes |
| CFWRIDE.DAT | 262144 | 64 sectors of 4096 bytes |
| CFWPIC.DAT | 1048576 | 65536-byte prefix and six 163840-byte banks |

The measured stock FAT12 geometry is checked explicitly: 4096-byte sectors,
eight sectors per cluster, two identical FAT copies, 512 root entries, data
start `0x9000`, volume end `0x07F80000`. Other geometries fail closed. A complete
directory/cluster ownership audit rejects cross-links, loops, free-in-chain,
orphans, mirror mismatch and incorrect sizes. Fragmented CFW files are supported.
Every granted cluster ends at or before `0x07F70000`. Names alone grant no writes:
UID, purpose, schema, CRC and complete record markers must also pass.

## Stable formats

All integers are explicitly little-endian. No C struct or SettingKey array is
written to disk. A common 4096-byte sector has magic CFJ1 (`0x314A4643`) at 0,
schema 1 at 4, purpose at 8, generation at 12, payload length at 16 and three
STM32 UID words at 20. Payload starts at 64, maximum 3968 bytes. CRC32 at 4088
covers bytes 0..4087; complete marker CMT1 (`0x31544D43`) is at 4092.
Unused bytes are FF. Purposes are CFG=1, RIDE=2, PIC identity=3, photo=16+slot.

The next journal sector is erased while the latest valid sector is preserved.
Header/payload/CRC are programmed, read physically and compared, then the
complete marker is programmed last and the entire sector is read again.
Only then is success published. Equal payloads do not cause an erase.
Generation comparison supports wrap. A bad latest CRC falls back to an older
complete record. An unsupported complete schema makes that file read-only.
No valid record produces an explicit error, not a fabricated zero history.

CFG payload: CFG1 magic, schema 1, then sorted `u16 field, u16 length, bytes`.
Unknown field IDs are retained. Deleted IDs must never be reused.

| Stable field IDs | Meaning |
|---|---|
| 0201 | Rider UTF-8, 0..48 bytes, no terminator on disk |
| 0202 | Six BT link keys, explicit version/generation and 24-byte entries |
| 0203 / 0204 | ELM binding / opaque retired GPS binding, never phone2 |
| 1001..1006 | Brightness mode, percent, auto bias, wallpaper enable, slot, center brightness |
| 1010..1015 | Time zone, units, scale, model, stopped threshold, language |
| 1020..1024 | Display hold minutes, BT hold minutes, three stage enables |
| 1030..1032 | Oil distance / ON hours / days interval |
| 1040..1042 | Belt distance / ON hours / days interval |
| 1050..1052 | Service distance / ON hours / days interval |

RIDE payload is 304 bytes: RID1/schema/date at 0/4/8; four 44-byte trip records
at 16 (u64 distance/moving/stopped/unknown, u32 max speed/valid/partial); three
20-byte maintenance origins at 192 (valid, ODO, day, u64 ON time); lifetime
IGN milliseconds at 256; reserve active at 268; refuel count/fuel known/fuel
baseline at 276/280/284; reserve distance at 288; lifetime-valid flag at 296.
Padding is zero. Stored ODO values are maintenance references, never a replacement
for the cluster's current ODO. Runtime ticks, animation/button/link states are absent.

Restore combines the saved base and this boot's observations exactly once.
TODAY is reconciled with the current date. Failed restoration leaves history
unknown even when fresh UART telemetry arrives. IGN counting starts before
storage loading; the saved total is added once when ready.

## Requests and scheduling

`AppSettings_GetResult` means applied to RAM/device. For persisted settings,
`AppSettings_GetSaveResult` with the same ID reports durable status. RTC and
other non-journal actions return CFW_ARGUMENT from the latter. The last eight
request results are retained; expired IDs are not reported as success.

`ConfigStore_Set/Get` operate on the RAM cache. `SetWords` preflights a batch
before changing any field. Saving is coalesced after 1 second idle or 2 seconds
of continuous changes. Failures remain visible while retries are attempted.
`SettingsService_RequestRiderName` updates the RAM name immediately in Product;
its diagnostics separately acknowledge the durable commit. Preferences, ELM
binding and six link keys use the same CFG cache. Keys are marked persisted only
after readback. Product `RequestProvision` and absolute `RequestOilUsage` return
DENIED: installation is host-only, and lifetime usage belongs to the live counter.

`RideStore_RequestCheckpoint(&id)` requests a post-action UI snapshot. Storage
cannot acknowledge a snapshot from before the action. SESSION_END, trip/service
reset and refuel request checkpoints; a raw IGN edge does not. Dirty ride data
starts periodic recording at 50 seconds, allowing a healthy commit before the
60-second deadline. Diagnostics expose last success and overdue/failure state.
Storage failure cannot be advertised as saved. Checkpoints preempt photo work
between bounded operations. Physical operation latency and FPS require hardware
measurement; emulator time is not NOR erase time.

`PhotoStore_RequestReplace(slot,jpeg,length,&id)` copies input to fixed SDRAM and
returns. Baseline JPEGs must be at most 128 KiB and at most 480x480. StorageTask
computes CRC and fully decodes before programming the inactive bank. It then
reads the actual NOR, compares, checks CRC and fully decodes again before commit.
Only a complete generation is exposed. Two decoded buffers per slot protect a
generation still borrowed by the renderer. Replacements reuse allocations.
`PhotoService_Retiring/Release` must be honored by CPU texture upload consumers.

Product SWD photo mailbox is ABI **3**: plain slots 0..2 invoke PhotoStore.
The old ABI2 `0x100` WALL-create flag is rejected. Use `tools/photo_replace.py`.
`tools/wallpaper_install.py` remains ABI2-only and must not be loosened to work
on Product. Mailbox completion means verified storage commit, not visual proof.

## Provisioning and recovery

1. Identify the running APP and donor. Obtain independent current full 128 MiB
   A/B backups with `tools/storage_swd_backup.py backup --elf ...`. Require
   identical hashes and a verified manifest. Keep the original copies.
2. `tools/cfw_storage_install.py plan --backup DIR --output NEW_DIR` audits the
   complete FAT, hashes existing files, checks reserve exclusion and selects
   free clusters. Same-name collisions, insufficient space and corruption abort.
   It records every changed sector's before/after bytes and the exact payload.
3. Read-only legacy migration accepts only UID/layout/CRC-valid SET1 v1/v2/v3
   records. Unsupported versions abort. Retired raw addresses are never changed.
   Initial photos are read from WALL0..2 first, otherwise the original albums.
4. Install the verified Product APP using the APP-only installer and keep the
   lower 64 KiB identical. With CFW files missing, persistence reports missing
   and ordinary writes remain disabled; it does not create defaults on media.
5. Execute the planned installer against the exact APP/UID with IGN ON. It
   checks live preimages, SDRAM payload readback, bounded candidate clusters,
   purpose records and metadata again. Payload is written/read back completely
   before FAT/root publication. All planned changed ranges are read back.
6. Obtain a new full A/B backup and compare against the planned image, preserving
   every original file and reserved byte. Explicitly reboot to audit and restore.
   Then verify runtime saving, photographs, IGN/STOP and stock V5.16 round trip.

Initial FAT creation is **not power-loss atomic**. A partial first installation
requires inspection of the saved sector journal and fresh readback; never
automatically retry, format or repair. The installer does not overwrite an
existing CFW filename. Normal journal/photo updates need no FAT changes.

The maintenance lease `g_cfw_quiesce` lets the backup tool stop new physical
transactions and drain active writers before its A/B snapshots. RAM changes may
remain dirty. Request=1/ack=1 is a pause, not a save. The tool releases its own
lease in finally and records a failure if it cannot resume. Do not leave it held
after a disconnected debugger session. STOP also waits for pending writes.

Stock APP replacement leaves CFW files in place. Stock preservation during
normal V5.16 use, including photo/save/USB operations, must be measured on the
real device. Factory reset/PC formatting is outside the preservation guarantee.
