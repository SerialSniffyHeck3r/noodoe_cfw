# 0.9.69 · Settings catalog update fix

If a CFW update stops with **Device rejected opcode 151 result=3**, install the **0.9.69 APK over the existing app without clearing its data**, then select the matching **0.9.69 ZIP** and retry the CFW update while stopped. A failure during the initial settings snapshot occurs before this attempt starts resource or Product transfer. Keep existing installation records; if an earlier transfer was interrupted, use Check current CFW / installation result first.

The released Product retained the old notification-duration field in persisted settings but removed its menu descriptor. The first settings-list request therefore failed. This was reproduced by executing the released 0.9.68 ARM code; it is not evidence that NOR flash is physically damaged. The same missing descriptor could reject loading an existing settings journal.

The new app recognizes that specific legacy layout, checks that the updater is idle/failed, reads the four preceding writable preferences with existing individual read commands, then reads all remaining catalog pages normally. It still saves a complete checked, device/candidate-bound preference/name copy before transfer. Busy updates, other errors, unexpected text/layouts and incomplete reads remain errors. The new Product retains the obsolete duration descriptor as hidden/read-only; it does not add a menu or change popup timing.

After confirmed boot, the existing settings comparison, durable save acknowledgements and readback apply. Keep app data until completion. Existing NOR files, photos, trip history and maintenance baselines are preserved by the existing updater; this preference copy is not a complete NOR backup. The colour ring and sharing features from 0.9.68 remain unchanged.

F4 stock 5.14 / 5.16 admission, manual Bootstrap pairing and existing recovery instructions are unchanged. Gate, Bootstrap, stock recovery, resources, Diagnostic and Uninstall images are unchanged. If Stage 6/4 requests a reset, hold **UP + O together for 3 seconds** and reconnect to check the result. The underlying manual-reset limitation remains.

Validation: released-ARM reproduction and rebuilt-ARM catalog checks, focused Android backup/legacy/error tests, Debug/Release/APK builds, matching Product hashes and APK signer verification. No physical vehicle/radio test.

[Installation and recovery](../Installation/README.en.md) · [0.9.68 features and settings retention](30-Sharing-Settings.en.md)
