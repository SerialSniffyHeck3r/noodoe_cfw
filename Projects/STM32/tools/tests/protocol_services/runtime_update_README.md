# RuntimeUpdate 실제 ARM 경계 검사

`runtime_update_run.py`는 제품 `RuntimeUpdate.c`, `Update_Service.c`, `NDCP.c`, `Update_SHA256.c`를 수정 없이 함께 ARM GCC로 컴파일하고 Unicorn Cortex-M에서 실행한다. `runtime_update_test_port.h`로 RAM/NOR/metadata/BT pause/reset/context 경계만 대역으로 바꾼다. 제품 CMSIS 경로와 실제 Bluetooth public header를 사용하는 컴파일도 `-Wall -Wextra -Werror`로 별도 수행한다.

```powershell
& '<local-user>/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/tests/protocol_services/runtime_update_run.py
```

검증 결과는 **`-O0`, `-Os` 각각 45,529개 CHECK 통과**다. 기존 3,827개 guard를 유지하고 물리 NOR pair-swap codec 검사를 추가했다. 해시가 고정된 결과는 `runtime_update_output/runtime_update_results.json`에 있다. 검증한 RuntimeUpdate.c SHA-256은 `d6c2d95b053d98a98b9465979f0c5f70017b5ecab5743a47d694bb7f74e0973d`다.

검사 범위:

- Init 전 Process 무동작, 미준비 RAM/NOR·잘못된 JEDEC/capacity·allocator 실패·정렬/범위를 벗어난 주소의 공개 차단.
- allocator와 초기화 중 GetService=NULL, 서비스와 전용16KiB scratch의 비중첩, 기본 authorization=0, 성공 Init 재호출의 allocation/세션 보존.
- 실제 callback의 context, stage 전체 범위, offset overflow, NULL/zero length, page256-byte 및 erase4KiB 경계 방어. 실패 요청은 BSP에 도달하지 않음을 호출 횟수로 검사.
- 연결 종료/권한 철회/transaction 불일치 차단. 실제 DATA 첫 page 또는 erase 내부에서 disconnect→reconnect→재승인을 주입해 이전 연결의 다음 page가 실행되지 않음을 검사.
- NOR 대역은 물리 주소 byte 배열을 그대로 읽고 쓴다. stub에서 pair-swap이나 주소 xor1을 하지 않는다. 실제 adapter가 논리 주소 `p`를 물리 주소 `p xor 1`로 변환한 결과를 독립 oracle로 대조한다.
- 홀수/짝수 시작과 길이, 256-byte page 양 끝, 4KiB 경계, staging 첫/마지막 byte를 검사한다. 홀수 RAM source/destination 주소와 canary를 사용하고 요청 외 물리 byte 보존, 이웃 RMW read 0회, 물리 program byte 총합이 논리 요청 길이와 같음을 검사한다.
- 하나의 논리 program에서 lead/middle/tail로 나뉘는 첫째 및 둘째 물리 program 안에 재접속·재승인을 주입한다. 이미 완료한 물리 byte만 남고 이후 mutation은 차단됨을 전체 page 비교로 검사한다.
- 홀수 길이 DATA들을 page/erase 경계를 넘도록 이어 쓰고 각 chunk를 재전송한다. 동일 재전송은 논리 readback만 수행하며 erase/program을 반복하지 않는다. 변경된 재전송은 거절한다. 쓰기 권한이 없는 READ_STAGE의 홀수 시작/길이, page 및 staging 끝 경계 응답을 검사한다.
- metadata의 COMT→UMC2 변환, version/ISO CRC/SHA/수신·검증 길이 일치, 전용16KiB scratch 전달 및 원래 오류 코드 보존.
- BT quiesce 실패에서는 writer/resume0회. quiesce 성공 후 epoch/ready 변경·zero token에서는 writer0회와 resume1회. writer 성공/실패 모두 resume하며 writer 오류와 resume 오류가 함께 있으면 writer 오류를 보존.
- 실제 UpdateService가 metadata 모호 오류 뒤 FAILED/authorization0/capability0이 되고 자동 재시도/reset을 하지 않음을 검사.
- 실제 service/adapter callback으로 전체448KiB를 stage하고 112 erase·1792 page program, 전체 readback SHA/ISO CRC와 commit을 검사.
- 전체448KiB raw NOR가 `pair_swap(canonical APP)`와 byte 단위로 일치함을 실제 ARM C에서 확인한다. 실행 후 Python도 raw 전체를 독립 기대값과 비교하고 다시 pair-swap한 순정 복원 순서가 canonical 전체와 일치함을 확인한다.
- VERIFIED가 disconnect 뒤에도 남되 권한은0, 재승인 전 commit 거절, RESET ACK 전/다른 sequence/1499ms는 reset0회, 1500ms는1회. uint32 tick wrap도 검사.
- pending metadata 각5words의 불일치, metadata read 오류, reset 대기 중 연결 종료가 reset을 차단함을 검사.

전체 이미지 fixture는 458752 bytes, SHA-256 `a20f1f1d6634bb8bdf06ba2222724e2574492a28a7ba99cb5abd180decd29e11`, ISO CRC `0x07B68166`이다. Python hashlib/zlib가 기대값을 만들고 실제 ARM C가 계산한 결과와 비교한다. 이것은 모의 이미지이며 실물 펌웨어나 NOR 내용의 해시가 아니다.

같은 fixture의 물리 pair-swap 표현은 SHA-256 `6e47279eb5a2db96a1b30c6f8b9d2412ed4cd2c88e96433fd98021a8c7bba356`, ISO CRC `0xB798B2CA`다. 메타데이터 CRC는 canonical 이미지의 `0x07B68166`을 유지해야 하며 물리 표현 CRC로 바꾸지 않는다. 실제 순정 BL 기계어를 실행한 시험은 아니고 관측된 pair-swap 복원 규칙과 전체 canonical 일치성을 검사한 것이다.

metadata engine의 실제 sector2 보존/erase/program 검증은 이 시험에서 다시 구현하지 않는다. adapter가 정확한 인수와 token을 전달하는 경계만 검사한다. BT 실제 UART/RTS·DMA drain과 bounded pause는 BT 자체 시험의 범위다. context 대역은 guard 경로를 확인하며 실제 IRQ 선점 타이밍을 재현하지 않는다. 실제 장치, FLASH, 전원 차단, 설치 성공, 실기 timing 또는 전체 프로젝트 빌드를 확인한 결과가 아니다. 제품 파일 변경 및 장치 접근은 하지 않았다.
