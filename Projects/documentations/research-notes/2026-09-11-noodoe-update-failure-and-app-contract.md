# Noodoe 업데이트 실패 처리와 커스텀 APP 호환 조건

2026-09-11. 실물 A/B 덤프와 동일한 SR1.5 V5.16 APP, 복원한 resident 부트로더, 순정 APK 및 현재 OpenNoodoe 코드 스냅샷을 대조했다. 이 작업은 정적 분석과 로컬 상태 모델 검증이다. 장치 연결·전송·지우기·기록과 실행 코드 수정은 하지 않았다.

## 핵심 결과

**resident 부트로더에는 이전 APP로 돌아가는 자동 롤백이 없다.** 설치 도중 실패하면 유효한 요청과 NOR 원본이 남아 있는 경우 다음 부팅에서 같은 이미지를 처음부터 다시 복사한다. 새 APP가 정상 실행되는지 확인하고 성공을 확정하는 절차도 없다.

커스텀 APP는 부트로더가 담당하지 않는 수신 무결성·이미지 검증·저장 완료 확인·설치 요청 작성·부팅 결과 보고를 구현해야 한다. 최초에 순정 APP를 통과하는 정책과 이후 커펌이 resident에 인계하는 조건은 다르다.

## 1. 어디서 실패했는가에 따라 상태가 다르다

아래는 **이전의 미처리 요청이 없는 정상 시작 상태**를 전제로 한다. 이미 pending 요청이 있다면 NOR 슬롯을 새 전송으로 덮어쓰면 안 된다.

| 실패 지점 | 기존 내부 APP / NOR / 설치 요청 | 이후 동작과 한계 |
|---|---|---|
| BEGIN의 전원·버전·길이 거절 | APP 유지, 새 설치 요청 없음 | 조건을 고친 뒤 새 작업으로 시작 가능 |
| 수신 중 연결 문제·누락·길이 불일치 | 수신 버퍼가 불완전. 아직 APP 복사 안 함 | TERMINATE와 DONE의 길이 검사를 통과하지 못하면 정상 설치 완료 상태가 되지 않음 |
| 수신 task 비활성 타임아웃 | 진행 중 task·file의 RAM 상태 정리 | NOR의 오래된/불완전 데이터가 자동 소거됐다는 뜻은 아님. 내부 APP를 지우는 경로는 아님 |
| NOR 기록·재판독·CRC 실패 | APP 유지, NOR에 부분 데이터가 남을 수 있음 | 파일의 CRC·완료 길이 등을 지워 정상 완료 상태를 막음. 유효한 설치 요청을 작성하지 말 것 |
| 파일 저장 성공, task DONE 이전 | APP 유지, NOR에는 파일이 있을 수 있음 | 파일 종료와 task 완료를 구별해야 함. 호스트 응답 손실이면 실패 확정도 성공 확정도 아님 |
| DONE 후, IGN OFF 이전 | APP 유지, NOR 파일과 완료 상태가 있음 | 일반 active-task cleanup이 완료 task를 지우지는 않음. 단 실제 SPP disconnect가 어느 cleanup으로 연결되는지는 별도 미확정 |
| RESOURCE 미완료 또는 설치 처리 오류 | APP 유지. FW 파일이 있어도 FW 요청 작성을 건너뛸 수 있음 | 상위 성공 이벤트만으로 FW 설치 요청 기록을 입증할 수 없음 |
| 설치 요청 섹터 erase/program 중 전원 상실 | APP는 아직 유지되지만 메타가 erased 또는 부분 기록일 수 있음 | 다음 부트의 정상화·분기 결과에 따름. 전원 손실에 대해 원자적인 레코드는 아님 |
| 순정 메타 writer가 앞 20바이트 이후의 기록에서 오류 반환 | 이미 유효한 설치 요청이 만들어졌을 수 있음 | 오류 응답만으로 요청 없음·재전송 가능을 선언하면 안 됨. 실제 메타를 재판독해야 함 |
| 요청 작성 성공, reset API 거절 | 요청을 보존하고 현재 APP가 계속 실행될 수 있음 | OFF/HW revision<3 조건. 다음 실제 부팅 때 설치 가능. 새 전송으로 staging을 덮어쓰지 말 것 |
| 부트 NOR 첫 블록 읽기 실패 | APP erase 전에 실패 | 현 부트에서는 실패 루프로 감. 요청이 남으면 다음 새 부팅에 재시도 |
| 두 번째 이후 NOR 읽기 또는 내부 program 실패 | APP 앞부분이 이미 지워지거나 바뀔 수 있음 | 이전 APP로 fallback하지 않음. 요청·NOR가 유효하면 다음 부팅에 처음부터 재복사 |
| 내부 FLASH erase 오류 | 현재 구현은 APP erase 반환을 즉시 검사하지 않음 | 이후 program 결과도 완전한 기록 증거는 아님. 성공 표시가 나와도 readback 검증 필요 |
| 복사 성공 후 요청 해제 도중 전원 상실 | 새 APP는 기록됐지만 메타는 부분 기록 가능 | 같은 이미지 재복사 또는 APP 진입 등 다음 부트의 레코드 해석에 따라 달라짐 |
| 설치 성공 처리 후 새 APP가 crash/hang | 요청은 이미 해제됨 | 재시작해도 같은 APP로 들어감. 부트가 새 APP 건강 상태를 평가하거나 이전 버전을 복구하지 않음 |
| BL 자체 갱신 도중 전원 상실 | 초기 벡터/압축 코드가 불완전해질 수 있음 | 부트 자체가 실행되지 않을 수 있어 APP OTA 경로로 복구할 수 없음 |

