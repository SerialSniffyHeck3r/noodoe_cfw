# NewDoe 0.11.3 software validation

Product Debug/Release reserve: {'Debug': 512, 'Release': 512}. Hard minimum 512 bytes; compiler policies and heap/stack/CCM reservations are unchanged. Gate, Bootstrap and the 0.11.2 language archive are byte-identical to the previous release. The internal FONT requirement metadata shrinks from 637 to 474 bytes; all 12 file paths, required character bits and backup hashes remain exactly equal.

- Service retry, partial catalog restart, runtime restart tokens, publication/disconnect, old-pack replacement and metadata tests pass in ARM O0 and Os. Persistent read faults stop after three attempts; no automatic FONT write is introduced.
- Nine real-resource language pipelines load without phone interaction; injected faults during inspection, catalog loading and glyph preparation recover. Published catalogs/masks survive a same-content rescan.
- The final linked Product ELF loads Korean through its actual StorageWork language dispatch. Component and file-boundary emulation, not a hardware boot.
- 72 existing English EVE frames are identical. All nine languages render across 477 frames, with no file reads during drawing, missing prepared glyphs or GPU cache collisions.
- Android: 134 selected tests pass, including uncertain final replies, stale recheck snapshots, unchanged pack reuse and same-Gate ZIP import. One network test is deferred to the publisher’s public download hash checks.
- Matching APK/Product/ZIP hashes, preserved signing certificate and the replaceable-library relink kit are checked.

The reported hardware symptom cannot be assigned to a single cause without its diagnostic trace. These checks establish the fixed code paths, not universal hardware reliability. See [installation and hardware check steps](INSTALL.md). The accepted legacy stock-return power-loss limitation remains for older Gate migration.
