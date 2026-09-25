# RecoveryGate + Product installation, 2026-09-21

RecoveryGate + Product is installed and running. Both post-installation runtime
samples pass Product checks and durable healthy-boot confirmation. Final screen
capture evidence is in `final-runtime/product-capture`.

## Scope and preservation

Bench UID `0039001f 3436510f 35373339`, original V5.16 and resident 0.14.
User requested installation of current CFW, through the attached ST-LINK.
This is not evidence of Bluetooth installation or compatibility with vehicle
resident 0.15/SR0701. Options and original internal lower 64 KiB are preserved.

The original 128 MiB external NOR was independently downloaded twice:
`nor-before/A.bin`, `nor-before/B.bin`. Both SHA-256:
`7cdddd2f237735cef922c99833b8c6b6cd08a9dc90ff06400dc36d73b1b89ac0`.
The first pass was resumed after a Windows manifest publication sharing error,
not a device read failure. Its existing 112 MiB was checked against recorded
chunk CRC/SHA values, live ELF and UID before appending. B is a new full pass.
`resume_backup.py` and `nor-before/manifest-before-resume.json` preserve provenance.
Manifest publication now retries bounded Windows sharing conflicts only; no
hardware write command gains automatic retries.

## Required resource migration

The existing NOODOE.RSC contained a valid old A resource ID
`ee6c0c049be5e6fe28ec5e1614738b75bf8334e5e4f7c0134b072f7574ff4af7`
and an entirely empty B slot. Current Product requires
`a933141bd995d94a4fe62c8fac693bcf5aca06689a139a2357cad81261036694`.
Installing Product without migration would produce a resource error screen.

Added a bounded Bootstrap resource updater, command 8 in its existing SWD
mailbox. It audits the existing FAT/container, authenticates UID and backup
token, validates the required resource format and hashes, preserves valid A,
and requires all of B to be erased. The writer grants only the verified B
extent, verifies physical body/header reads, commits last, then rereads the
whole slot. It does not grant FAT writes or silently replace partial/nonempty B.
No NDCP wire command or Android behavior was changed for this bench migration.

The modified Bootstrap is 438,624 bytes. Its installation was independently
read back twice with original lower 64 KiB intact (`resource-bootstrap`).
`resource-b/result.json` records successful physical migration, old A and FAT
unchanged. Ordinary settings and photos were not reset.

## Initial gate files

The installer derives these allocations from the real backup plus verified
resource delta; it does not relabel an expected image as a fresh physical backup:

| File | NOR offset | Size |
|---|---:|---:|
| CFWA.DAT | 0x06DE1000 | 512 KiB |
| CFWB.DAT | 0x06E61000 | 512 KiB |
| CFWBOOT.DAT | 0x06EE1000 | 64 KiB |

The original CFWREC.DAT matches canonical V5.16 recovery data. Settings, ride,
photos and original files remain byte-identical during provisioning, except the
intended resource B update. The expected final NOR SHA is
`e9e66d13ff53419c45619f483c034a3dcaa82a761a09519a5352b6f1cbb25578`.
`allocation-preflight/expected-after.bin` is explicitly a derived image.
Each allocation has physical preimage/readback proofs and before/after sector
journals. Two fresh device-side full NOR SHA sweeps are required before MCU
installation. These sweeps are verification, not downloaded backup files.

## Internal image and budgets

Gate: `0x08010000..0x0801FFFF`; Product: `0x08020000..0x0807FFFF`.
Final combined 448 KiB SHA, including the handoff fix below:
`7f975fd2e77ceec97a9e466c2eb08a341afc563e10ef6d7edffa5174c0c3ae8a`.
Product padded 384 KiB SHA:
`dbca17ce8d9e998af62dad8c9e4b7cfc9b327a7827a88ae116741f2eff422c60`.

| Configuration | APP used/free | General SRAM free | CCM free |
|---|---:|---:|---:|
| Release | 316,888 / 76,328 B | 65,384 B | 16,320 B |
| Debug | 349,460 / 43,756 B | 63,936 B | 16,320 B |

Both actual ELFs pass existing budgets without reducing heap, stacks or criteria.
Live attestation/capture tooling now uses the validated manifest APP address,
with explicit accepted layout 1/2 addresses; it no longer hardcodes 0x08010000.

## Real-device handoff defect and correction

The initial installed image booted Product successfully but `g_boot_store`
became state3/error3 when confirming the healthy boot. The journal correctly
held BOOT_PENDING sequence2, while the retained handoff incorrectly held
sequence0/attempts0. Gate `Mailbox()` initialized a valid CRC, changed sequence
and attempts, then called `GateRetained_Request()`. That function correctly
rejected the now-stale CRC and initialized the record again, discarding the
journal identity. This was an integration bug, not flash corruption or a
natural watchdog reset.

Gate now populates the complete handoff and seals it once. The regression runs
the actual linked ARM `Mailbox()` function for all reasons, nonzero sequence and
attempts, and sequence wrap boundary. The previous ELF fails6/7 scenarios; the
fixed ELF passes7/7 (`handoff-before.json`, `handoff-after.json`). The journal and
CRC validation in Product were not relaxed, and no completion was forged.

Product's storage owner was drained before changing only S4. Two independent
512 KiB readbacks prove original lower64 KiB and Product unchanged, with the new
gate at S4. Debug freeze was cleared and normal reset/run restored. Final gate
is16,808B; `gate-repair/result.json` and `final-bundle` are the final artifacts.
The earlier `bundle` and failed runtime sample are retained as historical proof.
No external files needed reinstallation after this gate-only correction.

The second real boot wrote BOOT_PENDING sequence3, and Product autonomously
committed CONFIRMED sequence4 after60s. `g_boot_store=[2,0,4,0,0xffffffff,1,1]`;
the retained record has sequence3/confirmed3/attempts0 and valid CRC. Two live
samples show all four owners progressing, IWDG RUN without failure, ready
resources, restored settings/ride/photos, intact48 KiB CCM guards and minimum
RTOS free heap23,776B. `final-runtime/product-check.json` passes22 checks and
`final-runtime/gate-runtime-check.json` passes10 additional checks.

Whole-NOR sweeps before first Product boot both matched the planned image.
Normal Product execution may subsequently append ride and boot journals;
the pre-boot whole-NOR hash is not asserted as the current running-device hash.

## Verification completed before final installation

- Actual ARM resource writer: 14 O0/Os cases, all pass. Includes old/new resource
  corruption, nonempty B, IGN loss and input changed after prepare. Old A/FAT
  preservation and commit ordering checked. These are not physical power cuts.
- Existing Bootstrap storage ARM regression: O0/Os passed.
- Install chain tests 12, recovery host tests 9, SWD PC tests 11, capture tests 6:
  passed. The actual gate ELF's FAT reader audited the expected full image in ARM
  emulation; this is not yet proof of target execution.
- `install_product.py` requires complete NOR verification and actual ELF budgets,
  writes only the combined APP range, reads all 512 KiB twice, then starts normal
  Gate boot. `verify_product.py` checks actual owner progress, resources, restored
  persistence, CCM guards, IWDG and durable healthy-boot confirmation before capture.

## Remaining physical scope

Healthy board wireless operation, vehicle BL0.15, deliberate physical power-cut
tests and the independent Gate's physical emergency button gesture are not
established by this installation. Prior Bootstrap O-hold stock restoration is
a separate tested path, not evidence for all independent Gate recovery paths.
