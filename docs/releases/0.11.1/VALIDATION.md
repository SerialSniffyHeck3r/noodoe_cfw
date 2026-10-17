# NewDoe 0.11.1 validation

- Gate: 49,668 /65,536 bytes; no Bluetooth. Product Debug/Release:1,116 bytes free each.
-39 compiled ARM display frames fit the480px round aperture; text-box padding3px and safe radius225px. Preview ROM glyphs are approximate, not physical LCD captures.
-20 menu cases,8 idle/wake cases,47 boot paths plus2 reset-entry cases, and20 display-initialization cases passed.
- Android:8 selected importer/language-install-controller tests passed, zero failures/errors/skips. Existing signature and matching Product hash verified.
- Product Release differs from0.11.0 by only one version-character byte. Other runtime behavior is byte-identical.0.11.0 power/riding/resource evidence is inherited, not presented as re-run.
- Relink D8/zipalign reconstruction passed; no reconstructed APK signing or physical installation.

See [installation](INSTALL.md), the release verification.json and SHA256SUMS.txt. The new Gate requires keep-data stock return/reinstall even from0.11.0. The accepted old-code power-loss limitation remains. No hardware current or RF claim is made.
