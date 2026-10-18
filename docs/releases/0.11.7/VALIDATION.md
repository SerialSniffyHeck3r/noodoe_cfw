# NewDoe 0.11.7 software validation

- Original stock V5.16 Thumb routines replayed with synthetic bonds: NOR read, saved peer/key restore, authentication lookup and HCI key reply. No actual device keys are published.
- Bootstrap key/journal tests passed at O0 and Os, including first install without CFG, re-pair/deletion preservation, changed stock key, fragmented CFG, malformed/torn/unreadable records and 40 write-phase interruptions per build (20 original journal + 20 imported-key/source-fingerprint).
- 54 Bootstrap ARM UI/state cases, 20 pinned BTstack HCI/security cases and 6 Bootstrap UART/controller cases passed.
- 218 Android tests passed across 24 suites. One public-download fixture is skipped; actual published assets are downloaded and hash-verified separately.
- Product Release differs from 0.11.6 by exactly six bytes: two version digits and four version-record CRC bytes. Product Bluetooth and riding logic are byte-for-byte unchanged. Both Product builds have 512 bytes free. Bootstrap uses 452024 bytes with 6728 bytes free. Gate is unchanged.
- Matched APK/ZIP/Product contract, APK signing continuity, version records, unchanged language archive, flash budgets and replaceable-library kit verified.

This is software evidence, not a physical RF, pairing, LCD or installation qualification. Only stock V5.16 was available for binary replay; the existing stock V5.14 admission remains without independent key-layout verification. A phone that has already forgotten its key still needs manual pairing once. The accepted older-firmware stock-return power-interruption limitation remains.

See [installation and same-phone test procedure](INSTALL.md). A normal Product update does not replace an already-running Bootstrap. Use the new ZIP during stock installation to exercise the changed boundary.
