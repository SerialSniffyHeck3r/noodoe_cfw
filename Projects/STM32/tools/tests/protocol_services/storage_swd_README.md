# NOR → 전용 SDRAM → SWD 읽기 서비스

제품 파일은 `Middlewares/Noodoe/StorageSWD/inc/StorageSWD.h`와 `Middlewares/Noodoe/StorageSWD/src/StorageSWD.c`다. SPI5 소유 StorageTask에서 `StorageSWD_Init()`을 호출하고 이후 loop마다 `StorageSWD_Process()`를 호출한다. 이 작업은 Core, Runtime hook, IOC, 프로젝트 설정 또는 장치 상태를 수정하지 않았다.

Init은 기존 BSP RAM/NOR의 ready 상태와 검증된 RAM capacity를 확인하고 `BSP_RAM_Allocate(0x800000)`으로 전용 8MiB 버퍼를 한 번 확보한다. geometry64MiB를 실제 capacity로 가정하지 않으며 64KiB BSP scratch와 다른 할당을 피한다. 성공 후 Init 재호출은 버퍼, 요청과 완료 응답을 보존한다. 준비 실패는 오류 진단만 남기고 이후 명시적인 Init 재시도를 기다린다. 이 서비스는 RAM/NOR 초기화나 파일시스템 mount/format을 호출하지 않는다.

한 번의 Process는 최대 4096 bytes를 `BSP_NOR_Read()`로 읽는다. 반환된 SDRAM bytes로 CRC32/ISO-HDLC를 바로 누적하고 마지막에 XOR FFFFFFFF를 적용한다. CRC는 Python `zlib.crc32`와 같은 정의이며 STM32 word CRC와 다르다. NOR의 전체 `[0,0x08000000)` 범위를 읽을 수 있으나 한 요청은 1..8MiB로 제한한다. offset/length는 덧셈 overflow 없이 검증한다.

NOR program/erase/unlock, FLASH 쓰기, 옵션 바이트 변경은 이 서비스에 없다. 또한 **다른 NOR 작성자를 중단시키거나 NOR 전체의 원자적 snapshot을 만들지는 않는다.** 일관된 백업을 위해 다른 작성 경로를 닫아 두는 정책은 Runtime/백업 소유자가 맡는다.

## ABI와 게시 순서

`g_storage_swd`는 내부 SRAM에 놓이는 128-byte 구조체다. 모든 필드는 little-endian uint32이고 상세 offset은 헤더에 주석과 static assert로 고정했다. 주요 범위는 다음과 같다.

| offset | 소유자 | 의미 |
|---:|---|---|
| 0..44 | 장치 | magic/ABI/크기/state/init_result, 버퍼·NOR 용량, UID 3words, JEDEC |
| 48 | Host | request_magic=`0x31514552` |
| 52 | Host | request_arm=`0x52454144` |
| 56/60 | Host | NOR offset / byte length |
| 64 | Host | request_seq, 마지막에 게시 |
| 68 | 장치 | active_seq |
| 72..92 | 장치 | response offset/length/result/address/ISO CRC/completed bytes |
| 96 | 장치 | response_seq, 마지막에 게시; 0=미완료 |
| 100..124 | 장치 | 요청 결과/소비 seq/거절 seq/poll·수락·오류·BUSY 거절 누계 |

Host는 single outstanding 요청만 사용한다. 새 요청은 request_seq=0 → 48..60의 네 필드 → 이전 시도와 다른 nonzero request_seq 순서로 쓴다. 장치는 seq를 양쪽에서 읽고 같을 때만 필드를 수락한다. seq를 유지한 채 필드만 덮어쓰는 host는 계약을 위반한다.

상태는 IDLE=0, BUSY=1, READY=2, ERROR=3이다. BUSY 중 새 seq는 원 요청을 변경하거나 큐에 넣지 않고 `request_result=4`, `rejected_seq`와 누계에만 거절을 남긴다. 원 작업의 완료 응답은 계속 원 seq로 게시된다. BUSY에서 거절된 요청을 다시 보내려면 완료 후 새 seq가 필요하다.

수락 시 response_seq를 먼저 0으로 내리고 BUSY를 게시한 다음에만 버퍼를 덮는다. 완료는 buffer/descriptor → DSB → DMB → response_seq 순서다. 정상 완료에서는 response_completed=length이고 CRC는 전체 length를 덮는다. ERROR에서는 CRC=0이고 completed는 오류 전 성공한 부분 bytes만 뜻한다. 오류를 완전 백업 데이터로 해석하지 않는다.

완료된 SDRAM 버퍼는 유효한 새 요청이 수락되기 전까지 불변이다. 잘못된 요청은 ERROR descriptor만 게시하며 버퍼를 덮지 않는다. 중간 읽기 오류가 나면 자동 재시도/추가 읽기를 하지 않는다. 성공한 seq의 반복 polling/재전송은 NOR를 다시 읽지 않는다.

Host는 응답 seq/state와 descriptor를 읽고 SDRAM을 가져온 뒤 동일한 seq/state/불변 descriptor를 다시 확인한다. `polls`처럼 계속 변하는 진단값은 일치 비교에서 제외한다. SWD 전송 손상으로 CRC가 다르면 같은 READY 버퍼를 다시 읽을 수 있으며 새 NOR 요청을 보내면 독립된 데이터가 되므로 같은 시도의 재독해가 아니다. UID는 `UID_BASE+0/4/8`의 순서로 제공한다. 문자열 표현은 상위 PC 도구가 정한다.

## 실제 ARM C 검사

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/tests/protocol_services/storage_swd_run.py
```

검증은 실제 `StorageSWD.c`를 ARM GCC로 컴파일하고 Unicorn Cortex-M에서 실행한다. NOR/RAM/UID/barrier의 경계만 대역으로 바꾼다. 제품 CMSIS/BSP include 경로도 STM32F429xx와 `-Wall -Wextra -Werror`로 별도 컴파일했다.

- `-O0`: 4,143개 CHECK 통과.
- `-Os`: 4,149개 CHECK 통과. 최대 8MiB를 2048번 Process로 채우고 전체 bytes를 Python fixture와 비교했다.
- 준비 전 allocation 차단, 용량 부족, allocator 실패·재시도, 성공 Init 멱등성, UID/ABI를 검사했다.
- 길이0/8MiB 초과/NOR 끝 넘침/uint32 overflow, 잘못된 arm/magic, seq=0 및 두 seq 읽기의 불일치를 검사했다.
- 4097-byte 읽기, NOR 마지막 byte, BUSY 새 요청 거절, 버퍼 불변성, 중간 read error, 실행 중 RAM/NOR 준비 상태 하락과 새 seq 복구를 검사했다.
- 완료 게시 전 response_seq가 아직0인지, 범위를 벗어난 목적지나 4096 bytes 초과 read가 없는지 검사했다.
- 최대 버퍼 CRC `0x97E064F9`, SHA-256 `107f7bfbd8848ec7df72643d1be8b799438d731b7affb55e88dda5e185ad6fa6`는 모의 NOR fixture의 결과다. 실물 NOR의 값이 아니다.

소스 SHA-256과 검증 결과는 `storage_swd_output/storage_swd_results.json`에 있다. 실제 SPI5, SDRAM, SWD, UID 값, RTOS 선점, backup 속도는 이 시험으로 확인하지 않았다. 장치 접근 및 프로젝트 전체 빌드는 하지 않았다. Host 도구 `tools/storage_swd_backup.py`와 그 시험은 상위 작업에서 별도로 관리한다.
