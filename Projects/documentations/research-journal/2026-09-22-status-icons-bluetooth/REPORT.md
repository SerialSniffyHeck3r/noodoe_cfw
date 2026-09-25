# 상태 아이콘·라이트 링·연결 설정 — 2026-09-22

## 결과

현재 연결된 벤치에 새 Product를 설치했다. MCU 플래시를 독립적으로 두 번 다시 읽어 일치했고 순정 하위 64KiB, 기존 Gate 및 FAT는 유지됐다. 설치는 SWD 정비 경로이며 무선 업데이트·실차 RF 합격을 뜻하지 않는다.

- BT/location 24px 아이콘: 각각 X120/X336, Y78. 이전보다 안쪽으로 32px, 위로 23px 옮겼다. 시계·메인 영역·속도 링 위치는 변경하지 않았다.
- Light 빈 속도 링: 청회색 대신 중립 회색 `#D8D8D8`. Dark 색상 유지.
- Connections: 실제 휴대폰/무선 상태, Pair phone, Close pairing, Disconnect phone, Re-pair phone, Restart Bluetooth를 연결했다.
- Ambient sensor: 실측 lux/오류 표시와 Retry ambient sensor. 자동 밝기는 유효한 순정 캘리브레이션 기반 LIVE 조도 인덱스를 사용한다. 미확인·오래된 값은 수동 밝기로 돌아간다.

## 조작과 완료 계약

환경설정 → Connections에서 O로 실행한다. Pair phone은 120초 검색 창을 연다. Close pairing은 검색 창만 닫고, Disconnect phone은 현재 연결을 종료하되 저장된 키를 유지한다. 상대 폰의 자동 재연결까지 금지하는 기능은 아니다.

Re-pair phone은 확인 후 HCI 정지 → 기존 휴대폰 키 삭제 → CFWCFG.DAT 저장 세대 확인 → BT 재시작 → 검색 창 열기 순서다. Android에서도 기존 Noodoe 페어링을 삭제해야 한다. 이 보드에서 실제 키 삭제는 시험하지 않았으며, 삭제/저장 실패/시간 초과 경로는 ARM 모의시험으로 검증했다.

작업은 논블로킹이며 큐 접수와 실제 완료를 구분한다. 전체 35초 제한, 일치하는 요청 ID의 결과만 처리하며 실패 ACK를 재시작 명령으로 무한 반복하지 않는다. 설치/시험 부팅 RUN 유지 중에는 이를 중단할 수 있는 연결 작업을 거절한다. BT/HCI/SPP 저수준 소스는 Bootstrap과 Product가 공유한다.

자동 밝기의 0..9 인덱스 → 10..100% PWM, bias −2..+2, 1초 dwell, 100ms당 2% slew는 CFW 정책이다. 순정 PWM 곡선과 동일하다는 주장은 아니다. 조도 값/캘리브레이션이 없거나 stale/override이면 수동 밝기를 사용한다. IGN ON에서만 PWM을 갱신한다.

English·차종 식별·버전은 사실 조회 항목이므로 읽기 전용이다. 빈 사진 슬롯은 업로드 후 선택할 수 있다. 이를 작동하지 않는 가짜 설정으로 활성화하지 않았다.

## 검증

| 항목 | 결과 |
|---|---|
| Product Release/Debug, Bootstrap | 빌드 통과 |
| 설정 UI/비동기 BT/조도 정책 | O0/Os 각각 254 assertions |
| BT transport/키/검색 창 | O0/Os 전체 7개 시나리오 통과 |
| 테마/연료 정책 회귀 | O0/Os 각각 111 assertions |
| 외장 자산 동일성/선택 폰트 계약 | O0/Os/Oz 각각 1,050 assertions |
| 설치 ZIP | 실제 Android production importer 통과 |
| 실제 EVE 캡처 | Light/Dark, 480×480, 약 1.68초 조립 |
| 최종 런타임 표본 | 29.9FPS, CPU 비유휴 59.2%, RTOS 최소 여유 23,728B |
| 폴트/메모리 | 유효한 새 fault 기록 없음, SYSTEM ERROR 0, CCM guard 오류 0 |

BT 재시작과 조도 재시도는 실제 설정 owner 큐를 통해 실행했다. 둘 다 약 0.5초에 DEVICE_ERROR를 반환했으며 UI/다른 태스크는 계속 동작했다. BT 최종 오류 `0x101`은 HCI UART transport 오류 이벤트, 조도는 I2C 오류 3이다. 초기 부팅의 BT timeout `0x302`와 구분한다. 이 결과로 부품 고장 위치나 RF 성공을 확정하지 않는다.

실제 RF 페어링/휴대폰 재연결, 정상 센서의 자동 밝기, 물리 IGN 절전 유지와 소비전류, 장시간 부하는 이번 벤치에서 검증하지 못했다. 실제 Cube GUI 재생성도 수행하지 않았다.

## 메모리

| 자원 | Release | Debug |
|---|---:|---:|
| APP 사용 | 324,552B | 356,904B |
| APP 여유 | 68,664B | 36,312B |
| 일반 SRAM 링크 여유 | 56,352B | 54,896B |
| CCM 여유 | 16,320B | 16,320B |

Product가 실제 사용하는 Lato16을 LVGL 기본 폰트로도 지정해 쓰지 않는 Montserrat14 메트릭의 링크 유입을 제거했다. 사용 중인 폰트·크기·글리프·RSC 내용은 유지했고 다른 프로필의 기본 폰트는 그대로다. RTOS 힙 48KiB, 스택·큐·예산 기준은 줄이지 않았다. Bootstrap은 457,700B 사용/1,052B 여유이므로 추가 기능 전에 반드시 재측정해야 한다.

## 산출물과 근거

- 실행 ELF/BIN/manifest: `release/`; Debug: `debug/`.
- 설치된 padded 384KiB Product SHA256: `815585dc939a09601df7c7d1466fec536cd92f71454177f94c90dd4904e25448`.
- 새 Bootstrap 포함 ZIP: `vehicle015/installer.zip`, SHA256 `a6942f62d6af181225097bf27a69fcbde85c7fc98615c1ab335e74d6405459fe`.
- APK 소스는 이번에 바뀌지 않았고 기존 6.5와 ZIP importer 호환을 확인했다. 실행 중 구 Bootstrap과 새 ZIP의 일치 검사를 우회하지 않는다.
- `verification-summary.json`, `source-hashes.json`, `changed-source-hashes.json`, 빌드/시험 로그 및 실제 캡처를 함께 보관했다.
- 설치 원본 증거: `../2026-09-22-product-theme-fuel/bench-write-evidence-1790066392221414800/`.

![Light 실제 EVE 캡처](icons-bt-light.png)

![Dark 실제 EVE 캡처](icons-bt-dark.png)
