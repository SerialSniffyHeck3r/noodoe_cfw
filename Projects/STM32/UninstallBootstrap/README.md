# Standalone stock restoration / CFW cleanup

`tools/build.ps1 -Profile UninstallBootstrap -Configuration Release` builds the
owned startup/linker profile without changing the Cube-generated project.
`tools/uninstall_build.py` also produces a manifest and the exact-image SHA pin
consumed by Product. Build this profile **before** Product and the release ZIP.

## Routes

- KEEP_CFW_DATA: existing Product/Gate recovery, unchanged binary/format. No
  new firmware transfer, FAT edits or container deletion. Emergency keys use it.
- ERASE_CFW_DATA: common NDCP updater target 3 receives a release-pinned 448KiB
  image into the existing stock staging range. Original BL installs it. The
  standalone screen starts with KEEP selected. UP/DOWN selects ERASE; release O,
  then hold it for two seconds to approve. No external resource/CFWREC is needed
  to decode or restore the embedded, SHA-verified stock V5.16 APP.

## Internal flash contract

| Range | Owner |
|---|---|
| 08000000–0800FFFF | Existing BL, installation metadata, factory data |
| 08010000–08010FFF | Standalone entry vectors + fixed UNB1 descriptor |
| 08011000–0801FFFF | 60KiB append-only cleanup journal |
| 08020000–0807FFFF | Standalone executable + compressed stock APP |

The linker rejects overlap. The binary includes FF in the journal range. The
standalone flash writer cannot erase any internal sector and can only program
the journal. The shared original installation-metadata writer is a separate
explicit operation used after staging and full verification. It preserves the
metadata sector's other bytes. BL executable and factory sector are not targets.

## Ownership and physical cleanup

The audited volume must have the existing FAT12 geometry, matching FAT mirrors,
complete ownership graph, no cross-links/orphans and no container past the safe
allocation boundary. Every selected file must have its expected size and an
independently validated format/UID identity, not just a matching name.

Targets: CFWA.DAT, CFWB.DAT, CFWBOOT.DAT, CFWREC.DAT, NOODOE.RSC,
CFWLOG.DAT, CFWCFG.DAT, CFWRIDE.DAT, CFWPIC.DAT and optional CFWTEXT.DAT.
The two image identity tails and recovery identity anchor the resource file,
whose content SHA is verified. Settings/ride/photo/log identities are checked.
Unknown formats stop deletion. LFN chains attached to targets also stop deletion.

Optional CFWTEXT.DAT has a reserved 4KiB identity at offset 0x1F000: CFJ1 v1,
purpose 4, payload length 8, UID, TXT1/version 1 at payload +64, CRC at 4088 and
commit-last at 4092. This is an ownership contract for a future cache writer;
an older or unmarked same-name file is **not** erased. Its last sector cannot
also be used for an A/B text payload. No previously shipped cache is silently
reclassified as owned. WALL*.JPG provenance is unavailable, so those files stay.

Before any NOR erasure, the 36KiB logical boot/FAT/root snapshot and cleanup plan
are read back from internal flash and committed. UID plus BL/factory SHA bind the
journal. Approval survives reset. Selected clusters are exclusively owned and
32KiB aligned, so all their 4KiB erase units contain no stock data. Each physical
erase is read back, including slack. Already erased sectors do not incur another
erase after a reset.

FAT12 shared nibbles and unrelated directory bytes are preserved. Each changed
metadata sector has an append-only intent before erase/program. A torn sector
is reconstructed from the saved snapshot and deletion plan. Both FAT copies and
the full final metadata are read back. Directory entries are deleted using E5;
their remaining metadata bytes are left unchanged. This is physical file-content
erasure and allocation release, not a forensic wipe of all historical metadata.

After approval there is no ordinary cancel or IGN sleep. An I/O error displays a
code and keeps the journal; fresh O for two seconds resets into resume. A damaged
journal stops processing. It never formats a damaged volume. Before approval,
an ownership/audit rejection still offers the embedded stock KEEP path. Once
metadata editing has started, it never boots stock with a partial FAT.

## Host contract

NDCP 0x93 reports support, actual UID, 448KiB size, transport version 7 and the
compiled image SHA. BEGIN/DATA/FINISH/COMMIT/RESET use the existing update service,
including negotiated 512/960-byte chunks and physical verification. Product's
normal 384KiB Gate update is unchanged. Exact SHA pinning prevents arbitrary APP
images from acquiring the original-BL installation path.

Host logs the intent before mutations, retains all evidence and tries to collect
device logs before sending the cleaner. Reconciliation only queries. An
uncertain COMMIT does not cause another COMMIT; same verified/committed images
do not need uploading again. Upload resume across a Bluetooth reconnect uses
the live updater transaction; a Product MCU reset before COMMIT can require a
fresh upload. No CFW container has been erased at that point.

The cleaner has no Bluetooth. The phone therefore shows device-owned work and
does not fabricate progress or claim deletion succeeded. Stock return confirmation
checks version, model/PCBA and Bluetooth address. It cannot observe whether the
user selected KEEP instead of ERASE locally. That limitation is recorded in the
host journal; successful stock detection alone is not proof of erasure.

## Retained reinstall

The app offers fresh start or retained data. Retained mode requires existing
valid settings/ride/photo containers before mutation. Its initial journal uses
GATE_RESTORE_RETAINED in transaction/reset_epoch with generation 1 and no previous
CFW slot. Trial reads compatible settings but cannot write them. Confirmation
consumes the marker without setting RESET_PENDING. Later normal updates keep
their existing once-only reset policy. Incompatible records are not interpreted
as zero or automatically formatted.

## Validation boundary

`tests/run.py --all-cuts` executes the real ARM cleanup core with modeled NOR and
internal flash. It interrupts each changed write before and during the operation,
restarts the core and compares final stock bytes/FAT/results. `tests/extra.py`
adds reported write errors and optional text/identity rejection.

These tests do not emulate actual flash electrical failure, IWDG hardware,
Bluetooth or the vendor BL's own install interruption behavior. The original BL
handoff remains a separate risk window. No attached bench or remote vehicle was
accessed for this implementation. Actual no-SWD two-route and physical power-cut
acceptance tests remain outstanding; elapsed emulator time is not install time.