구체적인 반환값, 검사 주소, NOR 준비 상태와 전원 출력은 [부트 실패 상세](../analysis/2026-09-11-update-failure-contract/boot-failure/README.md)와 [MCU APP 상세](../analysis/2026-09-11-update-failure-contract/mcu-app/README.md)를 따른다.

DONE 이후라도 OFF 처리를 하기 전에 상시 전원까지 끊으면 RAM의 완료 상태가 사라진다. 요청을 아직 쓰지 않았다면 NOR에 파일이 남아 있어도 다음 부트가 자동으로 찾아 설치하지 않는다.

취소도 단계별로 다르다. `0x0A`의 CANCEL operation 6은 선택 가능한 task의 RAM 상태를 정리하지만, `0x0B`의 FILE CANCEL operation 4는 오류 8 stub이다. 둘 다 이미 남긴 설치 요청을 되돌리는 범용 취소 API로 볼 수 없다. 성공 DONE 뒤 task ID가 0이 되면 과거 ID로 task를 찾는 CANCEL 자체가 실패할 수도 있다.

RESOURCE BEGIN 뒤의 timeout/CANCEL/확인된 cleanup에는 resource gate를 1로 복원하는 쓰기가 없다. 따라서 FW 완료 상태가 살아 있어도 미완료 resource의 gate 때문에 요청 작성을 건너뛸 수 있다. 호스트가 resource 실패를 단순 무시하고 FW 설치 완료로 표시해서는 안 된다.

### 실패 화면은 무선 복구 서비스가 아니다

부트 `0x200045F0`은 `0x200045F6`만 반복한다. 이 함수는 PG13을 읽고 OFF로 해석되는 조건에서 PD13 LOW, PG14 HIGH, 보드 revision≥3이면 PI9 HIGH를 출력한다. 이 루프에 파일 수신·다시 설치·시스템 reset 호출은 없다. 커넥터 회로 전체를 검증한 것은 아니므로 이 출력만으로 실제 전원 차단 시각을 단정하지 않는다.

부트의 HardFault/MemManage/BusFault/UsageFault 기본 처리도 자체 무한 루프다. 검사한 부트 코드에서 watchdog 재시도 카운터나 새 APP 성공 확인 기능은 발견하지 못했다. 외부 watchdog이나 option byte에 따른 하드웨어 동작까지 배제한 것은 아니다.

### 재시도는 설치 요청과 원본이 남아 있을 때만

유효한 pending 레코드가 있으면 APP의 초기 MSP가 잘못되어 있어도 먼저 설치 분기로 간다. 반대로 pending이 해제된 뒤 새 APP의 초기 MSP만 정상 범주에 있으면, reset 주소가 잘못됐거나 APP 본체가 손상됐더라도 부트는 전체 이미지를 검증하지 않는다.

