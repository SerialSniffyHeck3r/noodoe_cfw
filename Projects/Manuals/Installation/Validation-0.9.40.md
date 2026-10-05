# 0.9.40 validation — 5 October 2026

## What changed

Product physical sampling and emergency gestures run in the Health task every10ms, independently of Graphics and Storage. Health supervision still runs every100ms. BSP queue/snapshot consumers use short critical sections. A press belongs to one UI owner and generation; ownership changes and overflow cancel old input instead of inventing an acknowledgement. Existing functional reducers are retained behind that routing fence.

UP+O3s writes a sealed retained reason and triggers NVIC reset immediately. Gate feature8 consumes the matching software-reset marker and excludes the carried O press from recovery admission. Real unfinished installations and damaged images still follow journal validation. O across stable IGN OFF→ON for2s enters the Gate menu without a Storage drain.

Rollback dismissal is RAM state, with NOR acknowledgement queued separately. A confirmed rollback without an active candidate/write no longer blocks audit or new update admission. APK reconciliation still checks UID, running-image confirmation, previous transaction and target identity. It never changes BUSY to IDLE or skips integrity checks.

Product0x45 is an IO-owned snapshot, with the original84-byte prefix and an optional32-byte observation suffix. It reports freshness, progress age, link generation, pending queue and ownership flags. A9 includes the bounded audit ticket/state. Read-only audit uses a5s queue deadline and60s total deadline. Cancellation invalidates its ticket; it does not release another NOR writer or pretend an in-progress physical read has completed.

## Executed checks

| Check | Result |
|---|---|
| Android installer/status/rollback/pairing/reconnection tests |75 passed |
| Android lint |0 errors,0 fatal,135 warnings |
| APK signer |Matches0.9.38; versionCode80 |
| Product and BootStore ARM update scenarios |102 passed across O0/Os |
| Trial/recovery ARM scenarios |42 passed, including actual AIRCR reset writes, no Graphics/Storage calls, held-key and tick-wrap cases |
| Bootstrap/session policy cases |52 passed |
| Settings/UI ownership |9,376 assertions per O0/Os run;1,000 owner-change event sequences per run |
| Routine audit |18 assertions per O0/Os run; queue/full timeout, disconnect and stale completion |
| Physical BSP buttons |94 assertions per O0/O2 run |
| Product and legacy Graphics input adapter |O0/O2 passed; current five-menu sequence |
| Product UI model |O0/Os/Oz passed |
| Gate retained handoff |7 cases passed |
| Product BT regression |34 scenarios,1,576 assertions |
| Bootstrap BT regression |6 scenarios,58 assertions |
| APK replaceable-library kit |D8, zipalign and signature verification passed |

The first broad Android test invocation ran out of PC disk space. The listed75 relevant tests were subsequently run successfully after temporary test data was released. This is not a claim that the entire Android suite passed.

The Graphics test harness was updated for the existing five-menu order and800ms long threshold. The EVE capture harness now models the current bounded SPI RAM_DL read; its old stub produced an empty rejected frame. The corrected harness runs the real compositor and does not skip unsupported drawing commands.

## Build budgets

| Product | Flash used | Free flash | Required free | Free SRAM | Free CCM |
|---|---:|---:|---:|---:|---:|
| Debug |391,160 B |2,056 B |2,048 B |45,592 B |16,320 B |
| Release |357,720 B |35,496 B |4,096 B |44,616 B |16,320 B |

Bootstrap:445,172 B,13,580 B spare. Gate:23,588 B,41,948 B spare. Compiler policies, partitions, heap/stack reservations and resource/font payload are unchanged. Space was recovered by removing unused Product encoder bookkeeping, duplicate cancellation code and redundant status bookkeeping.

The matching Product contract is597dfae25049e016837bbc70d55b9fa72054e71fead41cf7d09f142190526f1d. The normal update sends Product only. Gate capability mismatch selects explicit migration.

## Visual check

The full480×480 quick-settings image uses the actual ARM/LVGL/EVE path. Its completed display list is3,168 B, below8KiB. The brightness value, action text, UP/LCD hint and DOWN/moon hint remain inside the panel. This is an EVE software simulation, not a physical LCD photograph or timing measurement.

## Limits and the reported locked device

No healthy physical radio/dashboard was available. High-speed UART, authentication and SPP regressions passed in simulation; real pairing, RF throughput and repeated installation remain unverified. No claim is made that the reported old unit has recovered.

The supplied older diagnostics proved that secure SPP and the CPU remained responsive while read-only audit/status work stopped progressing; no accepted reset was evidenced. The changes bound that failure path in the new firmware. They cannot replace an already running old firmware whose existing recovery/write path does not respond. Old-device recovery and new-code test results are separate.

Font pixels, text sizes, map renderer and riding layout were not changed. The only UI drawing change is the quick-settings recovery hint. Public distribution contains APK/ZIP, compiled relink objects, capture and manuals; source belongs exclusively to the private repository.
