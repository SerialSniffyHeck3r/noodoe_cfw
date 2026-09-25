# Stock updater → Bootstrap → CFW → approved stock recovery

Completed bench execution 2026-09-20. **Both donor recovery paths installed the
pinned stock APP through the resident bootloader. Final Product CFW is installed;
all 22 final runtime checks, selected-region persistence checks and the actual
480×480 EVE capture passed. Recovery was triggered through SWD, not phone or
buttons. The healthy motorcycle remains unsupported by this bench bundle.**

## Compatibility boundary

The approved original donor is **V5.16 / BL0.14 / PCBA `sr0601`**. The healthy
motorcycle's existing DeviceInfo capture instead reports **V5.16 / BL0.15 /
PCBA `SR0701`**. Shared model `SAA1AA(KR)` and HW0 do not prove compatible boards.
HW0 is a literal field emitted by this stock APP, not a detailed board identity.

- Immutable resident code: `0x08000000..0x08007FFF`, SHA256
  `f8b379c3fac078a8e01d8b6c36fccc5bb5ea5852a0db008822ef6871e0f38df5`.
- Pinned stock recovery APP: `0x08010000..0x0807FFFF`, 458,752 bytes, SHA256
  `162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf`.
- Resident code is not replaced. Installation metadata uses the existing
  preservation/readback/CRC-last writer; a whole512KiB donor image is never an
  OTA APP payload.
- `boot-identity-audit.md` and `stock-identity-observed-boot015/identity.json`
  preserve the original byte/disassembly/actual packet evidence. No approved
  BL0.15 binary was found by the recorded local inventory.
- The current client/bundle policy rejects BL0.15 and checks PCBA case exactly.
  Successful donor trials cannot authorize the healthy motorcycle installation.

## Implemented paths

1. An isolated Android installer or Windows reference client reads stock identity
   without normal autosync. Identity collection needs no firmware bundle.
2. Stock transfer uses its actual `0A BEGIN → 0B START → 0D DATA → 0B TERMINATE →
   0A DONE` phases. Accepted transfer is not boot confirmation. Permanent12V
   remains present; the stock APP performs the IGN OFF handoff to its own BL.
3. The Bootstrap APP carries the pinned compressed stock APP internally and uses
   EVE ROM text rather than Product fonts/LVGL/installed resources. Its maintenance
   service requires physical intent, paired/encrypted SPP and same-session backup
   proof. Only five fixed files are provisioned; no generic raw write service is
   exposed over Bluetooth.
4. Host/device independently verify full NOR A/B, FAT ownership, proposed
   allocation/metadata, file bodies and final postimage. Local `CFWREC.DAT` is
   prepared before Product installation. Resource compatibility and all required
   files gate the Product commit.
5. Product/Bootstrap update identity reads actual padded448KiB APP and32KiB BL
   hashes. Journals distinguish intent, acceptance, uncertain replies and actual
   boot identity; uncertain COMMIT is never automatically replayed.
6. Local approved-stock recovery validates the complete source, writes only the
   existing APP staging area, independently rereads it, then explicitly commits
   through the preserved metadata path. Product remote `0x48` and the physical
   DOWN+ENTER path share the early recovery implementation.

Source ownership/contracts: `App_Logic/Bootstrap`, `App_Logic/Recovery`,
`App_Logic/Runtime/RuntimeUpdate`, `Middlewares/Noodoe/Recovery`,
`Middlewares/Noodoe/Storage/BOOTSTRAP_WIRE.md`, `Recovery_Core`, and the restricted
BSP NOR adapters. Detail and failure boundaries: `RECOVERY-CORE.md`.

## Client and packaging evidence

- Android: private installer Activity + foreground service transport ownership,
  durable journal, disk-streamed backup/provisioning, evidence export, same-link
  update/recovery and exact model/PCBA/BL/APP/UID checks.
- Windows: `tools/stock_installer.py`, `stock_recovery_client.py`,
  `bootstrap_storage.py`; network/hardware require explicit execution arguments.
- `tools/recovery_bundle.py` validates the original donor pins, independent ELF
  layout/export against actual objcopy and BIN, embedded stock decompression,
  resource CRC/SHA and both APP requirement records.
- The current frozen artifact is [bench-bundle-final/recovery.zip](bench-bundle-final/recovery.zip),
  marked `target.scope=bench-only`, based on factory/code reconstruction rather
  than a radio query. Both frontends reject its wireless installation. The
  actual Android importer checked the final ZIP and verified that rejection in
  `android-bench-bundle-final-import.log`. The older `bench-bundle` is preserved
  as historical evidence and is superseded by this final artifact.
