# Install session v2 (companion / Bootstrap / Product 6.4)

This implements the scoped installation and maintenance flow. It does not add
RadioRecovery. The independent Gate continues to restore the pinned stock APP
from `CFWREC.DAT`, without Bluetooth, RTOS, SDRAM or external fonts.

## Entry and update paths

- An older Gate is replaced through stock → new Bootstrap → new Gate + Product.
  A routine Product update rejects a different Gate; it cannot silently install it.
- Fresh installation audits every FAT allocation and directory chain, captures
  original metadata, and independently computes the permitted file extents.
  It does not read/hash 128 MiB twice or create a phone-side `expected.bin`.
- The Product is sent once to CFWA; CFWB and final Product staging use local
  verified copies. Stock bytes come from Bootstrap's embedded pinned image.
  Empty configuration/ride/photo/log/journal containers are generated locally.
- Existing stock files and recovery bytes remain read-only. After a stock round
  trip, changed **owned CFW A/B/BOOT only** can be explicitly reseeded. Their exact
  physical preimages are first stored on the phone. The device independently
  checks UID ownership, bounds, preimage SHA and the new image. FAT is unchanged.
  A valid journal record is copied to its last sector before replacing the first;
  the normal first record is verified before removing that temporary copy.
- Normal updates transfer only the 384 KiB Product when Gate/resources match.
  The earlier confirmed image remains until trial confirmation. New resources
  are optional and use the inactive resource slot. A resource RAM receive offset
  survives a link reconnection, not an MCU reset; it is never called a durable
  checkpoint. Product checkpoints are physical 4 KiB checkpoints.
- A 60 second healthy run, BT initialization and app reconnection remain required.
  Confirmed updates reset settings/CFW photos once. Rollbacks preserve old state.

## Wire additions

Existing NDCP framing/correlation is unchanged. Firmware owns the results.

| Opcode | Meaning |
|---|---|
| `86` | schema 2 capabilities: chunk 960 (legacy fallback 512), 4096-byte boundary, feature bits |
| `81` | `SCP2` scope: metadata SHA, nine expected extents, v2 marker; full FAT audit |
| `82` | final scoped ownership/all-file verification, no whole-NOR content claim |
| `87` | local source: embedded stock or verified Product A; no arbitrary address |
| `88` | explicit A/B/BOOT reseed: kind/size/new SHA/scope proof/old SHA |
| `89` | SHA of the inspected file, only after device validation |
| `8A` | generate an empty UID-bound container in bounded chunks |
| `49` | copy verified A body into final update staging; normal DATA/hash guards |
| `8C` | 80-byte install snapshot, schema 1, stage/bytes/file/sector/error/cancel status |
| `8D–91` | resource status, begin, RAM data, inactive-slot publish, safe cancel |

Resource `8D` reports state/transaction/RAM offset/total/error/physical progress,
running requirement SHA and requested SHA. Only SAVED means physical readback,
hash and marker publication completed. A lost publish response is queried first.

## Lifetimes and cancellation

InstallSession is shared by Bootstrap and Product. IGN OFF, link loss and the
idle gap between files do not release its screen/BT/power lease. Trial boot keeps
the lease until the durable boot journal is resolved. A newly pressed O held for
three seconds requests cancellation; already accepted writes drain to a safe
boundary. Committed or uncertain installation is not forcibly aborted.

Gate has Back to stock, Details and UP/DOWN help. Restore confirmation is a fresh
O press for two seconds, without an extra IGN cycle. The emergency gesture is
unchanged. `Verified` is shown only after the recovery image actually verifies.

## Android state reset and themes

The foreground service owns one socket and worker. Activity recreation/theme
changes only attach another observer. Device/ZIP selection is peer-scoped; UID
binding checks unresolved attempts across peer-address aliases. Every new epoch
invalidates previous callbacks and responses. An old service-start intent cannot
stop a newer foreground transfer.

“연결·설치 상태 초기화” first logs intent, fences commands, closes sockets, waits
at most two seconds for the old worker, and records an unknown outcome if needed.
It clears active choices and presentation only. It sends **no ABORT or RESET**.
Logs, journals, backups, downloaded ZIPs, Android pairing, themes, notification
allowlists and quick replies remain. Device storage is untouched. Same-device
ambiguous commits still require reconciliation; reset does not bypass them.

System theme is the default; Light/Dark overrides are persistent. Installer,
dialogs and system bars use that theme. Phone-rendered content has its own
rendering contract and is unaffected by app theme.

## Logs and measurement

`files/installer` holds device-bound journals/evidence; `logs` contains the
structured event sessions. Significant mutating command intentions are synced
before transmission. Connection records count actual TX/RX wire bytes. Session
timing records elapsed, explicit user-wait and active durations separately.
Logs do not include media/notification contents, precise coordinates or secrets.

Normal signed APK: export ZIP with the app's system file picker to Downloads,
then `adb pull /sdcard/Download/<chosen-name>.zip`. Original records remain.
The diagnostic APK additionally permits `adb shell run-as io.noodoe.installer`
for internal evidence.

Full independent NOR A/B download and SHA comparison is a separate detailed
diagnostic action. It is intentionally slow and is not required for v2 install.

## Verification limits

ARM emulation exercises actual firmware state machines with mocked physical NOR;
it is not RF, voltage-loss, LCD or watchdog timing measurement. A fresh FAT
publication is still not power-loss atomic; original/prepared metadata is kept
for recovery and corrupted FAT is rejected, never autoformatted.
Actual phone/healthy-radio no-SWD lifecycle, physical IGN/power interruption,
full LCD clipping and install times require the connected hardware test.


## 6.5.2 Bootstrap 로컬 승인 설치

0x86 capability bit6이면 Bootstrap의 물리 O 승인 후 재검증·commit·reset을 기기가 수행한다. 앱은 COMMIT/RESET을 추가 전송하지 않고 Product 재접속/정상실행 확인을 수행한다. 같은 기기/ZIP의 완전수신 manifest가 RAM에 남으면 `BootstrapContinuation`이 FINISH readback만 재실행한다. NOT_CONNECTED로 끝난 완전수신은 재검증 가능하지만 손상/부분수신/불명확COMMIT을 덮어쓰지 않는다. 완전리셋 후 영구재개는 이 기능의 범위가 아니다. 자세한 근거는 `Reversing/analysis/2026-09-22-install-commit-hang/REPORT.md`.
