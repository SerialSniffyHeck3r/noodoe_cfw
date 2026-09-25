# SR1.5 V5.16 firmware updater assembly validation

> **2026-09-09 재검증:** SR1 CRC를 재현했고, 아래의 sector 전체 보존 재기록 설명을 16KiB shadow/4KiB program으로 정정했다. [새 바이너리 감사](../2026-09-09-ak550-boot-update-audit/README.md)를 우선한다.

Date: 2026-09-02 KST

이 문서는 보존된 순정 Android 앱의 updater와 실제 SR1.5 V5.16 Cortex-M4
어셈블리를 대조한 결과다. 분석 중 바이크에 펌웨어, 부트로더, OQC write 명령을
보내지 않았다.

## 한 줄 결론

현대식 펌웨어 파일이 `SPP -> 0x0A/0x0B/0x0D -> location 0x0800`으로 현재
애플리케이션에 전달되는 경로는 어셈블리로 확인됐다. 그러나 OTA 파일에는
`0x08000000..0x0800FFFF`의 하위 flash 내용이 없으므로, `DONE` 뒤의 플래시
교체, 검증, rollback 동작은 아직 직접 복원할 수 없다.

## 분석 대상

| 항목 | 값 |
| --- | --- |
| file | `1657088080998-s1-SR1.5_ota_V516.bin` |
| size | `458748` bytes (`0x6FFFC`) |
| SHA-256 | `3b64673054ca84b9cf504f4cc7ebcb2e6615b9fbf59df79a37a92dcf14f037ca` |
| load address | `0x08010000` |
| initial SP | `0x20025318` |
| reset vector | `0x08076718` |
| represented app range | `0x08010000..0x0807FFFB` |
| final word | `0x70067874` at `0x0807FFF8` |

초기 SP, peripheral map, FPU 명령 및 기존 하드웨어 교차검증은 STM32F429 계열과
일치한다. 마지막 word의 의미나 검증 알고리즘은 아직 확인되지 않았다.

## 계층별 확인 결과

### 1. Android 송신 계층: 확정

순정 modern updater는 `VERSION_1_5`와 `VERSION_2_0`에 공통 SPP file-transfer
상태기계를 사용한다. 현재 AK의 기존 실차 tuple인 firmware 5.16, protocol 0.0,
hardware 0은 순정 판별식에서 `VERSION_1_5`다.

펌웨어 전송 파라미터는 다음과 같다.

```text
location       = 0x0800
transfer type  = FILE (2)
file ID        = 1
content ID     = u16 major + u16 minor, little-endian
data chunk max = 11816 bytes
```

START의 `0x0B` 응답에는 meter가 이미 보유한 `receivedLength`가 들어 있으며,
순정 앱은 그 offset부터 재개한다. 순정 상태기계에는 20초 negotiate timeout,
10초 `CONTINUE`, `RESET/CANCEL/REMOVE`도 있다.

### 2. 실행 중 V5.16 명령 계층: 어셈블리로 확정

명령 테이블은 `0x08073F50` 부근에 있고 관련 핸들러는 다음과 같다.

| command | handler | 확인 내용 |
| --- | --- | --- |
| `0x0A` negotiate | `0x08028CE4` | 27-byte 이상, task/attribute 검증, location/type 허용 목록 |
| `0x0B` control | `0x08028EA0` | 32-byte 이상, task/op/transfer/path 검증, CRC/size/identity 전달 |
| `0x0C` continue | `0x0802902C` | 진행 상태를 별도 reply로 직렬화 |
| `0x0D` data | `0x080290FE` | task/transfer/data-type 검증, 저장 콜백 호출, 16-byte reply 직렬화 |

`0x0A`는 다음 packed `(location << 16) | type` 값만 명시적으로 허용한다.

```text
0x01000001
0x02000002
0x03000002
0x04000002
0x05000002
0x06000002
0x07000001
0x07000002
0x08000002  <- firmware, FILE
0x09000002
```

따라서 V5.16이 `location 0x0800 / FILE 2`를 우연히 관용하는 것이 아니라 공식
분기 하나로 처리한다는 점은 확정이다. 이 조합은 navigation/data 전용 callback이
아닌 일반 file callback 군으로 전달된다.