- Final ZIP SHA256:
  `17cddd795658853c0857e9698e4a3f808768d44bbffaf1b744b86e6c627649c4`;
  982,184 bytes. Exact ELF/BIN/resource/donor checks and reconstruction basis:
  [bench-bundle-final/audit.json](bench-bundle-final/audit.json).
- No healthy-vehicle installable bundle was generated. Its explicit rejection is
  recorded in `healthy-target-audit/audit.json`.

Details: `ANDROID_WINDOWS_CLIENTS.md`, `OpenNoodoe/Android/docs/CFW_INSTALLER.md`.
Exact client/APK/bundle hashes: `client-sha256.json`. APK is unchanged by final
artifact repackaging.

## Offline tests and archived builds

| Evidence | Recorded result | Boundary |
|---|---:|---|
| Android unit tests, lintDebug, assembleDebug |58 tests, PASS|No radio/device installation|
| Windows stock/recovery clients |9 tests, PASS|Mock transport|
| Bundle tests |6 tests, PASS|Offline artifact/identity gates|
| Recovery core |36 checks each O0/Os, PASS|ARM emulation; memory flash adapter|
| Protocol services |12,410 assertions each O0/Os, PASS|Mock peripheral boundaries|
| Runtime update |45,529 assertions each O0/Os, PASS|Full APP byte comparison in emulator|
| Bootstrap update gate |45,542 assertions each O0/Os, PASS|RCSQ/file gate and typed stock|
| App recovery/reset/identity |17 explicit checks each O0/Os plus hash/reset comparisons, PASS|AIRCR interception, no physical reset proof|
| Archived storage tests |307 O0 /6,457 Os assertions, PASS|Source hashes in runner results; latest storage fix must be revalidated|
| Final full-NOR hash ARM tests |6 cases /60 assertions, PASS|`real-nor-hash-arm-validation-v2/results.json`; captured NOR and computed postimage, no physical device|
| Fast hash fixture |4 cases /46 assertions, PASS|`real-nor-hash-small-arm-validation/results.json`; ownership and final-chunk handling, offline ARM|

Source-specific test results are authoritative; a pass before a subsequent change
does not certify the changed binary. Recovery test references are listed in
`RECOVERY-CORE.md`; client logs/results are in this folder. Real-NOR snapshots
used by emulator tests remain offline test inputs, not physical write trials.

Archived build matrix from `builds/*/memory-report.json`:

| Profile | APP used/free (bytes) | SRAM free | CCM free |
|---|---:|---:|---:|
| Product Release |390,516 /68,236|35,448|16,320|
| Product Debug |410,080 /48,672|35,448|16,320|
| Integrated Release |415,272 /43,480|20,864|65,536|
| Graphics Release |432,452 /26,300|45,576|65,536|

The archived Product Release canonical448KiB SHA is
`97059caeab949bff672af6d10859b0dffbfb59af1196c339205d93fb2b28506b`.
Root froze `bootstrap-hash-build/bootstrap.elf` and `bootstrap.bin` before final
packaging. The package checked independent ELF reconstruction, actual GNU
objcopy, exact supplied BIN, embedded stock recovery decompression/SHA and the
resource requirement. Bootstrap is433,516 bytes, unpadded SHA256
`1f4c8fa2a3285ab2572ded7dede6b6de7ac6743e4c01865952146116ed1f54ef`,
canonical448KiB SHA256
`ce0ca75b76bfb5f7b15de21a05820ac4799ccfa49b5d0cf970058c964f4c9cfd`.
Root reran `validate_image` against the frozen ELF; both `app.bin` and
`bootstrap.bin` now contain the same433,516 verified bytes.
Physical trial results and source-specific post-fix checks remain the root
agent's record below; artifact identity does not establish hardware success.

## Physical donor trial — Product returned and final checks completed

This section deliberately makes no success claim from the presence of an
`install-*` directory, a planned operation, an emulator result or a build.

