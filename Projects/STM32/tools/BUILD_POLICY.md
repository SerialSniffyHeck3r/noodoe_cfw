# Firmware compilation policy

All Cube Debug profiles use the same root defaults: **`-Os -g3 -fno-lto`**.
The linker also disables LTO. `DEBUG`, fault/assert handling, diagnostic APIs,
FreeRTOS/LVGL heap sizes and task stacks remain unchanged. `DATA_DEBUG` is an
independent feature switch; it is not enabled by selecting Debug.

`build_optimization.py` owns Debug defaults and validation. `services_build.py`
applies them after source selection; `check_project.ps1` verifies them before
building. `build.ps1` runs synchronization first, so Cube regeneration can
restore its defaults without silently restoring the old mixed policy.

There are no generated Debug per-file or per-folder optimization exceptions.
New C/C++ files inherit the root policy. The synchronizer removes only known
project-owned legacy exceptions and preserves user-created overrides.

For temporary source-level debugging, set a specific file's explicit CDT
optimization setting to Og or O0. Keep g3 and LTO disabled, rerun the build and
respect its memory budget. Clear that override afterwards to inherit Os again.
Do not place optimization switches in Other flags; the checker rejects hidden
overrides and all Debug LTO flags. Optimized code can make local variables
unavailable and source stepping discontinuous; ELF symbols and SWD memory /
register inspection remain available.

Product Release retains its existing Oz/LTO policy and non-LTO boundaries for
FreeRTOS assembly, EVE wrappers and fault handling. Those are Release codegen
constraints, not ad hoc Debug size exceptions. The ordinary Integrated/Graphics
Release policy is unchanged.

`services_build_test.py` covers migration, explicit user overrides, hidden flag
rejection and profile isolation. `tests/project_layers/run.py` exercises the
real synchronizer on isolated regenerated metadata without modifying Cube C.
Actual compiler commands, final ELF budgets and ARM tests are separate checks.

Bootstrap and UninstallBootstrap embed the pinned stock APP using host-only
Zopfli 0.4.3 (15 iterations). Install with the build Python's `-m pip install
--target Reversing/.tools/build-python zopfli==0.4.3`. The encoder is Apache-2.0;
no encoder or new compression format is linked into firmware. `bootstrap_pack.py`
checks the donor SHA, validates every cached DEFLATE stream by full decompression,
and writes encoder metadata. The cache is private build data, not a source or
release asset. `tests/bootstrap_image/run.py` verifies the actual ARM tinfl
decoder against all stock bytes, seeks and bounds. Rebuild UninstallBootstrap
before Product whenever the compressed bytes change, so its expected hash stays
paired with the packaged uninstall image.
