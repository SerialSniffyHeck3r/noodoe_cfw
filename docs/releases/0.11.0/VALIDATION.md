# NewDoe 0.11.0 validation

Software checks passed for the matching APK, Product, Bootstrap and new Gate. No hardware current, physical RF or LCD validation was performed.

- Product Debug and Release: 392,100 / 393,216 bytes; 1,116 bytes free. The requested minimum is 1,024 bytes.
- Gate: 43,340 / 65,536 bytes, no Bluetooth or external-font dependency.
- Gate: 20 menu cases, 8 idle/wake cases, 47 boot cases and 2 reset-entry cases; 20 EVE initialization/failure cases and 12 compiled-command previews.
- Product power: 4,044 assertions in each O0/Os run, including repeated simulated STOP/wake. Actual current remains unmeasured.
- Current riding protocol, packed audio/notification graphics and NVM5 maps passed O0/Os tests. Retired riding formats are rejected; installer compatibility remains.
- Resource storage: 23 scenarios in each optimization, including 730 interruption boundaries per run, inactive-slot preservation and stalled RAM-transfer cleanup.
- Android: 129 tests, zero failures/errors, one live-download test deferred to release download verification. Original signing identity retained.
- Relink kit: D8 and zipalign reconstruction passed. The reconstructed APK was not signed or installed on a phone.

See the release `verification.json` and `SHA256SUMS.txt` for artifact identity. The previews use compiled ARM display commands with approximate ROM-font glyphs; they are not LCD photographs.

The accepted old-code stock-return power-loss limitation remains. This Gate change requires one keep-data stock return/reinstallation; do not interrupt permanent power. See [installation and recovery](INSTALL.md).