### 3. `0x0D` 응답 의미: 어셈블리와 실차로 확정

`0x080290FE`는 write payload가 최소 7 bytes인지 확인하고 task ID, transfer ID,
data type이 모두 0이 아닌지 검사한다. 선택된 저장 callback의 결과를 받아 다음
16 bytes를 phone 방향 reply로 직렬화한다.

```text
u16 status
u16 task_id
u16 transfer_id
u16 result/detail
u32 accepted_chunk_bytes
u32 cumulative_received_bytes
```

offset 8과 12의 의미는 일반 파일 실차 전송 로그에서 각각 현재 청크 크기와 누적
수신 크기로 확인됐다. 그러므로 기존 문서의 "순정 앱이 파싱하지 않으므로 0x0D
reply가 없다"는 결론은 폐기한다. 순정 Android 앱은 reply 대신 sequence 송신
성공을 진행 조건으로 쓸 뿐, meter firmware는 reply를 실제로 만든다.

### 4. CRC와 파일 종료: 부분 확정

순정 Android 구현은 CRC32 입력의 마지막 1~3 bytes 뒤에 zero를 채워 4-byte
경계까지 계산한다. OpenNoodoe 0.4.1도 같은 padded CRC32로 수정됐고, 일반 파일
전송에서 terminal control까지 실차 검증됐다.

단, 일반 파일의 성공은 펌웨어 이미지의 START/TERMINATE/DONE과 부트로더
검증까지 성공한다는 증거가 아니다.

### 5. 내부 flash persistent 영역: 어셈블리로 확정, 용도는 추정

`0x080426D4`의 함수는 쓰기 대상이 `0x08008000..0x0800FFFF` 안에 있는지
검증한다. 요청 주소를 16 KiB 경계로 정렬하고 해당 sector 전체를 RAM에 복사한
뒤 변경분을 합쳐 erase와 word-program을 수행한다.

STM32F429 sector map에서 이 범위는 정확히 sector 2와 sector 3이며, application
시작점 `0x08010000` 바로 앞이다. erase 호출에는 첫 sector면 ID 2, 다음이면 ID 3을
선택하는 분기도 그대로 존재한다. 따라서 다음은 구분해야 한다.

- **확정**: 실행 중 application이 하위 flash sector 2/3을 보존 갱신할 수 있다.
- **강한 추정**: `0x08000000..0x08007FFF`에는 boot code, sector 2/3에는 설정이나
  update state 같은 persistent data가 배치됐을 가능성이 높다.
- **미확인**: firmware `DONE`이 이 함수로 install marker를 쓰는지, marker의 주소와
  구조가 무엇인지.

`0x080427FA` wrapper는 board shutdown/reset 루틴 `0x08043C50`으로 이어지지만,
이 wrapper와 firmware transfer callback의 연결도 아직 확인되지 않았다.

## 현재 가장 타당한 updater 모델

```text
Android app
  -> Classic Bluetooth SPP/RFCOMM
  -> framed command 0x0A / 0x0B / 0x0D
  -> running SR1.5 application at 0x08010000
  -> general file/storage callback
  -> staging storage and install marker (not yet located)
  -> possible persistent state in internal sectors 2/3 (candidate only)
  -> reset or bootloader transition (not yet connected to DONE)
  -> bootloader validates and replaces application (not present in OTA image)
```

외부 MX66L1G45G 128 MiB NOR는 이미 사진/테마/리소스 저장장치로 확인되어
firmware staging 후보로 강하지만, staging offset이나 파일시스템 경로는 아직
확정되지 않았다. 내부 flash의 빈 영역이라고 단정할 수도 없다.

애플리케이션에는 `NVIC_SystemReset`으로 이어지는 wrapper가 있으나, 현재까지
찾은 호출 경로를 firmware `DONE` callback과 연결하지 못했다. 따라서 이를 OTA
재부팅 루틴이라고 부르지 않는다.

## OpenNoodoe 0.4.1과의 차이

