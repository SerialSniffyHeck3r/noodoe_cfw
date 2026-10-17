# NewDoe 0.11.2 validation

Gate uses 60,604/65,536 bytes, leaving 4,932. Product Debug and Release each retain 524 bytes against the 512-byte minimum. SRAM/CCM/heap/stack reservations and existing artwork remain unchanged.

- 77 actual ARM Gate frames fit the round display. Approximate ROM glyph rasterization, not physical LCD photographs.
- 77 Gate menu/progress checks, 8 sleep/wake cases, 7 deep-rescue cases and 47 boot paths plus reset-entry cases passed.
- 72 English frames retain identical EVE commands and every RAM_G byte after shared icon rendering was introduced.
- All nine system languages render across 477 settings frames, with no file reads during drawing, missed cached glyphs or GPU collisions.
- Recovery, parking, settings, update preservation and language reader/publication tests passed; interruptions are simulated, not physical power cuts.
- Android: 128 tests across 14 selected suites passed. One live-download test was deferred until publication; the actual new archive was imported locally. Matched Product/APK/ZIP and language hashes, original APK signature and replaceable-library relink kit verified. Public asset downloads are independently checked after publication.

Both update images occupy 7,522 compressed bytes total in Gate and expand to equal 240×168 transparent ARGB4 images. Selection is cosmetic and latched once per boot. Product contains a CRC-protected version record at 0x080201c0; absent/invalid records display no version.

See [installation](INSTALL.md) and the release verification.json / SHA256SUMS.txt. The new Gate requires keep-data stock return/reinstall even from 0.11.1. The accepted legacy power-loss limitation remains. No physical current, battery-life or RF qualification is claimed.
