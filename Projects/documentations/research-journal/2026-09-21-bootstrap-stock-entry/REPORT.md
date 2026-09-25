# Bootstrap 복원 진입·검은 화면 수정

2026-09-21. 최종 벤치는 **순정 V5.16 실행 상태**다. 사용자가 새 O2초 확인을 실행했고, 원본 APP 일치 및 이후 실제 순정 화면 표시를 확인했다.

## 두 문제를 분리해 수정

1. 이전 Bootstrap은 Back to stock에서 키 OFF→O 유지→ON을 요구했다. 사용자의 정정에 따라 Bootstrap 메뉴 및 초기 복구 WAIT에서는 **O를 놓았다가 새로2초 유지**만 요구한다. 진입 때 이미 눌린 키는 승인으로 세지 않는다. 중도 해제는 취소하며 진행률을 표시한다. `Bootstrap_Confirm.h`/`bootstrap_confirm.c`를 두 경로가 공유한다. 정상 메뉴는 저장 작업 종료를 확인한 후 기존 confirmed intent를 전달한다. 독립 RecoveryGate의 비상 키 제스처는 변경하지 않았다.

2. 검은 화면은 MCU 정지가 아니었다. exact ELF 대조 후 읽은 `early_active=1`, `watchdog phase=WAIT`, 증가하는 feeds로 초기 복구 루프 실행을 확인했다. 기존 `gate_display.c`는300ms 뒤 EVE ID/CPURESET을 한 번만 검사했다. ready=0, 패널 초기화 전 GPIO 상태, 마지막 SPI1 DR=2가 관측됐다. 정상 BSP와 같은 **ID500ms / CPURESET500ms / DLSWAP250ms 유한 대기**를 추가했다. `g_gate_display`에 첫 실패 단계·레지스터·값·ready·프레임 수를 남긴다. 실물 재시험에서 stage9/error0/ready1, panel status0x9C,165회 화면 제출 및 skipped0을 관측했다. 사용자가 그 화면에서 실제 복원을 실행했다.

## 실제 복원 결과

- 사용자가 O를 놓고2초 이상 유지해 복원을 실행했다. IGN 추가 조작은 요구하지 않았다.
- 첫 read-only 전체 덤프는 순정 idle 상태에서 SWD read 실패로 중단됐다. 이 파일은 증거로 사용하지 않는다.
- 설치 메타데이터가 완료 상태이고 벡터가 순정임을 확인한 뒤 약6초 CPU halt/debug freeze를 사용해 전체512KiB를 읽었다. 종료 시 freeze0/resume을 복구했다.
- `user-stock-proof-a.bin`의 APP448KiB는 승인된 V5.16 원본과 정확히 같고, 하위64KiB도 설치 전과 같다. 전체 SHA256: `be1aa05f1aa95a669bdaf9aecf0e12399eab240d3f74cab58fc686678772e634`.
- CPU PC0x08031934는 기존 순정 분석의 tickless-idle post-hook이다. CFSR/HFSR은0이었다. TIM5 duty50%, PI8 ON/PC8 enable 상태였다. 이것만으로 전체 UI 건전성을 확정하지 않았으며, 이후 사용자가 “어 이제 나온다”라고 실제 표시를 확인했다.

## 순정 화면이 늦게 보인 원인 범위

이 벤치의 기존 순정 실측에서는 조도 I2C 주소 응답 대기 timeout0x3000=12,288ms와 HCI Reset 응답 실패(-14)가 확인됐다. 따라서 무응답 주변장치를 기다리는 순정 초기화가 지연의 유력 원인이다. 근거는 `../2026-09-12-integrated-bringup/stock-probe-04/power-interpretation.md`와 해당 events.tsv다.

이번 부팅의 최초 화면 제출까지 연속 시간 추적은 하지 않았으므로 지연 전체와 각 원인의 기여를 확정하지 않는다. 이번 SWD 검사 자체의 약6초 halt도 추가됐다. Bootstrap 복구용 화면이 영구히 검었던 초기화 오류와, 원본 순정 복원 후 늦게 화면이 뜬 관측을 같은 원인으로 혼동하지 않는다.

## 검증과 산출물

- 실제 ARM UI/확인·취소·제스처·설치 대상 검증: O0/Os24회 통과. 새 O hold의1999/2000ms 경계, 진입부터 눌림, 중도 해제, one-shot, tick wrap를 검사했다.
- 실제 `gate_display.c` ARM 시험: O0/Os10회 통과. 준비 완료, CPURESET 지연 후 정상, ID 오류, CPURESET 영구 대기, panel 응답 오류. SPI/시간은 모형이므로 실제 파형 검증을 대신하지 않는다.
- 수정 Bootstrap436904B / 여유21848B. 실물 설치 후 독립 전체512KiB2회 읽기와 후보 일치를 확인했다. 이후 사용자가 이를 통해 순정으로 복원했으므로 최종 장치는 Bootstrap이 아니다.
- RecoveryGate도 수정 display 코드를 반영해 재빌드했다. Gate 자체의 실제 설치 시험은 아니다.
- `installer/installer.zip`은 갱신된 **벤치 전용·installable=false** 번들이다. 기존 정상 Product ELF를 재사용했으며 Product 코드 변경은 없다. Android wire schema 변경도 없다.

현재 미검증: 정상 Bootstrap 메뉴→drain의 물리 버튼 왕복(공유 확인 로직의 ARM 시험 및 이전 SWD drain 시험과 구분), BT 무선 설치, 실차BL0.15, 독립 Gate 설치, 전원 차단 도중 복원.

현재 순정 화면을 다시 시험용 Bootstrap으로 덮어쓰지 않았다. 원본 파일·FAT·옵션 바이트를 임의 변경하지 않았다.