즉 **같은 정상 파일의 복사를 다시 하는 복구**와 **잘못 만든 펌웨어에서 정상 펌웨어로 돌아가는 복구**는 다르다. 후자는 현재 resident가 제공하지 않는다. APP가 실행조차 되지 않으면 APP 안에 구현한 BT updater도 사용할 수 없다.

## 2. 순정 APP 정책과 resident 계약을 분리

| 항목 | 지금 V5.16으로 첫 커펌을 받게 할 때 | 커펌이 resident에 다음 APP를 인계할 때 |
|---|---|---|
| 버전 | 제시 major/minor가 5.16 초과 | APP 설치 분기는 버전 상승을 검사하지 않음. 커펌의 정책으로 결정 |
| 길이 | 최종 `0x50001..0x70000` | 부트 길이 gate는 `1..0x70000`. 실제 실행 가능한 벡터·코드 길이는 별도 검증 |
| 전송 링크 | 순정 Classic SPP `0x0A/0x0B/0x0D` | resident는 링크를 보지 않음. 동일 순정 APK를 쓸 계획이면 그 수신 프로토콜을 구현 |
| 설치 시점 | DONE 등 완료 후 IGN OFF 이벤트 | resident에 유효 요청을 남기고 실제 부팅을 일으키면 됨. IGN OFF 이벤트 자체는 APP 정책 |
| RESOURCE | BEGIN했다면 완료 gate 필요 | resident APP 복사 경로에 resource task 조건은 없음. 커펌 UI 자산 의존성은 자체 관리 |
| 전송 CRC | 파일 재판독 CRC와 기대값 비교 | resident는 요청 필드 비영 여부만 검사. 따라서 커펌이 직접 충분히 검증해야 함 |
| RTC backup `0x13=A5A50002` | 순정 OFF/reset 경로의 상태 기록 | resident가 이 값을 설치 gate로 읽는 근거는 확인되지 않음. 필수 설치 marker로 혼동하지 않음 |

이 구분 때문에 작은 커펌이 resident에서 실행 가능한 것과, 그 작은 BIN을 지금의 순정 APP로 바로 전송할 수 있는 것은 서로 다른 문제다. 첫 설치에는 순정 수신기의 크기 조건을 충족하도록 이미지 제작 방식을 정해야 한다.

## 3. 커스텀 MCU APP가 구현할 계약

### A. 부팅 가능한 APP 이미지와 초기화

- 내부 FLASH 배치는 `0x08010000`부터, 최대 `0x70000`바이트다. 전체 512 KiB 복구 덤프를 APP 파일로 보내지 않는다.
- 파일 시작에 초기 MSP와 Thumb Reset 벡터를 둔다. Reset 주소가 새 이미지 내부의 실제 코드인지 검증한다. 초기 스택은 실제 실장 MCU의 SRAM과 linker map으로 확인한다.
- 부트가 검사하는 것은 `(MSP & 0x2FFC0000) == 0x20000000` 하나다. 이 mask 통과가 실제 유효 RAM·정렬·Reset 주소·전체 이미지의 건전성을 뜻하지 않는다.
- 부트는 APP[0]을 MSP에 넣고 APP[1]로 `BLX`한다. APP Reset handler는 돌아오지 않는 진입점으로 구현한다.
- **초기에 `VTOR=0x08010000`을 설정해야 한다.** 부트의 VTOR는 `0x20000000`에 복사한 부트 벡터를 가리킨다. APP로 넘어가기 직전에 바꾸어 주지 않는다.
- 직전 인계에서는 SysTick CTRL/LOAD/VAL만 0으로 만든다. NVIC/PRIMASK/BASEPRI/CONTROL/clock/주변장치가 전원 reset 직후 상태라고 가정하지 않는다. APP 시작 코드가 인터럽트 마스크, 자신의 벡터, 잔존 IRQ/DMA, `.data`/`.bss`, 클록과 사용할 주변장치를 순서에 맞게 초기화한다.
- 전원 유지 GPIO를 포함한 보드 상태를 먼저 파악한다. 모든 GPIO를 무차별 reset하는 초기화는 전원 유지 출력까지 바꿀 수 있다. 부트가 설정한 PD13/PG14 등의 의미를 보드 회로와 대조해 유지한다.

