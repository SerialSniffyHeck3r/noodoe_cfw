# Bootstrap 벤치 시험 — 2026-09-21

## 결과

요청한 범위로 Bootstrap을 정리해 현재 벤치 Noodoe에 설치했다. 루트 메뉴는 **Bluetooth test / Install CFW / Back to stock**이고, 실패는 별도 **SYSTEM ERROR / aw shit :(** 화면으로 처리한다.

**실제 내장 순정 복원과 IWDG 리셋 후 독립 복구 대기 진입을 확인했다.** 최종 장치에는 수정 Bootstrap이 다시 설치되어 실행 중이다. 블루투스는 이 벤치에서 여전히 controller startup timeout **0x302**다. 실제 휴대폰 연결·SPP 전송·최종 Gate+Product 설치 성공은 이번 결과에 포함하지 않는다.

## 기능과 조작

- UP/DOWN: 루트의 세 항목 순환. O: 선택.
- Bluetooth test: controller 상태, SPP 연결·암호화 상태, 실제 NDCP 요청/응답 수와 오류를 표시한다. 120초 페어링 창을 열지만 저장·설치 권한을 주지 않는다. O로 돌아온다.
- Install CFW: 별도 로컬 설치 진입 후 같은 암호화 세션에만 설치 권한을 부여한다. 백업·필수 파일·이미지 검증 뒤 Back이 기본 선택인 확인 화면을 표시한다. 업로드 완료를 설치 성공으로 간주하지 않는다.
- Back to stock: 키 OFF → O를 1초 누른 채 → 키 ON → 계속 2초 유지. 중간에 떼면 취소한다. 정상 화면과 early WAIT는 같은 GateGesture 정책을 사용한다. CPU 리셋 때 O가 이미 눌렸다는 사실만으로 이 제스처를 추정하지 않는다.
- SYSTEM ERROR: 제목은 빨강, 바로 아래 `aw shit :(`. O로 복원 안내에 들어가며, 안내에서 O를 짧게 누르면 메뉴로 돌아갈 수 있다. BT 고장 상태에서도 진단과 오프라인 복원을 막지 않는다.

기존 NDCP wire 상태0..8을 유지하고 BT_TEST=9만 추가했다. Android 설치 포크도 상태9를 이해하지만 이를 설치 승인 상태로 취급하지 않는다. OpenNoodoe는 수정하지 않았다.

## 실기기에서 발견해 수정한 원인

### 1. Bluetooth 초기화 전 시간 조회의 잘못된 호출

처음 설치한 Bootstrap은 `Bluetooth_Start()` 안에서 아직 생성되지 않은 BTstack run-loop의 시간 함수를 호출했다. 실제 Cortex-M4 fault에서 **PC=0, CFSR=0x00020000(INVSTATE), LR=0x0801D8ED**를 수집했다. `Now()`를 포트의 `hal_time_ms()`로 바꿔 초기화 이전에도 HAL tick을 직접 사용하도록 했다. 가짜 run-loop를 먼저 만들어 우회하지 않았다. Product에도 같은 수정이 적용된다.

근거: `openocd-fault-dap.log`, `openocd-psp.bin`, `bt-test-results.json`.

### 2. IWDG 시작 명령 이전의 설정 완료 대기

기존 코드는 PR/RLR 설정 후 SR을 기다리고 나서 시작 명령을 보냈다. 실제 F429에서는 **SR=3, PR=0**에 머물러 시작하지 못했다. ST HAL과 동일하게 **CCCC(start) → 5555(unlock) → PR/RLR → SR 완료 → AAAA(reload)**로 바로잡았다. 초기 RAM에 의존하지 않는 기존 pre-C 호출 계약은 유지한다. 회귀시험도 MMIO store 순서를 검사하도록 보강했다.

이 수정은 Bootstrap·Product·Gate가 공유한다. 최신 세 대상 모두 재빌드했다. vendor HAL은 수정하지 않았다.

## 실기기 검증

| 시험 | 관측 결과 |
|---|---|
| 설치 전 전체 내부 플래시 A/B 백업 | 512KiB 두 파일 일치 |
| 최종 Bootstrap 설치 | 0x08010000부터 448KiB APP 범위만 쓰고 별도 전체 읽기로 일치 확인 |
| 실행 안정성 | 약 5분 간격 샘플에서 Bootstrap heartbeat 6,243→67,740, 워치독 feeds 6,252→67,745 증가. fault magic=0, failure=0 |
| 내장 순정 복원 | SWD로 retained confirmed intent를 전달한 뒤, 장치의 HSI-only 복구 코드→원래 resident BL이 V5.16을 설치 |
| 복원 데이터 검증 | OpenOCD 독립 전체 읽기 3회가 서로 같고 원본 APP와 정확히 일치. 하위64KiB 그대로 |
| Bootstrap 재설치 | 전체512KiB 독립 두 번 읽기 후 동일 후보임을 확인하고 실행 |
| 실제 워치독 장애 주입 | 실행 중 failure 필드에 시험값을 써 급식을 중단. debug freeze=0. 35초 후 실제 external reset 관측 |
| 리셋 결과 | reset_flags=0x24000000(IWDG reset 포함), watchdog phase=3(WAIT), early_active=1, 정상 Bootstrap heartbeat=0. APP는 Bootstrap 그대로 |
| 최종 상태 | 소프트웨어 재시작 후 early_active=0, 정상 Bootstrap 태스크·watchdog RUN, IGN ON, BT timeout0x302 |

워치독 시험은 **의도한 RAM 장애 주입**이다. 자연 발생 fault로 기록하지 않는다. WAIT에서 자동으로 순정을 설치하지 않았다. 원래 BL 보존·APP hash 검사를 모두 통과했다. IWDG 옵션 바이트, RDP, 대량 소거, FAT 포맷을 수행하지 않았다.

물리 버튼 제스처와 실제 유리 화면은 사용자의 확인을 아직 받지 못했다. EVE 초기화 성공·UI 상태 값은 육안 확인을 대체하지 않는다. 순정 복원 후 화면 정상 여부도 이번에는 별도 확인되지 않았다.

### 읽기/쓰기 실패도 보존

CubeProgrammer의 첫 수정본 다운로드는 verify 실패했고, 두 번 읽은 APP가 후보와 **37,116바이트** 달랐다. 이 상태를 성공으로 처리하지 않았다. CPU가 프로그래머 RAM-loader 내부에서 fault 상태인 것도 확인했다. 인과관계 전체를 확정하지는 않았다. 이후 OpenOCD native `stlink-dap`/`dapdirect_swd`로 APP를 다시 쓰고 읽기 검증을 통과시켰다.

순정 복원 검증 중에도 CubeProgrammer는 한 번 read 실패, 이후 두 번 서로 다른 데이터를 반환했다. 그 파일은 합격 근거로 사용하지 않았다. OpenOCD 독립 읽기3회가 모두 원본과 정확히 일치한 것이 최종 근거다. 관련 실패 로그/파일은 삭제하지 않았다. 최종적으로 debugger halt와 watchdog freeze를 해제했다.

## 빌드·자동 시험

| 대상 | 결과 |
|---|---|
| Bootstrap | 436,400B / APP 여유22,352B |
| RecoveryGate | 16,520B / 할당64KiB 내 여유49,016B — 재빌드만, 독립 Gate 실물 설치 아님 |
| Product Release | APP316,888B / 여유76,328B / 일반 SRAM 여유65,384B |
| Product Debug | APP349,460B / 여유43,756B / 일반 SRAM 여유63,936B |
| CCM | 여유16,320B |
| UI/제스처/설치 대상·취소 ARM 시험 | O0/Os 총24회 통과 |
| BT transport ARM 시험 | O0/Os 총14회 통과 |
| Watchdog ARM 시험 | O0/Os 각각206 assertions 통과 |
| Android | 단위시험67개, assembleDebug, lintDebug 통과 |

Product 예산 기준·힙·스택을 낮추지 않았다. 최신 `NoodoeInstaller-debug.apk`와 bench-only `installer/installer.zip`도 생성했다. 번들은 기존 제한대로 **installable=false**이며 실차 BL0.15에 설치 가능한 배포본이 아니다.

## 아직 검증되지 않은 부분

- 손상된 벤치 BT가 응답하지 않으므로 무선 검색·페어링·암호화 SPP·무선 업데이트는 미검증이다. timeout 하나로 하드웨어 고장 위치를 특정하지 않는다.
- 현재 저장소 startup audit는 BS_FAILED/BS_COLLISION(7)이다. 8개 설치 의존 파일을 모두 준비·검증한 상태라고 보고하지 않는다. 이번 시험에서는 FAT 파일을 자동 생성하거나 충돌 파일을 덮어쓰지 않았다.
- 완성 Gate+Product 묶음의 실제 설치·부팅은 이번 실증 범위가 아니다. 업데이트 검증/취소/승인 정책의 자동시험과 실제 설치를 구분한다.
- 실제 버튼, 전원 차단 중 업데이트, 정상 무선 기판, 실차 resident0.15 호환성은 별도 시험이 필요하다.

## 식별자

- 최종 Bootstrap raw SHA256: `abe8462e2e2be73a32529d23a90f469f84596cc35145a6a8c1a075cdd459621a`
- 최종 Bootstrap 포함 전체512KiB SHA256: `bcb905365ebf5f083012ceef76832dc2f46c2b4e49453985d2e184ff5c8ee48e`
- 복원된 순정 전체512KiB SHA256: `be1aa05f1aa95a669bdaf9aecf0e12399eab240d3f74cab58fc686678772e634`
- 순정 APP SHA256: `162faeecc64bf56d828570168785e66d1a7da0152ef912bf83f9a19094d91edf`

기계 판독 결과는 `verification-summary.json`, 상세 데이터는 `stock-final-proof.json`, `watchdog-wait-proof/result.json`, `final-current-state/result.json`, `automated-tests-summary.json`에 있다.