| Required record | Status / evidence |
|---|---|
| Initial internal-flash A/B |PASS: two524,288-byte files are identical; `device-before/internal-A.bin`, `internal-B.bin`|
| Frozen Bootstrap ELF/BIN/canonical APP SHA |PASS: `install-bootstrap-hash/result.json`; full APP readback matches canonical `ce0ca75b…4c9cfd`|
| Complete independent NOR A/B and continuity proof |PASS: `nor-before/manifest.json` reports134,217,728 bytes each, byte-identical; committed ride generation3114→3115 independently captured and audited in `ride-continuity-proof.json`|
| Exact preimage, changed FAT/file regions, physical readback |PASS: `install-rec-verified/result.json` state `regions_verified`; both full physical postimage hashes also match the plan|
| Resident immutable code and preserved factory fields |PASS for Bootstrap installation: exact lower65,536-byte preservation in `install-bootstrap-hash/before-full.bin` / `after-full.bin`|
| Actual Bootstrap boot / live diagnostics |PASS for runtime: exact image and live heartbeat in `bootstrap-hash-status/result.json`; stage9/error7 is the expected missing-REC gate before provisioning, not a successful radio check|
| Whole-NOR postimage physical hash, pass1 |PASS:134,217,728 bytes, error0,362.828s; `nor-after-rec-hashes/result.json`|
| Whole-NOR postimage independent second hash |PASS:134,217,728 bytes, error0,361.094s; same expected SHA as pass1; `nor-after-rec-hashes/result.json`|
| Post-provisioning reboot and REC-ready inspection |PASS: `bootstrap-reboot-ready/result.json`, `verified_files=31`, all five containers ready|
| Bootstrap embedded stock recovery / resident handoff |PASS with a documented acquisition anomaly: `bootstrap-to-stock-confirmed.json`; exact approved stock APP, resident/device data preserved; SWD-published confirmed recovery intent|
| First Product installation and live runtime checks |PASS: `install-product-recovery/result.json` full APP readback; `product-first-boot-check.json`22 checks passed across two live samples; minimum free RTOS heap24,912B|
| Product local CFWREC stock recovery / resident handoff |PASS with bounded page reconfirmation: `product-to-stock-reconciled/result.json` and `stock-verification.json`; two raw full acquisitions differ, reconstructed evidence is identified as such|
| Physical recovery button gesture / wireless request |NOT EXERCISED; neither was exercised by these SWD-triggered trials|
| Stock visible operation / user observation |NOT CONFIRMED in this trial; earlier persistence-trial confirmation is separate evidence|
| Final return to intended Product APP |PASS: `install-product-final/result.json`, exact pre/post physical reads with no page reconciliation; canonical APP `97059cae…28506b`, lower64KiB preserved|
| Final live samples A/B |PASS: `product-final-boot-check.json`, all22 checks, exact running APP identity, ready resources/storage, advancing IO/storage/graphics heartbeats, minimum free RTOS heap24,912B|
| Persisted-data comparison |PASS within the seven freshly read regions: `final-persistence-proof/result.json`, `selected_regions_verified`;2,592,768 physical bytes, write lease released|
| Final display capture |PASS: `final-capture/capture.json` and `display.png`;480×480 RGB565,113 CRC-checked chunks, snapshot1,140ms, capture failures0|
| Final device state and outstanding faults |Product CFW running; no retained fault or active system error in final samples, CCM guards intact. Known BT/ambient hardware limitations remain|

The original complete NOR A/B hash is
`365d8afd42d9d8b028380b746646e45024ac37d059d4253252d050d1fbec1764`.
They remain unchanged. A legitimate committed CFWRIDE journal transition from
sector42/generation3114 to sector43/generation3115 was captured twice and
validated, producing the explicit derived pre-install baseline
`ff9dcc98e16d1bcbc07f711efbd343c1a97b2d3d36f981fbda84e153f5cda6b1`.
This is recorded continuity, not replacing the original backup with an
unexplained changed file.

The provisioned `CFWREC.DAT` uses512KiB at NOR`0x06D61000`, first cluster3501,
root entry42. FAT metadata and the recovery file were physically reread and
compared to the host plan. The expected whole postimage SHA is
`e46c2f5cca47208d41df81065c73e6ad8e95ac421796b616033d4bc3a4a8ef4d`.
Both independent on-device physical sweeps matched it. These sweeps hash
physical NOR and return the digest; they are **not two newly downloaded full
backups**. The original downloaded A/B remain preserved. Subsequent Product
operation and its later saves were checked separately over the selected regions
described below; that later check is not a fresh full-NOR proof.

The final ARM hash tests include complete128MiB input and a mutation of its final
byte, ownership conflicts and cancellation. They support the hash service's
logic, while the separate physical records above establish both real passes.

## Recovery readback evidence and its limits

For Bootstrap→stock, two complete physical acquisitions (one live4MHz and one
halted100kHz) are byte-identical and contain the pinned stock APP. A different
halted acquisition had three inconsistent bytes in the word at`0x0801B71C`.
Two fresh4KiB acquisitions of that page independently matched the approved stock
bytes. `bootstrap-to-stock-confirmed.json` preserves every source/log/hash and
does not rewrite the anomalous capture or create a reconstructed full capture.

For Product→stock, the two raw complete524,288-byte acquisitions were **not**
identical. Eight disagreeing4KiB pages were each reacquired twice; the fresh
paired page reads were used to reconcile the evidence. The resulting evidence
hash is`be1aa05f1aa95a669bdaf9aecf0e12399eab240d3f74cab58fc686678772e634`,
and its APP slice matches the pinned V5.16 hash. This is a documented composite
of actual acquisitions, **not a new raw full-flash capture**, and does not fill
missing bytes from the expected firmware. `stock-verification.json` retains
both original hashes and the eight page offsets/hashes. The exact electrical,
probe or software cause of these inconsistent acquisitions is not established.