순정 APP는 `0x08076718→0x08073E90`에서 자신의 RCC 초기화와 `VTOR=0x08010000` 설정을 한다. 순정 고정 주소 함수를 커펌에서 호출하는 방식은 해당 코드가 APP 교체로 사라질 수 있으므로 호환 API로 취급하지 않는다.

### B. 수신·NOR 저장·검증

커펌이 미래 업데이트를 받을 최소 흐름은 다음과 같다.

```text
IDLE → RECEIVING → STAGING → VERIFYING → READY
                                          ↓ 설치 결정
                                    COMMITTING → PENDING_REBOOT
                                                      ↓
                                       다음 부트가 내부 APP 교체
```

- 수신 task·file ID, 총 길이, 현재 offset을 일관되게 관리한다. 중복 패킷 재전송이 중복 append가 되지 않도록 한다.
- 한 이미지의 수신·staging·설치 요청을 다른 전송과 동시에 진행하지 않는다. 특히 pending 요청이 있는 NOR 슬롯은 덮어쓰지 않는다.
- 저장 위치는 NOR 물리 `0x07F90000`, 4 KiB block 번호로는 `0x7F90`이다. 파일시스템 경로나 MCU 주소가 아니다. 컨트롤러·주소 모드·소거 단위·busy 완료를 포함한 NOR driver가 필요하다.
- 부트는 마지막 블록도 4 KiB 전부 복사한다. 따라서 `ceil(length/4096)*4096`바이트까지 준비하고 파일 밖은 FF로 채운다. 같은 영역에 다른 데이터를 두지 않는다.
- NOR write/erase 완료를 기다리고 실제 NOR에서 다시 읽어 길이·내용을 검증한다. 수신 RAM 버퍼의 CRC만 검사한 것으로 저장 무결성까지 확정하지 않는다.
- 순정 프로토콜을 유지하면 전송 CRC는 파일 끝을 4바이트 경계까지 zero-pad한 CRC32다. NOR의 4 KiB FF padding과 구별한다.
- V5.16의 전송 CRC `0xAF819880`과 SR1 이미지 끝 STM32 word CRC `0x70067874`는 별개다. trailer가 resident 설치 필수라는 증거는 없지만, 최초 순정 OTA 형식과 후속 custom image 정책은 명시적으로 관리한다.
- 설치 요청에 들어갈 CRC는 **0과 `0xFFFFFFFF`를 모두 배제**한다. 후자는 부트가 erased 값으로 보고 0으로 정상화한다. 순정 APP의 nonzero 검사만 복제하면 이 경계값을 놓친다.

### C. 설치 요청 레코드와 섹터 소유권

현재 resident의 첫 20바이트 형식:

```c
/* 레이아웃 설명. 타깃에 기록하는 코드가 아니다. */
struct NoodoeInstallRecord {
    uint32_t resident_version;    // 0x08008000: 현재 0x000E0000 보존
    uint16_t app_major;           // 0x08008004
    uint16_t app_minor;           // 0x08008006
    uint32_t nor_start_block;     // 0x08008008: APP = 0x00007F90
    uint32_t image_length;        // 0x0800800C: byte 길이
    uint32_t transfer_crc_request;// 0x08008010: 0/FFFFFFFF은 비활성
};
```

resident 첫 워드를 바꾸면 부트가 자신의 `0x000E0000`으로 복구하면서 요청을 해제하는 경로를 탄다. 새 APP 버전으로 덮어쓰는 위치가 아니다.

**섹터 2 전체 `0x08008000–0x0800BFFF`를 부트 메타데이터용으로 전용 예약해야 한다.** 설치 성공 후 부트는 16 KiB 섹터를 지우고 첫 20바이트만 다시 쓴다. 뒤에 설정·보정값·사용자 데이터를 새로 넣어도 다음 성공 설치에서 보존되지 않는다. 섹터 3 `0x0800C000–0x0800FFFF`의 기존 데이터도 의미를 파악하기 전에는 변경하지 않는다.

