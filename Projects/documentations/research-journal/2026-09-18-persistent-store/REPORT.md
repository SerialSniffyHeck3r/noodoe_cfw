# CFW 영구 저장 구현 결과 — 2026-09-18

**실기기 설치 및 CFW → 순정 V5.16 → CFW 부팅·사진·설정 복원 검증 완료.**
순정 왕복 후 전체 NOR까지 읽어 기존 파일1,037개와 CFW 파일 밖의 모든 바이트 보존을 확인했다.
현재 기기는 CFW로 돌아와 있다. 순정 저장 동작과 누도 자체 USB 시험은 별도 미검증이다.

현재 APP SHA256: `0ffa5eede0963f9cb38351d2507b3c03ec9be9b5eb463a2cf08cb184ef823d92`.
APP 전체 읽기 대조와 순정 하위64KiB 보존을 모두 통과했다.
초기 ST-LINK/USB 실패와 불량 SWD 읽기는 성공 기록으로 대체하거나 삭제하지 않았다.
연결 복구 원인 자체는 확정하지 않았고, 모든 큰 읽기는 firmware CRC 또는 알려진 SHA로 검증했다.

## 구현

- 순정 FAT 안의 CFWCFG.DAT(128KiB), CFWRIDE.DAT(256KiB), CFWPIC.DAT(1MiB)를
  전체 소유권 검사 후 파일별 제한된 권한으로 사용한다. Product의 raw NVM 저장을 폐기했다.
- 설정/누계는 4KiB 순환 저널이며 실제 NOR 읽기 대조 후 완료 표식을 마지막에 쓴다.
  버전·UID·용도를 확인하고 CRC 불량 최신본은 이전 완료본으로 복원한다.
- 안정된 필드 ID, 이름·6개 BT 키·설정, 트립·정비 기준·IGN 누계·저연료 상태를 연결했다.
  과거 runtime timer를 복원하지 않고 저장 누계와 새 부팅 증가분을 한 번만 더한다.
- 설정 변경 병합(1초/2초), 50초부터 시작하는 주행 체크포인트, SESSION_END/초기화
  요청, 저장 성공/오류/기한 초과 진단을 구현했다. RAM 적용과 영구 완료 응답을 분리했다.
- 사진 A/B 기록은 입력과 실제 readback을 모두 완전 JPEG 디코드한 뒤 commit한다.
  Product SWD 사진 ABI3은 CFWPIC만 사용하고 이전 WALL/앨범 생성 경로를 거부한다.
  렌더러가 사용하는 decoded generation을 보존하며 반복 교체는 고정 할당을 재사용한다.
- 최초 설치 도구는 전체 A/B·해시·FAT 그래프·기존 파일 내용·변경 전후 섹터를 남긴다.
  Legacy SET1 v1/v2/v3은 읽기 전용으로 검증·이관하고 상위 버전은 거부한다.
  전체 백업 중에는 물리 writer를 drain하는 유지보수 lease를 사용한다.

상세 형식/API/계층/설치·복구 절차:
[PERSISTENCE.md](../../STM32CubeProjects/FuckNudo_Noodoe_CFW_Project/Middlewares/Noodoe/Storage/PERSISTENCE.md).
코드 스냅샷 SHA는 source-sha256.json, 변경 목록·전체 증거 목록은 verification-summary.json에 있다.
생성 Core/IOC/vendor 원본과 힙·스택 용량을 변경하지 않았다.

## 빌드와 메모리

| 빌드 | APP 플래시 여유 B | 일반 SRAM 여유 B | CCM 여유 B |
|---|---:|---:|---:|
| Product Release | 74,896 | 35,736 | 16,320 |
| Product Debug | 59,756 | 35,736 | 16,320 |
| Integrated Release | 43,880 | 20,880 | 65,536 |
| Integrated Debug | 3,264 | 20,224 | 65,536 |
| Graphics Release | 26,316 | 45,576 | 65,536 |
| Graphics Debug | 5,524 | 45,576 | 65,536 |

Product Release/Debug는 최종 ABI3 코드로 다시 빌드했고 candidate-Release/Debug.elf,
.bin, .map을 보존했다. Product의 64KiB/32KiB APP 및 32KiB SRAM, 12KiB CCM 기준을
통과했다. RTOS 힙은 48KiB다. Integrated/Graphics는 기존 프로필별 예산을 그대로
검사했다. Product는 기존 RWX LOAD linker 경고 1개이며 컴파일 오류는 없다.
Graphics에는 기존 bsp_display_capture.c의 미사용 ign 변수 경고가 남아 있다.

