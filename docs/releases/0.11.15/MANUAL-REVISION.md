# NewDoe 0.11.15 manual-r2 — documentation validation

2026-10-19. Documentation and native help only; no new device firmware.

- Replaced accumulated instructions with Korean and English beginner guides: 17 matching sections, clearly separated phone/device actions, first installation, updates, final CFW confirmation, languages, connection recovery, phone-free stock recovery, daily controls and parking.
- Checked 259 local links across 42 current guide/index/topic pages. Kept stable old topic URLs. Marked old release notes/captures as historical instead of presenting them as current instructions.
- Revised native quick help in all 11 app languages, including referenced older-Gate and pairing explanations. Removed superseded quick-reply/update advice. App help still opens offline; the full manual opens the current public page.
- Two targeted Robolectric tests pass, including all 11 locale routes/topic pairs and read-only help opening without a connection. These are software tests, not phone captures or Bluetooth tests.
- APK package and signing certificate are unchanged; versionName remains 0.11.15, versionCode increases from 126 to 127. Permissions are unchanged. Comparison of 377 compiled application class files finds only BuildConfig changed (version code); no installer or riding implementation classes changed.
- Product hash/contract and original CFW ZIP, Bootstrap and B-resource archive match the already released 0.11.15 artifacts. No Gate, stock-return, radio, language-runtime or sleep algorithm was edited.
- Replaceable Mapsforge objects match this APK. D8 reconstruction, ZIP alignment, archive integrity and the original APK signature pass. Reconstructed APK was not installed on a phone.

Existing firmware 0.11.15 needs no reflashing for this revision. A healthy B 0.11.2 language pack needs no retransmission. Install the optional manual-r2 APK over the previous app to update offline help; keep app data.

The old-Gate stock-return power-interruption limitation remains. Camera-triggered connection loss, physical startup time, vehicle standby current and all-model compatibility are not newly hardware-verified by this work. Original firmware evidence remains in VALIDATION.md; this report does not replace it.
