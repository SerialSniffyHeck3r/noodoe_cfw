# Independent recovery gate

This is a separate executable, not a Product task or a Cube-generated module.
`tools/recovery_gate_build.py --output <directory>` builds only the explicit
standalone sources and selected allocation-free shared verifiers. It never
connects to the target or changes a Cube project.

## Boundaries

- Original resident bootloader S0/S1 and device data S3 remain unchanged.
- Original installation metadata remains in S2. It is used only for exact
  approved V5.16 restoration, not ordinary CFW updates.
- Gate occupies S4, `0x08010000..0x0801FFFF`. Its code currently uses less than
  17KiB, but S4 is the minimum64KiB erase boundary at that address.
- Product occupies S5-S7, `0x08020000..0x0807FFFF`, canonical384KiB images.
- Both executables reserve `0x2002FF00..0x2002FFFF` as a256B NOLOAD mailbox.
  CRC protects torn requests; this is neither persistent storage nor security
  authentication. The NOR journal, not retained SRAM, counts boot failures.

Gate starts IWDG before C initialization, resets inherited DMA masters before
claiming SRAM, selects HSI16MHz and uses polling SPI5 with bounded waits.
It requires no HSE, RTOS, SDRAM, resource fonts, Bluetooth or Product code.
The optional screen uses independently initialized SPI1/SPI4 and EVE ROM text.
A broken screen does not authorize writing. No failed driver loop feeds IWDG.
The internal flash worker and all code/constants used while FLASH is busy
execute from internal SRAM under a nonrenewable5s watchdog lease.

## Storage and installation

`gate_abi.h` is the authoritative explicit little-endian format. The independent
FAT walker checks both FAT copies, all file/directory ownership, loops, cross
links and orphan allocations. It maps fixed-size files below the original
reserved region. Logical/physical byte-pair conversion occurs once at the NOR
adapter boundary.

CFWA.DAT and CFWB.DAT each contain a committed4KiB GIM1 header, a padded384KiB
Product image, and an immutable UID/slot-bound GID1 identity at offset0x7F000.
An interrupted inactive header is unusable, but does not invalidate the active
image or the separate approved stock source. CFWBOOT.DAT is a16-sector journal.
An unknown committed journal version or ambiguous sequence history refuses
writing. Journal commits preserve the prior completed sector, physically verify
the complete body and CRC, and program the completion marker last.

A completed candidate request survives power loss before software reset. Gate
checks its header, UID, full SHA, vectors, embedded resource requirement and the
actual matching resource package. It commits COPYING before erasing S5-S7,
copies bounded chunks, then rehashes all internal Product bytes before READY.
Power interruption with a committed COPYING record repeats the validated copy;
it never jumps into a partial Product. The gate never erases S4 during a normal
update. No fallback bypasses a failed resource or image check.

Before every Product handoff gate commits BOOT_PENDING, increments attempts,
and publishes that exact journal sequence in the mailbox. Product must append
CONFIRMED only after 30 seconds of healthy owner progress for that handoff. Three
unconfirmed attempts enter recovery waiting; they do not restore stock.

The existing84-byte NDCP0x58 identity response intentionally retains its legacy
448KiB digest from0x08010000: it hashes gate plus Product together. GIM1 and
target2 transfers instead hash only the384KiB Product at0x08020000. A client
must compare0x58 with the known combined installation image, not directly with
the GIM1 Product hash. The wire format is not silently reinterpreted.

## Local recovery entry

The shared gesture observes IGN OFF for200ms, stable ENTER for80ms, and ENTER
held for500ms before IGN ON. ENTER must remain held for2s after ON. Authorization
expires30s after arming and requires a fresh release after expiry. Initial
ON+ENTER at a reset does not count as the gesture: gate waits for OFF then ON.
Explicit FAULT/WAIT requests enter waiting. A watchdog reset preserves the
unconfirmed-boot journal; three unsuccessful attempts enter waiting. ENTER held
at reset enters waiting immediately, but still requires a new OFF-to-ON gesture.
None of these conditions automatically authorizes a stock overwrite.
The healthy Product uses this same policy to request a software reset into gate;
vehicle main12V does not need to be disconnected.

After explicit local or already-confirmed remote consent, CFWREC.DAT is verified
against the exact pinned V5.16 SHA and UID. The original resident code hash is
also pinned. The source is hashed before staging, then staged at07F90000 and
physically read/hash verified. The shared S2 metadata writer commits only then.
SYSRESETREQ sends execution through the original installer, which intentionally
removes the gate as part of exact stock restoration.

## Limits and evidence

The first gate installation and final exact stock restoration still pass through
the unchanged original BL. The gate cannot make that BL's own S2 erase or S4
installation window atomic. Physical NOR damage, lost supply, corrupted original
BL, unavailable LSI/IWDG or a fault in the gate itself remain hardware/SWD recovery
cases. A valid hash is integrity evidence, not a signing/anti-tamper boundary.
Flash controller locking and range checks protect ordinary software mistakes;
S4 is not option-byte write-protected because the original stock installer must
be able to replace it.

`tests/run.py` executes the actual ARM policy/format/copy core. `--all-cuts`
interrupts both journal boundaries, all three erases and every4KiB programming
step, and checks resumed full-image verification. It also rejects a corrupt or
changing source. `tests/run_journal.py` executes the actual gate journal writer
with physical NOR semantics, partial erase/program/marker and readback corruption
at O0 and Os. These tests do not simulate actual voltage collapse, SPI timing,
original BL execution or RF. Build manifests explicitly retain
`hardware_tested: false` until a separate bench result exists.
