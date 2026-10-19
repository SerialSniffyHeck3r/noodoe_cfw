# NewDoe 0.11.14 validation

- Android: 270 tests across 33 suites; no failures/errors/skips. Includes lost language inspection/publication/activation reply, exact-device readback, no mutation replay, delayed NDCP response, real EOF, old/new Bootstrap progress ABI, unchanged install handoff and bundle identity.
- Language runtime: 149 cases each at O0/Os across all nine locales, boot-only selection, English-only, resource/RAM fallback and format contracts.
- Actual Release ELF with the B pack and verified stock FONT fixture, substituting only file IO: Korean locale 6, Settings → 설정; 8,412 worker steps / 7,554 file reads; 1,455,264 bytes allocated. No phone query or IGN transition drives activation. Emulation does not establish hardware boot time.
- Bootstrap: 54 ARM scenarios at O0/Os; new assertions cover each file's upload, RAM verification, NOR writing/readback, metadata publication, repeated queries and installation-time inspection. Current and overall bars have separate counters. No physical pairing or flash transaction was tested.
- Actual Product input chain: 10,000 scenarios / 220,023 assertions. Hardware observations are mocked. Existing trip/persistence algorithms, UART fixes, Gate, developer-message bytes, compiler policies and reserved memories remain unchanged.
- Product Debug/Release free flash: 2,152 / 2,152 bytes; hard minimum 512. Bootstrap free flash: 5,832 bytes. No RAM/heap/stack reserve reduction.
- APK signer, Product hash, matching ZIP, unchanged language archive, Java object parity and D8/zipalign checks pass.
- Phone evidence was collected from installed 0.11.12/code123 before update: foreground service was alive; no confirmed camera-correlated failure interval is available. Private phone logs are not published. Camera-triggered loss remains unverified until the vehicle returns.
- The previously accepted old-code stock-return power-interruption limitation is unchanged. This is software validation, not hardware qualification.

[Installation](INSTALL.md)