Release APP SHA256: `0ffa5eede0963f9cb38351d2507b3c03ec9be9b5eb463a2cf08cb184ef823d92`.
Debug APP SHA256: `6cd7ff8e731493dafc3962cdeeb867ddc8a4706dfc3431d264b7ad79ebf5c68e`.

## 오프라인 검증

실제 프로젝트 C를 ARM Cortex-M4 에뮬레이터에서 실행하고 NOR/UID/시간/SDRAM만
모의 장치로 교체했다. 아래 assert 수는 최적화 한 가지당 수이며 총 3,092개다.
실제 erase 시간, 전기적 중단 또는 실제 BT/패널 동작을 뜻하지 않는다.

| 시험 | 검증 수 | 결과 |
|---|---:|---|
| TestAppSave | 10 | O0 / Os 통과 |
| TestAudit | 8 | O0 / Os 통과 |
| TestConfig | 13 | O0 / Os 통과 |
| TestDeadline | 8 | O0 / Os 통과 |
| TestInstall | 11 | O0 / Os 통과 |
| TestJournal | 322 | O0 / Os 통과 |
| TestPhoto | 12 | O0 / Os 통과 |
| TestPhotoCuts | 604 | O0 / Os 통과 |
| TestPhotoMailbox | 76 | O0 / Os 통과 |
| TestRide | 20 | O0 / Os 통과 |
| TestRideJournal | 440 | O0 / Os 통과 |
| TestSettings | 21 | O0 / Os 통과 |
| TestHistorical (과거 실제 전체 볼륨) | 2 | Os 통과 |

저널 18개 소거/프로그램/완료 단계와 JPEG 기록의 각 소거/페이지/완료 단계에서
중간 또는 물리 완료 직후 중단을 주입했다. 설정32·주행64섹터를 각각3바퀴 기록해
소거 분산과 최신본 보존을 확인했다. 체크포인트 성공 간격≤60초는 모의 시간에서
검사했다. 사진10회 교체의 메타데이터 불변·기존 texture 보존·할당량 안정도 확인했다.

기존 NOR 쓰기 게이트, legacy 저널, 설정 서비스/UI, wallpaper, stock photos 및
프로젝트 재생성 연결 회귀도 통과했다. Cube GUI 재생성 자체는 실행하지 않았고
재생성으로 손상된 설정을 복원하는 fixture의 멱등성/Core·IOC 보존을 검사했다.

과거 검증 스냅샷을 대상으로 설치안을 만들었을 때 기존 파일1,103개 중 비어 있지
않은1,036개의 내용과 예약 tail이 모두 동일했다. 당시 빈 공간17.3125MiB 중
추가1.375MiB만 사용한다. **이는 현재 기기에서 새로 취득한 백업이 아니다.**
이후 실제 설치 전 새 백업에서 다시 검사했고, 현재 장치의 결과는 아래에 기록했다.




## 실기기 설치와 왕복

1. `nor-before-fast`: 원본128MiB를 독립적으로 두 번 읽어 완전히 일치했다.
   A/B SHA256 `a157b429706063ff4189ac3cef4240b4051b99fb9fd0282b9388ba763b92279c`.
   B56MiB 뒤의 USB 실패에서는 검증된 prefix만 보존해 나머지를 독립적으로 이어 읽었다.
   복사로 B를 채우지 않았다. 재개 도구의11개 무결성 검사가 통과했다.
2. 현재 빈 공간은17.25MiB였다. `provision`이 새 세 파일1.375MiB를 빈 클러스터에
   생성했고 변경 전후 섹터·FAT 소유권·원본 파일 해시를 보존했다. 기존 파일을 덮지 않았다.
   `nor-after-provision`은 이후 전체128MiB 물리 읽기가 계획 이미지와 일치함을 증명한다.
   이것은 최초 독립 A/B에 더한 전체 계획 이미지 대조이며, 설치 후 두 번 읽었다고 표현하지 않는다.
3. 정상 버튼 이벤트 경로로 밝기를25→20으로 바꿨다. RAM 적용 뒤1.135초에 실제 저장됐고
   재부팅 후20이 복원됐다. 시험 종료 전25로 다시 변경하고 저장했다.
   손상된 물리 UP 버튼이 수리됐다는 뜻은 아니다. 개발용 입력 mailbox를 사용했다.
