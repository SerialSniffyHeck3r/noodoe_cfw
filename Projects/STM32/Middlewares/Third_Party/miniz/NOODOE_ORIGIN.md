# miniz source provenance

Source: https://github.com/richgel999/miniz/tree/3.0.2

The project imports upstream tag **3.0.2**, under the accompanying MIT
`LICENSE`. Upstream files are unmodified. Only `miniz_tinfl.c` is compiled;
archive, compression, allocation, filesystem, time and zlib APIs are disabled.
`miniz_export.h` is a project-owned static-link declaration shim in
`App_Logic/Bootstrap/inc`, not an edit to upstream sources.

The Bootstrap uses the raw DEFLATE decoder to expose an embedded, SHA-pinned
stock APP as bounded reads. Its 32KiB dictionary is CPU-only CCM. Product does
not link the embedded stock image or decoder into its normal runtime.

The executable ARM test is `tools/tests/bootstrap_image/run.py`: it checks
every byte of the 448KiB decompressed stock APP, restart/backward seeks and
out-of-bounds requests against the original independent dump.