Both recovery paths left immutable resident code, factory/device data and the
metadata tail intact; only the expected five installation metadata words changed
in the lower64KiB. These prove the resident stock-install path's resulting bytes.
They do not prove radio delivery, physical button handling, visible stock UI,
absence of every historical fault, or all NOR bytes after later Product writes.

The first Product live test separately confirmed resources ready, intact CCM
guards, owner heartbeat progress, settings/ride restoration once and photo store
readiness. It does not substitute for the separate byte-level persistence proof
or final capture, and normal startup clearing a fault code does not prove
that no earlier fault ever occurred.

## Final Product and selected persistence proof

`install-product-final/result.json` records a fresh exact pre-install stock
capture and exact post-install Product capture. Neither needed page
reconciliation. The final Product canonical APP hash is
`97059caeab949bff672af6d10859b0dffbfb59af1196c339205d93fb2b28506b`;
the full512KiB postimage hash is
`430bafdb1ef1f0999bce149f6f9adc58889aa6d5783fb866d25554d01a4f794c`.
The lower64KiB remained byte-identical through this APP-only installation.
`product-final-status-A/result.json` and `product-final-status-B/result.json`
independently compare the running390,516 APP bytes with the same ELF.
`product-final-boot-check.json` passed all22 checks: resources/runtime/photo
storage ready, settings and ride restoration once, CCM guards checked and
intact, no retained fault or active system error, and progressing IO/storage/UI
heartbeats. Minimum free RTOS heap remains24,912 bytes.

The final capture shows the restored CFW home shell, Kuromi wallpaper, clock and
ODO placeholder. It is the real EVE raster read through113 CRC-checked mailbox
chunks, not a photographed panel or synthetic rendering. The snapshot completed
in1,140ms, with zero capture failures; CPU was running before/after the snapshot
and after export. RGB565 SHA256 is
`89d612b077ae0c08f398f5eea82fdb56a422b5670599aa736229046cd4da51f4`.
No fresh vehicle telemetry was injected in this final check, and physical panel
brightness/BT behavior is not established by an EVE capture.

The final persistence operation quiesced the single storage writer, freshly
read seven selected physical ranges in52.656s, and released only its owned
write lease. FAT metadata, `CFWCFG.DAT`, `CFWPIC.DAT`, `CFWREC.DAT` and the reserved
region preceding APP staging are exactly unchanged from their audited baselines.
The APP staging range contains the approved stock image in its physical
byte-pair order; canonical SHA matches the pinned V5.16 APP.

`CFWRIDE.DAT` legitimately advanced from generation3115/sector43 to
generation3117/sector45. Only sectors44 and45 changed; the prior latest committed
record remains intact. The generation and write count were unchanged across the
quiesced read, so that preservation check did not race a journal write.

This final proof covers exactly2,592,768 physical bytes. It neither downloads a
new full128MiB backup nor re-verifies all other NOR bytes after Product execution.
The detailed ranges, physical read receipts, hashes, comparisons and released
lease are retained in `final-persistence-proof/result.json`.
The first ST-LINK download of the REC read buffer failed its device CRC;
the retained second acquisition matched the device CRC and exact expected bytes.
This is another recorded acquisition inconsistency, not a discarded failure or
evidence that the NOR file was changed.

Machine-readable evidence index: `verification-summary.json`. Final screen:
[final-capture/display.png](final-capture/display.png).

## Remaining limits

- Vehicle permanent12V remains connected; the key changes IGN only. Normal
  recovery entry uses IGN ON to wake the existing APP, its runtime button chord,
  and a software reset. Startup-only/cold-power entry is not the required vehicle
  procedure. The follow-up audit and O0/Os runtime-chord regressions are recorded
  in `../2026-09-20-ign-recovery-entry/README.md`; production firmware is unchanged.
- Damaged donor BT cannot prove healthy-radio installation, retained pairing,
  encrypted-session operation or Android end-to-end timings. USB was unavailable.
- First FAT creation is not power-cut atomic. Partial creation is not silently
  repaired/restarted; the client retains evidence and requires diagnosis.
- Four128MiB stop-and-wait backup passes are expensive; a completed component
  test is not a performance measurement of the actual Bluetooth path.
- The physical rescue hook resides in APP. Broken APP vectors/early startup,
  damaged resident code, or interrupted internal-flash installation may prevent
  it from running. It is not an immutable recovery partition or guaranteed
  rollback after arbitrary firmware damage.
- BL0.15 / SR0701 remains a separate compatibility investigation. This work does
  not establish that changing a version check makes that vehicle safe to update.