순정 APP writer의 16 KiB shadow/첫 4 KiB 재기록 구현을 그대로 복제하지 않는다. 새 writer는 실제 erase/program 단위, 전압·클록 조건, RAM 실행 필요 구간과 오류 반환을 명시하고 기록 후 재판독한다.

순정 writer는 첫 다섯 word에 설치 레코드를 기록한 뒤에도 4 KiB 끝까지 계속 program한다. 그러므로 그 이후 word에서 실패하면 함수가 오류를 반환해도 **이미 pending 요청이 유효할 수 있다.** 이 경우를 `COMMIT_UNKNOWN`으로 취급하고 메타와 staging을 판독해 판단한다. 오류 반환 자체를 원상복구의 증거로 쓰지 않는다.

권장 인계 순서는 **설치 이미지의 저장·검증을 전부 끝낸 뒤 마지막에 요청을 활성화**하는 것이다. 예를 들어 요청을 erased 상태로 둔 채 resident/version/slot/length를 기록·재판독하고, 마지막 request word를 기록·재판독한다. 이 순서는 완성되지 않은 NOR를 설치 대상으로 만드는 창을 줄이는 설계 제안이다. 현재 resident의 단일 레코드를 완전한 전원 손실 원자성이나 롤백 장치로 바꾸는 것은 아니다.

요청 작성 후에는 저장소를 동결하고 reset/다음 부팅으로 이어간다. 실제 reset이 실패하거나 연결만 끊기면 상태를 `PENDING_REBOOT` 또는 `UNKNOWN`으로 유지하고, 이를 곧바로 새 전송 허가로 바꾸지 않는다.

### D. 성공 확인과 복구 범위

전송 ACK, NOR 검증, 설치 요청 작성, 내부 복사, 새 APP 실행을 별도 상태로 보고한다. 새 APP가 자신의 build ID와 초기 자기 진단 결과를 송신기에 보내야 호스트가 설치 성공을 확인할 수 있다. 버전 숫자만 올린 파일을 보냈다는 사실은 새 코드 실행 증거가 아니다.

화면·리소스 초기화가 실패해도 최소 BT 업데이트 기능이 살아 있는 APP 복구 모드를 설계할 수 있다. 다만 **벡터나 초기 코드가 깨져 APP가 실행되지 않으면 이 복구 모드도 실행되지 않는다.** 그 상황의 현재 확실한 복구 수단은 확보한 SWD와 원본 백업이다. 무인 자동 롤백까지 원하면 별도 검증·복구 단계가 필요하며, 현재 resident의 계약만으로 제공되는 기능은 아니다.

## 4. Android/OpenNoodoe 쪽에서 필요한 처리

현재 개발 소스는 `C:/shared/OpenNoodoe`에 있고 분석 중 변경도 관측되어, 읽은 파일을 별도 스냅샷과 SHA-256으로 고정했다. 아래는 그 스냅샷 기준 갭이며 다른 작업에서 이미 바뀌었을 가능성은 [호스트 상세 보고서](../analysis/2026-09-11-update-failure-contract/host-app/README.md)의 manifest와 비교한다.

- firmware 전송도 한 번에 하나만 허용하고 일반 파일 전송과 충돌하지 않게 한다.
- START의 `receivedLength`와 task/file ID를 검증한다. 재개 위치가 불확실한데 offset 0부터 이어 보내지 않는다.
- 전송 전 APP base/vector, 상한, 현재 수신기의 최소 길이·버전 조건, 전송 CRC와 이미지 형식을 확인한다.
- TERMINATE/DONE 응답 유실을 단순 실패로 확정하지 않는다. 장치는 이미 저장·완료 처리했을 수 있다. 설치 상태가 불확실하면 자동 재전송·취소·staging 덮어쓰기를 중지한다.
- 연결 종료와 재접속을 견디는 작업 기록을 남긴다: 장치 식별, 이미지 hash, task/file ID, 길이, 마지막 확인 단계, 기대 build ID.
- UI는 `전송 완료`, `설치 대기`, `설치 확인 중`, `새 APP 실행 확인`, `결과 불명`을 구분한다.

