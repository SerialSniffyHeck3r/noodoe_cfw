# B / 6 — resource verification before COMMIT

Opcode 67 is COMMIT. Result 11 / Code B means the commit outcome is uncertain. Phase 6 on this Product screen means UPDATE_FAILED, not flash sector 6.

0.9.44 fixes a race where COMMIT reaches the device before its separate resource verification finishes. Update the APK first: it also waits correctly when talking to an existing Product. Install over the existing app with the same signer; keep its data and installation records.

1. Save diagnostics while the error is still present.
2. On a Product supporting the UP+O system reset, hold both together for three seconds to restart. This is not a Bootstrap gesture.
3. After the previous CFW returns to its normal screen, use the app's current CFW / installation-result check.
4. Only a confirmed normal Product, no armed Gate candidate and an empty update state allow the previous attempt to be archived and a new update to start. Use the matching 0.9.44 ZIP.

An active candidate or uncertain live operation remains protected. Inspect diagnostics rather than resending COMMIT. Result 11 can also indicate a real storage failure; software reproduction does not establish the cause on every device. Physical installation success remains unverified.

[한국어](22-Commit-Resource-Wait.md) · [Installation](README.en.md)
