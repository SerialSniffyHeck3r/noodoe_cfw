> 2026-09-21: CFWREC v2 및 원본 BL 읽기 NDCP 0x60은 [BL_COMPATIBILITY.md](../Update/BL_COMPATIBILITY.md)를 따른다. v1 고정 BL0.14 설명과 구분한다.

# Bootstrap maintenance protocol (NDCP hexadecimal opcodes 50–59)

Only the storage owner calls BootstrapStorage methods. A transport adapter must
verify its paired/encrypted peer and physical maintenance intent before calling
SetSession(epoch,1); identifiers and backup tokens are not authentication.
Changing epoch or disconnecting immediately revokes the BSP create capability.
This protocol is not enabled by Product's ordinary NDCP handler.

Integers are little endian. Every reply starts with eight u32 words:
`result,state,error,position,filePhysicalBase,fileBytes,backupPosition,backupValid`.
Additional bytes follow this32-byte prefix. State values are the BS_* enum in
Bootstrap_Storage.h. Error result0 means request accepted, not durable completion.

|Opcode|Request|Additional response|
|---|---|---|
|50|u32 pass:0=A,1=B|none|
|51|u32 exactNextOffset,u32 bytes1..480|raw physical NOR bytes|
|52|u32 kind,u32 bytes,SHA256(file),SHA256(backup)|none|
|53|u32 exactNextOffset,data1..480|none|
|54|empty|queue hash/content/FAT validation|
|55|SHA256(file)|queue installation from PREPARED only|
|56|empty|32-byte agreed complete backup SHA|
|56|u32 metadataOffset0..36863, PREPARED only|up to480 logical after-metadata bytes|
|57|empty|revoke and close|
|58|empty|Bootstrap runtime identity: version,role,UID3,APP SHA32,immutable BL SHA32 (84 bytes; unlike the storage replies, no storage prefix)|
|59|u32 kind|queue read-only existing-file validation|

Two complete sequential reads of128MiB are required within the same authenticated
epoch. Firmware calculates SHA independently on both passes and compares. The
host must save both streams, independently compare bytes and SHA, preserve the
manifest, then echo their SHA at52. Firmware cannot prove host files were saved.
These snapshots are after initial Bootstrap OTA, not pristine pre-install flash.

File kinds:0=NOODOE.RSC1MiB,1=CFWCFG.DAT128KiB,2=CFWRIDE.DAT256KiB,
3=CFWPIC.DAT1MiB,4=CFWREC.DAT512KiB,5=CFWA.DAT512KiB,
6=CFWB.DAT512KiB,7=CFWBOOT.DAT64KiB. No other filename, size or raw write exists.
Creation is refused when the target name already exists. Full FAT ownership,
matching mirrors, clean chains and reserved-tail exclusions precede writes.
The device selects free contiguous clusters. Host compares PREPARED metadata and
placement against its offline plan and saves before/after sectors before55.

Process performs bounded writes (one erase/page/read per call). Entire payload is
physically reread and SHA checked before FAT/root publication. Unchanged metadata
sectors are skipped. Initial publication is not power-loss atomic: failure or
disconnect can leave orphan allocation or one updated FAT mirror. Never retry
or repair automatically. Keep the original complete backups and changed-sector
journal. Subsequent file creates preserve prior completed containers.

CFWREC header: NCR1@0,format1@4,header4096@8,file524288@12,UID3@16,
stockversion0x00100005@28,APP458752@32,BL0x000E0000@36,pinnedAPP SHA@40.
Bytes72..4087 are FF,CRC32@4088,commit CMT1@4092. Canonical APP at4096.
Source is pinned V5.16 APP SHA162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf.
Remainder is FF. This file has no ordinary writable capability.

Existing files are never recreated. `BootstrapStorage_Inspect(kind)` audits the
whole FAT ownership graph and validates the selected file's identity/content.
It may run read-only before authentication during bootstrap. Five verified file
bits permit `BootstrapStorage_FilesReady()` to return true; the public return is
a boolean, not the bitmask. RSC checks the compiled required resource SHA; REC
checks the fixed approved APP SHA; CFW journals/photo headers bind to MCU UID.
Photo banks receive CRC validation here; complete JPEG decoding is performed by
the host migration validator and Product's photo loader, not by Bootstrap.

## Physically attached bench transport

`g_bootstrap_storage_bench` is a 128-byte main-SRAM descriptor. Only the matching
Bootstrap ELF authorizes its use. `tools/bootstrap_recovery_install.py` performs
the host proof: exact live APP, direct UID/BL metadata, freshly reread full A/B
files, live FAT/selected-free-extent preimages, PREPARED plan comparison and
physical postimage. The default `--kind recovery` accepts only the pinned stock
APP and creates CFWREC.DAT. Explicit `gate-a`, `gate-b` and `gate-journal` kinds
accept canonical UID-bound initial containers, additionally derived from actual
Product Release/Debug ELFs that pass the unchanged memory budgets. JSON budget
claims are not accepted. Failed budgets prevent even a provisioning plan being
published. It cannot replace an existing name or choose an arbitrary write range.

Offsets: magic BSW1@0, ABI1@4, arena@8,capacity@12,command@16,BAK2 intent@20,
UID3@24,backup SHA@36,file SHA@68,sequence@100,ack@104,state@108,error@112,
selected physical address@116,file bytes@120,planned logical metadata pointer@124.
Commands1=PREPARE_REC,2=COMMIT,3=CANCEL,4=HASH_WHOLE_NOR,
5=PREPARE_GATE_A,6=PREPARE_GATE_B,7=PREPARE_GATE_JOURNAL.
Host writes fields then a new sequence last;
device snapshots between sequence reads and publishes ack after status. One
outstanding command is allowed, with explicit cancellation as the exception.
The backup token is not proof that host files exist; the trusted physical host
must perform that proof. No radio request can enter this SWD-only lease.