| 항목 | 확인된 순정 동작 | OpenNoodoe 0.4.1 | 상태 |
| --- | --- | --- | --- |
| SPP/framing | 사용 | 사용 | 일치 |
| firmware location/type | `0x0800` / FILE | 동일 | 일치 |
| negotiate layout | 27 bytes | 동일 | 일치 |
| file ID/CRC/size | ID 1, padded CRC32 | 동일 | 일치 |
| data reply | 16 bytes, chunk/cumulative | 동일 의미로 검증 | 실차 일반 파일 확인 |
| resume | `0x0B` receivedLength 사용 | offset 0 고정 | 누락 |
| keepalive | 10초 `CONTINUE` | 없음 | 누락 |
| timeout/state | 20초 및 세부 상태 | 단순 timeout | 불충분 |
| cancel/reset/remove | 구현 | 실패 시 best-effort DONE만 | 불충분 |
| generation | 1.0/1.5/2.0/invalid | modern boolean | 불충분 |
| package binding | OTA metadata/series/resource 관계 | 임의 `.bin` + 수동 버전 | 위험 |
| image validation | 세대별 image map | 기본 vector만 검사 | 불충분 |
| install confirmation | post-transfer meter 동작 필요 | DONE을 accepted로 표시 | 미검증 |

따라서 현재 구현은 "바이트를 meter의 firmware endpoint까지 보내는 골격"에는
가까워졌지만, "안전한 updater"는 아니다.

## 아직 닫히지 않은 핵심 질문

1. 수신 firmware가 외부 NOR 또는 내부 flash의 정확히 어디에 저장되는가.
2. `UPDATE_TERMINATE`와 location `DONE` 중 어느 단계에서 CRC/trailer를 검증하는가.
3. 마지막 word `0x70067874`가 CRC, magic, length 또는 다른 metadata인가.
4. 어떤 persistent marker가 bootloader에게 설치를 요청하며, 내부 sector 2/3이
   그 상태를 보유하는가.
5. bootloader가 active image를 덮어쓰는지, copy-on-boot인지, rollback slot이 있는지.
6. 저전압, SPP disconnect, ignition off, 전송 중 재부팅에서 어떤 복구 상태가 남는가.
7. V1.5와 V2.0의 package series/hardware/resource 호환성 규칙이 정확히 무엇인가.

## 다음 구현 순서

1. 위험 탭의 firmware 기능은 우선 `Inspect`와 `Dry Run`만 제공한다.
2. 아카이브 manifest, SHA-256, series, version, vector map, file size를 실차 tuple과
   묶고 하나라도 불일치하면 fail-closed한다.
3. 공통 전송기에 START `receivedLength` resume, `CONTINUE`, 명시적 cancel/reset,
   disconnect 복구를 구현한다.
4. 사진/테마/리소스에서 중단과 재연결을 반복해 상태기계를 검증한다.
5. 순정 앱의 실제 firmware update trace 또는 희생 가능한 unit 없이는 Armed Update를
   열지 않는다.
6. 최초 실험 시에는 전송 전후 device info, 모든 command/reply, SPP disconnect,
   재부팅 시간, 설치 후 version을 외부 파일에 동기 저장한다.

## 재현 자료

- firmware architecture baseline:
  `analysis/2026-09-01-firmware-update-architecture/README.md`
- hardware cross-check:
  `analysis/2026-09-01-sr15-hardware-crosscheck/README.md`
- command disassembler:
  `analysis/2026-09-01-sr15-hardware-crosscheck/disassemble_targets.py`
- current updater:
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/NoodoeService.java`
- package inspector:
  `android/OpenNoodoe/app/src/main/java/io/opennoodoe/app/OtaPackageInspector.java`
- transfer field live validation:
  `analysis/2026-09-01-transfer-100-percent-stall/README.md`

## 안전 경계

현재 AK550에는 firmware BEGIN, START, DATA, TERMINATE, DONE을 보내지 않는다.
확인 가능한 것은 package inspection, frame dry run, static disassembly, 순정 trace
비교까지다. 특히 `DONE accepted`는 설치 성공, 부팅 성공 또는 rollback 가능을
의미하지 않는다.
