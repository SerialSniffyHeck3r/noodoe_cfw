# NewDoe 0.11.8 software validation

- 54 Bootstrap ARM UI/state tests and 20 installation recovery tests passed at O0/Os. Covers saved/missing keys, default No, explicit Yes, disconnection while asking, no reset on timeout, final install authorization/countdown and storage reset barriers.
- 58 Android tests passed across 6 suites without failures or skips. Published files are independently downloaded and hash-checked.
- Scope audit: 105 critical firmware files unchanged and 64 Android installer files have identical logic. Firmware changes are confined to prompt entry, default choice and explanatory copy, plus Product version metadata. Stock key import, persistence, transfer, commit and recovery are unchanged from 0.11.7.
- Product Release differs from 0.11.7 by exactly six bytes: two version digits and four version-record CRC bytes. Both Product builds retain 512 bytes free. Bootstrap uses 452024 bytes with 6728 bytes free. Gate and language resources are unchanged.
- Release ELF captures confirm No is mint by default, Yes is white, no Recommended text remains and all text bounds fit the round screen. EVE command positions/metrics are real; ROM glyph shapes are approximated. These are not device photographs.
- Matched APK/ZIP/Product contract, APK signature continuity, version records, memory budgets and relink kit verified.

![Default No](reset-no.png)

No physical Bluetooth or installation qualification. The previously accepted old-code stock-return power-interruption limitation remains. The 0.11.7 stock-key tests were not repeated because that implementation is unchanged; stock 5.14 key-layout and physical RF remain unqualified.

See [installation](INSTALL.md). Use the new ZIP during stock installation to run the changed Bootstrap; normal CFW update replaces Product only.