Runtime calls BenchProcess on the sole storage owner, keeps ordinary radio
SetSession from revoking an active bench lease, and serializes StorageSWD raw
reads with mutation work. Initial missing-file inspections must finish before
the host uploads the arena. The 8MiB StorageSWD buffer is a separate allocation.

Command4 is read-only and does not grant any erase/program capability. It enters
BS_HASHING(11), reads physical NOR [0,128MiB) in 4096-byte calls, then publishes
its SHA-256 at offset68 and BS_SAVED before ack. The reported bytes field is the
fixed 128MiB span, not progress. A fresh command4 performs an independent sweep.
IGN/session loss or command3 stops at the next bounded call; an incomplete hash
is not a valid result. Queued radio requests/replies block a physical claim, and
active physical ownership rejects subsequent radio storage requests. Runtime
update may not acquire storage while hashing.

The host must first observe the completion ack, discard that observation, then
obtain two identical completed mailbox snapshots: a segmented SWD read could
otherwise combine an old digest with a newly published ack. The verifier saves
both independent device hashes and compares each with the complete audited
postimage. These are physical whole-NOR hash checks, not downloaded A/B backups.
The original complete A/B and explicit journal-continuity evidence remain intact.
Timeout cleanup only attempts explicit cancellation, never a reset or reflash.

## Independent-gate provisioning constraints

GIM1 image headers and GBJ1 initial journals are defined by
`RecoveryGate/include/gate_abi.h` and generated by `tools/gate_bundle.py`. Each image
container has a separate immutable GID1 identity record at offset0x7F000;
ordinary updates must preserve it even when the inactive image header is erased.
All image bytes are a padded384KiB Product linked at0x08020000. The initial
journal confirms slotA; slotB is an independently validated copy. The recovery
gate itself is not placed inside these Product images.

The existing five-file `FilesReady()` boolean is a base-resource prerequisite,
not proof of the additional gate image/journal set. Callers must separately
validate all three gate files and the intended image/resource relationship.

The SWD installer processes one named file per invocation. Creating a file
changes FAT and root preimages; the next invocation must use a newly proven
baseline. Do not reuse the previous A/B snapshot as though its FAT were current.
The CFWRIDE continuity exception applies only to an audited committed ride
checkpoint, not to arbitrary FAT changes or another provisioning transaction.

The existing Bluetooth/Android updater targets legacy APP target0. A layout2
Product requires target2 and an initial gate+Product package; the legacy client
must not be used for either step. Gate-aware radio provisioning and installation
remain unverified. The host kind implementation and ARM fixtures do not claim
an end-to-end wireless installation or a successful hardware install.
# Bootstrap diagnostic menu (2026-09-21)

Local entry is Bluetooth test / Install CFW / Back to stock. The test entry
accepts only identity/status opcodes `01`, `58`, `5A`, `45`; other requests
return `BS_DENIED` before storage/update dispatch. Pairing does not authorize
installation. A separate local Install CFW entry grants the existing bounded
maintenance lease; the existing transaction-specific install confirmation is
still required. Returning to the root menu revokes the lease.

Read-only `5A` keeps its 36-byte/schema1 response: result, schema, UI state,
phase, position, total, error, flags, pairing-entry milliseconds. States0..8
remain CHECK/READY/CONNECT/WORK/INSTALL_READY/INSTALL/PAUSED/RECOVERY/ERROR.
BT_TEST is appended as9. It never implies installation approval, controller
health, encrypted SPP, or successful command exchange; these are separate facts.

## Scoped resource migration (2026-09-22)

Capability `0x86` flags bit5 (`0x20`) advertises resource reseed on Bootstrap.
It is distinct from Product's bit4 resource-update interface. Older Bootstrap
must not receive this request; return to stock and install the matching new
Bootstrap before retrying a package with a different resource ID.

Scoped-v2 `0x88` additionally accepts kind0 (`NOODOE.RSC`), only after FAT
ownership/extent audit and successful stock recovery-file verification. The
104-byte request is kind, new-slot bytes (exactly512KiB), new-slot SHA256,
scope proof, and SHA256 of the complete physically-read old1MiB container
after byte-pair decoding. The phone durably saves that preimage before sending
the request. It never asks to overwrite an arbitrary NOR range.

The device reads and hashes the old1MiB container again, selects one valid
header to preserve, then verifies that slot's complete table/body SHA before
any erase. A corrupt selected body is rejected rather than silently discarded.
The new slot must match this Bootstrap's compiled required resource ID. The
target is exactly the other512KiB half; PREPARED returns its address and size.
The target preimage and unchanged FAT are rechecked before publication. Only
the target half receives a write lease. Body and incomplete header precede a
complete physical readback; commit marker `0x434d5431` is programmed last, then
the full target is read and hashed again. The preserved slot, FAT, recovery
image and other files are untouched by this migration. Existing SWD resource
provisioning retains its older valid-A/erased-B restriction.

The resource commit marker differs from the `0x31544d43` journal marker. Android
must validate the resource marker when selecting a slot from a bundle, both for
Bootstrap migration and ordinary Product resource updates. A stopped operation
can be retried only after obtaining a new audited physical preimage; RAM upload
progress is not a durable checkpoint.
