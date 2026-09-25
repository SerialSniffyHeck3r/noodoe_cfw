# BL 0.15 compatibility dependency audit — 2026-09-21

## Result

The healthy AK550 identity is already captured. No additional DeviceInfo collection is required to establish FW5.16 / BL0.15 / SR0701 / SAA1AA(KR).

This audit did **not** establish physical hardware incompatibility, or find a call to a private BL0.14 routine at a fixed resident-code address in the inspected recovery/update paths. The implementation hands off through NOR staging, internal metadata, and reset.

However, the current installer, Bootstrap, recovery core, metadata writer, Gate, and Product all contain donor-specific assumptions. Removing only the package policy will not provide the requested working Bootstrap -> diagnostics -> CFW / stock recovery path. There is a reproducible rejection even if BL0.15's executable hash happens to equal the donor's hash.

Production firmware, installer code, policy gates and device contents were not changed. Only this audit and its hardware-free reproducer were added. The user's conditional permission to remove gates **if no actual dependency exists** was not treated as permission to bypass the demonstrated broken downstream path.

## Identity evidence

- Original packet: `../2026-09-03-s24-current-symptom/20260903-002031-140/protocol.log`, lines9–12.
- Parsed, hash-linked copy: `../2026-09-20-bootstrap-recovery/stock-identity-observed-boot015/identity.json`.
- `../2026-09-20-bootstrap-recovery/boot-identity-audit.md` traces the V5.16 response to the halfwords at 0x08008000 without arithmetic conversion.
- The BL0.15 metadata word is therefore 0x000F0000 under this observed APP's read contract, not donor0x000E0000. The packet is not a dump of the BL0.15 executable.

## Dependencies found

Paths below are relative to `../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project` unless noted.

| Layer | Exact dependency | Consequence if host policy alone is removed |
| --- | --- | --- |
| Package | `tools/recovery_bundle.py:compatible` accepts only BL0.14/sr0601; NoodoeInstaller builder additionally emits deployment blocked unconditionally | Host-side policy, not proof of hardware incompatibility |
| Android | `NdcpSession` pins donor32KiB SHA and 0xE0000 metadata; `BootstrapProvisioner` checks and writes donor version into recovery container | Connected Bootstrap still cannot complete preparation/update normally |
| Wire update | `NoodoeControl.c` update-begin check; `Update_Service.c:EmptyMetadata` and postcommit check | BL0.15 metadata is rejected before install |
| Recovery | `Recovery_Core.c:EmptyMetadata`, Commit and Reset require 0xE0000 | Back to stock fails even if Bluetooth, display, NOR and buttons work |
| Metadata writer | `Update_Metadata.c:MetaTransaction` rejects anything except UPDATE_METADATA_RESIDENT, then explicitly assigns that constant to word0 | Current code refuses0.15; removing only the comparison would make it write0.14 into the BL version field. This changes metadata, not BL executable code |
| Resident fingerprint | `RecoveryResident_Verify` accepts only donor32KiB SHA | Bootstrap early restore, Gate restore, and Product preflight refuse a different BL executable. Actual0.15 SHA is unavailable, so a SHA mismatch cannot itself be asserted as observed |
| Recovery container | Android provisioner emits0xE0000; firmware container validation also needs coordinated review | Supporting a new version requires changing both producer and consumer, not one UI flag |

The real update handoff also assumes APP base0x08010000, 448KiB maximum envelope, staging0x07F90000, pair-swapped NOR access, five-word metadata at0x08008000, and CRC-last request semantics. These come from the original APP/BL analysis. Shared V5.16 provides evidence for the APP side of this contract; it does not independently demonstrate interrupted-install behavior of BL0.15.

## Comparison with original firmware

`../2026-09-11-bootloader-re/app-bt-update/README.md`, section "설치 요청의 형식", and the corresponding disassembly show that V5.16 writes its16-byte request beginning at0x08008004. The preceding4-byte boot-version field is preserved.

Our writer currently treats this **version field** as a fixed **magic constant**. Its rejection is a limitation of our CFW implementation, not evidence that Kymco made BL0.15's update ABI incompatible. A correct generalization must preserve the target's original value throughout readback, commit, recovery container creation, journal reconciliation and reset checks.

## Reproduction

Run `probe.py` with the workspace Python. It compiles current production `Recovery_Core.c` and `Update_Metadata.c` with Cortex-M4 GCC and executes them with Unicorn. Existing fake platform callbacks model metadata/flash; no physical device is accessed.

Both O0 and Os produce:

- RecoveryCore_Request with0xE0000: RECOVERY_OK(0).
- RecoveryCore_Request with0xF0000: RECOVERY_RESIDENT(5), zero erase/program/commit callbacks.
- UpdateMetadata_Commit with0xF0000: UPDATE_METADATA_RESIDENT_MISMATCH(5), zero erase/program operations, original0xF0000 preserved.
- UpdateMetadata_Commit with0xE0000: success; word0 remains0xE0000.

`results.json` records all four executions and production source SHA256. This is a software rejection reproduction, not a BL0.15 emulation, healthy-radio test, or vehicle recovery test.

## Required change boundary

To deliver the requested UX, first remove the donor-version dependency correctly: preserve observed target metadata, coordinate firmware/Android/Python and container formats, and define a target binding for the resident hash instead of pretending donor hash evidence belongs to the vehicle. Bootstrap diagnostics should include recoverability prerequisites as well as MCU/NOR/SDRAM/display/buttons/actual SPP exchange. A BT-ready flag alone cannot establish a working Back to stock path.

Checksum, flash-region protection, pending-transaction handling and data ownership validation serve different purposes from the unconditional deployment flag and should not be conflated with that flag. No unsupported-hardware conclusion should be inferred solely from PCBA spelling or the0.14/0.15 version difference.
