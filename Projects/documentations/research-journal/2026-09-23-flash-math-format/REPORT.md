# Product flash optimization — 2026-09-23

## Implemented

1. EVE arc chord calculation no longer pulls in double multiplication and double-to-int64 conversion. Its exact integer numerator divided by 32768 has the same truncation as the previous expression over the renderer's supported domain. `Graphics/Port/src/graphics_eve_arc.c` is a project-owned derivative, with the complete upstream MIT notice. Vendor source is unchanged. `tools/eve_arc_port.py` checks the upstream SHA256 and derivative; `check_project.ps1` enforces this contract.
2. LVGL uses its existing CLIB sprintf backend and the same newlib-nano bounded formatter already used by Product. The second built-in formatter is no longer linked. LVGL allocation and memory/string backends are unchanged. Build source selection explicitly excludes the upstream arc TU and the unused CLIB allocator/string files.

No feature, font, resolution, heap, stack or queue reduction. Both builds remove `lv_vsnprintf_inner`, `__aeabi_dmul`, and `__aeabi_d2lz` from their final ELF symbols.

| Product APP | Before used | After used | Saved | Free |
|---|---:|---:|---:|---:|
| Release | 327,564 B | 323,920 B | 3,644 B | 69,296 B |
| Debug | 358,360 B | 354,720 B | 3,640 B | 38,496 B |

Release still has only 3,760 B above its 64KiB reserve; Debug has 5,728 B above its 32KiB reserve. The entire reported free area is therefore not a discretionary feature budget. SRAM/CCM use is identical: Release SRAM free54,784 B, Debug54,512 B, CCM16,320 B. FreeRTOS heap49,152 B remains unchanged.

## Verification

- Product Release and Debug build, memory budgets pass. Existing five compiler warnings remain; see build logs.
- Actual ARM Cortex-M4 original-versus-new arc EVE command streams:17,160 cases each at O0/Os/Oz, all identical. This compares draw operations, not screenshots from the physical GPU.
- Actual original LVGL builtin versus ARM newlib-nano formatted output:1,120 cases each at O0/Os/Oz; output bytes, return values and bounded-buffer behavior match for tested UI formats.
- Existing subpixel arc test:10,000 samples each at O0/Os, pass.
- Services build-source policy tests9/9; project-layer restoration/idempotence/Core-IOC-preservation/profile-selection fixture passes. Actual GUI regeneration and full Integrated/Graphics builds were not repeated.
- Production Android ZIP importer accepts the new package. Gate, Bootstrap, stock, uninstall and resources match the prior6.8.0 release byte-for-byte.
- Companion6.8.0 APK is reused byte-for-byte (same signer). Its202 passing Android tests are prior-release evidence, not rerun results from this change.
- No ST-LINK/UART/phone/RF access, physical installation, screenshot, power-cut test or new FPS measurement in this turn.

## Diagnostic firmware proposal

See the project's `DiagnosticFirmware_DESIGN.md`. A temporary Diagnostic image can occupy the Product region while the independent Gate and original confirmed CFW remain available. Gate would restore the exact saved CFW locally from NOR on exit/reset/fault, without requiring a working radio or a second phone transfer.

This requires a distinct persistent maintenance transaction and Gate support. It must not be presented as an ordinary candidate update: ordinary confirmation can reset settings/photos, and ordinary rollback classifies exit as failure. No diagnostic executable or new Gate is included here; existing Product diagnostics remain until the dedicated path is implemented and verified.

## Artifact provenance

`verification-summary.json`, before/after ELF+memory reports, build logs and `android-importer.log` contain this change's evidence. The first package command accidentally passed Gate BIN to an ELF input and failed before creating a valid ZIP; `package-final/installer.zip` is the successful, independently verified artifact. Older failed output is retained as evidence and is not published.