### CONTINUE와 RESET을 구별

순정 APK의 10초 CONTINUE 타이머는 실제 구현 관찰이다. **V5.16 MCU의 operation 2 CONTINUE는 성공 0을 반환하는 no-op**이다. 원시 바이트 `0x08022974: 00 20`으로 재확인했다. operation 5 RESET은 `0x08022E44: 08 20`으로 오류 8을 반환한다. status 8은 `ERROR_INVALID_DATA`이고, `ERROR_NOT_SUPPORTED`는 3이다. 중간 분석에서 CONTINUE도 8이라고 잘못 보고한 부분은 이 원시 바이트 대조로 정정했다.

CONTINUE 성공 응답이 MCU의 수신 활동 타이머를 연장하는 것은 아니다. 확인된 카운터 갱신은 `0x0B` FILE control과 `0x0D` data 처리에 있다. 따라서 순정처럼 CONTINUE를 보내는 동작과, 데이터가 멈춘 상태에서도 세션이 계속 유지된다는 보장은 구별한다. MCU는 별도의 타이머로 active task를 정리할 수 있다. 이 RESET 명령을 성공한 설치 요청까지 취소하거나 보드를 재부팅하는 수단으로 쓰지도 않는다.

그 타이머는 period 20,000 ticks, counter 초기값 50, 주기마다 20 차감이다. control/data 활동은 counter를 50으로 갱신한다. 1 ms RTOS tick 기준 활동이 멎은 뒤 약 40–60초에 정리되는 계산이며, 큐 지연을 포함한 실측값은 아니다. APK의 명령 응답 timeout과 별도로 관리한다.

## 5. 오프라인 검증

[contract_validator.py](../analysis/2026-09-11-update-failure-contract/contract_validator.py)는 장치 I/O 없이 이미지와 메타 분기를 검사한다. 실제 opcode에서 확인한 정상화·gate의 좁은 모델이며 CPU·FLASH 전원 장애를 에뮬레이션하지 않는다.

검증 결과 [contract-validation.json](../analysis/2026-09-11-update-failure-contract/contract-validation.json):

- 메타데이터 14개 사례: 요청 없음, 유효 요청, slot/length/CRC 경계, resident 버전 오염, 잘못된 MSP와 pending의 우선순위.
- 부트가 성공 후 메타 섹터를 지우고 순서대로 byte program하는 과정의 중단 지점 21개.
- 실제 OTA, 같은 바이트에 가상의 상위 제안 버전, RAM에서만 만든 작은 이미지, 전체 SWD 덤프 오입력 등 4개 이미지 사례.

모델에서 성공 후 메타 해제 17/18/19바이트 지점은 부분 요청 값 `FFFFFF00/FFFF0000/FF000000`을 남겨 같은 APP 재복사를 선택할 수 있었다. 이는 erase 완료 후 N개의 byte program이 완료됐다는 가정 아래의 상태 열거다. 실제 brownout이 반드시 그런 상태만 만든다는 주장은 아니다.

원본 V5.16은 벡터·길이·CRC 검사를 통과하지만, 제시 버전도 5.16이면 순정 수신기의 상승 정책에 거절된다. 반대로 작은 synthetic image는 부트 길이 gate를 통과해도 순정 최소 길이를 통과하지 못한다. 이 검사 통과는 실제 실행이나 보드 호환을 보증하지 않는다.

다음은 참고용 로컬 실행이며 이미지나 장치에 쓰지 않는다.

```powershell
& '<local-user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' 'analysis/2026-09-11-update-failure-contract/contract_validator.py'
```

## 증거

- [분석 인덱스](../analysis/2026-09-11-update-failure-contract/README.md)
- [부트 실패 분기](../analysis/2026-09-11-update-failure-contract/boot-failure/README.md)
- [MCU APP 실패·초기화](../analysis/2026-09-11-update-failure-contract/mcu-app/README.md)
- [호스트 구현 갭·APK 오류 처리](../analysis/2026-09-11-update-failure-contract/host-app/README.md)
- [앞선 전체 업데이트 구조](2026-09-11-noodoe-bootloader-and-bluetooth-update.md)