4. 사진0을 동일한 JPEG로 A/B 교체했다. 실제 NOR readback·CRC·전체 JPEG decode 후
   generation2가 활성화됐고 generation1도 유효하게 남았다. 슬롯1/2는 generation1이다.
   첫 host 시도의 `.jpg` 확장자 거부를 수정해 byte-identical `.bin`으로 SRAM에 전달했다.
   거부된 시도는 NOR 기록 요청 전 종료됐으며 원본 사진을 재압축하지 않았다.
5. `to-stock`: 원본 전체 덤프의 V5.16 APP만 설치·전체 읽기 대조했다.
   사용자 관찰은 “순정 정상, 3개의 이미지가 사이클 함”. `back-to-cfw`로 동일 CFW APP를
   다시 설치하고 원래 순정 BL을 통해 부팅했다. 양 방향 모두 하위64KiB는 그대로다.
6. `after-stock-return`: 밝기25, 설정·누계 복원 완료, restore_count1,
   세 사진 ready/error0을 확인했다. `runtime-audit-after-stock`에서 FAT·예약 tail 불변,
   설정/사진 전체 파일 byte-identical, 이전 완료 주행 레코드5개 보존을 확인했다.
   누계는 이후 관측한 IGN 시간만 증가한다. 현재 UART speed/ODO는 미수신 상태로,
   실제 주행거리 복원·날짜 경계 시험을 이 결과로 대신하지 않는다.
7. 순정 왕복 후 전체 NOR까지 읽어 기존 파일1,037개와 CFW 파일 밖의 모든 바이트 보존을 확인했다.

주요 증거: `roundtrip-file-comparison.json`, `nor-after-stock-roundtrip/result.json`,
`runtime-audit-before-stock/result.json`, `runtime-audit-after-stock/result.json`.
모든 유지보수 읽기는 writer drain 뒤 실행하고 종료 시 자신이 소유한 pause를 해제한다.
장시간 전체 백업 중에는 정상 저장을 의도적으로 유예하므로 그 시간을 일반60초 체크포인트
성능 시험에 포함하지 않는다. 보존된 초기 주행 레코드의 IGN 누계 차이는50.696/50.100/50.116초였다.

## 실물 자원 관측

- CFW 복귀 후29.8FPS, CPU 약60.1~60.2%. 짧은 정상 화면 관측이며 모든 부하의 최악치 보장이 아니다.
- FreeRTOS 힙 최소 여유24,912B, IO 스택 여유2,504B, Storage 스택 여유2,936B.
- SDRAM 할당12,823,840B. 위의 링크 RAM/CCM 예산과 별도로 측정했다.
- 유효 fault 기록은 없다(magic=0). 무효 레코드의 나머지 SRAM 값을 fault 증거로 해석하지 않는다.
- 조도/BT 손상 보드의 실제 무선 정상화를 주장하지 않는다.

최종 `final-runtime`에서는 저장 pause request/ack가 모두0으로 복구됐고,
체크포인트 성공 시각이 다시 증가하며 overdue/failures0을 확인했다.
TRIP A 화면에서는29.8~29.9FPS/CPU69.7~69.9%, 힙 최소 여유24,912B였다.
`final-capture/display.png`는 실제 EVE 출력의480×480 캡처다(패널을 촬영한 사진은 아님).
현재 UART speed/ODO 미수신이라 트립/ODO의 `--`는 정상적인 미확인 표시다.
마지막 전체 NOR 읽기 중 한 구간의 SWD CRC 불일치는 같은 고정 버퍼를 다시 읽어
검증한 데이터로 대체했다. 실패 전송658바이트 차이와 원본 파일은
`roundtrip-read-retries.json`에 보존했으며 전기적 원인은 아직 확정하지 않았다.

## 남은 실기기 검증

- 실제 IGN 빠른 반복·STOP 복귀·전기적인 전원 차단 중 저널/사진 보호.
  소프트웨어 reset과 ARM 모의 중단 검사를 실제 전원 차단 검사로 대신하지 않는다.
- 장시간 실제 UART/주행·트립 초기화·날짜 변경과 사진 교체 부하 중 저장 기한/FPS.
- 순정 V5.16에서 저장 내용을 바꾸는 동작과 USB 사용 후 재검사.
  사용자는 누도 자체 USB 연결이 불가능하다고 확인했다. 현재 확인한 것은 부팅·사진 순환이다.
- Cube GUI 재생성 자체, 정상 기판에서 실제 BT 동시 연결.

최초 파일 생성의 전원 차단 원자성, 순정 공장 초기화/PC 포맷 이후 데이터 보존은 보장하지 않는다.
원본과 변경 섹터를 유지하고, 실패 시 자동 포맷이나 재생성을 하지 않는다.
