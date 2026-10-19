# NewDoe 0.11.13 validation

- Android: 229 tests across 28 suites; no failures/errors/skips. Tests cover no connection after stock transfer, persisted manual handoff, one connection through local permission and provisioner entry, retry/unknown-result handling, progress, settings preservation, bundle identity and multilingual help.
- Bootstrap: 54 ARM test scenarios across O0/Os, including every developer-message page inside the circular aperture. Gate's obsolete legacy fixture is excluded by the existing bootstrap-only runner; released Gate is unchanged.
- EVE options verified against the repository's vendor definitions: CENTERX=512, CENTERY=1024. Old preview had those axes reversed. Correct decoder reproduces the old left shift; all 8 new pages fit. Glyph pixels in previews are approximate; no physical LCD measurement.
- Original developer message: 3581 bytes, unchanged SHA-256. No text removed or rewritten.
- Bootstrap image: 452296 bytes, 6456 bytes free. Product Debug/Release are 0.11.13 with 2,660 / 2,660 bytes free (512-byte minimum). Compiler and reserved memory settings unchanged. Trip/ignition-session/persistence/maintenance algorithms match the prior source.
- UART service: 249 assertions per O0/Os; BSP IRQ: 122 per O0/Os. Cases include partial frames across RX restart/overflow, current ODO decreases/zero, stale snapshots, transient faults, DMA quarantine and explicit STOP, including late DMA completion after STOP rejection. ODO-to-HOME/trip checks: 13 per O0/Os; remote settings: 54 per O0/Os. No invented hardware frame format.
- Protocol suite: 23,388 assertions per O0/Os; power/UI: 1,183 per O0/Os; trip/page models: six configurations. Selected configuration, ride, settings and save storage tests: 8 runs. Unrelated exhaustive photo cut-point suite is not claimed. Real Product input chain: 10,000 scenarios; hardware-facing callers are compiled separately because LTO otherwise bypasses linker mocks. Production build flags are unchanged.
- APK signer, riding hash, ZIP identity, language archive and compiled-library relink checks pass. Gate, stock recovery, resources, diagnostic and uninstall images are unchanged in the ZIP; Product and Bootstrap change.
- Software tests only. Physical pairing/installation/display must still be checked on the vehicle. Old-code stock-return power-loss limitation remains accepted and unfixed.

[Installation](INSTALL.md)
