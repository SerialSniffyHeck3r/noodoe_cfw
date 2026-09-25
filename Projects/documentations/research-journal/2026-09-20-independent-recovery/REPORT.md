# 독립 RecoveryGate 구현과 현재 설치 차단 상태

2026-09-20. 이번 작업에서는 ST-LINK, UART, USB 저장소 및 실기기에 접근하지 않았다. 벤치에는 이전에 검증한 펌웨어가 그대로 남아 있다. 이 문서의 ARM 시험은 실제 C 소스를 에뮬레이터에서 실행하되 물리 NOR/FLASH 및 일부 보드 경계를 대체한 시험이다.

## 결론

독립 리커버리, Product의 새 업데이트 경로, IWDG/태스크 생존 감시와 호스트의 설치 차단을 구현했다. 그러나 **새 Product 메모리 기준을 통과하지 못했으므로 설치 가능한 완성본은 아니다.** 실기기 설치, 물리 복구 시험 및 휴대폰의 새 전송 프로토콜 연동은 완료하지 않았다.

| 대상 | 실제 결과 | 판정 |
|---|---:|---|
| 독립 Gate, S4 64KiB | 코드16,444B / 여유49,092B | 경고를 오류로 처리하는 독립 빌드 통과 |
| Product Release, S5-S7 384KiB | 사용341,272B / 여유51,944B | 64KiB 여유 기준보다13,592B 부족 |
| Product Debug, 기존 root O0/g3 | FLASH 영역612B 초과 | 링크 실패. 32KiB 여유까지 확보하려면 최소33,380B 절감 필요 |
| Release 일반 SRAM | 사용161,160B / 여유35,192B | 32KiB 여유 통과. mailbox256B는 따로 제외 |
| Release CCM | 사용49,216B / 여유16,320B | LVGL48KiB+guard64B, 12KiB 여유 통과 |
| FreeRTOS 힙 | 49,152B 예약 유지 | 실행 중 최소 여유는 이번에 미측정 |
| Bootstrap 별도 빌드 | 사용436,636B / 여유22,116B | 빌드만 검증, 설치하지 않음 |
| Integrated Release 별도 복사본 | 사용417,352B / 플래시 여유41,400B | 기존 프로필 빌드·주소·예산 검사 통과 |
| Graphics Release 별도 복사본 | 사용433,144B / 플래시 여유25,608B | 기존 프로필 빌드·주소·예산 검사 통과 |

기존 폰트·아이콘의 보조 비트맵16,148B를 외장 자산으로 옮기고 Release에 전체 그래프 Oz/LTO를 적용했다. 글리프와 픽셀 데이터, 기능, 큐, 태스크 스택, RTOS 힙을 줄이지 않았다. 실행 코드는 내부 플래시에 남긴다. 초기 Release 결과340,844B와 전역 최적화가 잘못 적용됐던 초기 Debug 결과는 최종 설치 판단에 사용하지 않는다. 최종 수치는 방어 검사 보완 및 Debug O0 정책 복구 후 결과다.

## 실제 구조

| 내부 주소 | 역할 |
|---|---|
| `0x08000000..0x08007FFF` | 기존 순정 resident BL, 유지 |
| `0x08008000..0x0800BFFF` | 기존 설치 metadata, 일반 CFW 업데이트에서는 쓰지 않음 |
| `0x0800C000..0x0800FFFF` | 기존 보존 데이터, 유지 |
| `0x08010000..0x0801FFFF` | 독립 RecoveryGate. 실제 코드보다 큰 이유는 S4의 최소 소거 단위가64KiB이기 때문 |
| `0x08020000..0x0807FFFF` | 새 Product APP,384KiB |
| `0x2002FF00..0x2002FFFF` | 양쪽 실행 파일이 예약하는256B NOLOAD 인계 mailbox |

Gate는 자체 startup, HSI, polling SPI, 일반 SRAM을 사용한다. HSE·FreeRTOS·SDRAM·LVGL·외장 폰트·Bluetooth에 의존하지 않는다. EVE 오류/대기 표시는 ROM 글꼴이며, 표시 실패가 쓰기를 허가하지 않는다. FLASH busy 구간은 필요한 함수와 상수를 SRAM에 두고 유한 워치독 lease 안에서 실행한다. DMA는 C 초기화 전에 정지한다. Cube 및 vendor startup은 수정하지 않았다.

새 `Linker/Noodoe_Product.ld`와 이미지 layout2 검증이 이 경계를 강제한다. Bootstrap/Integrated/Graphics의 기존 layout1은 유지한다. 구형 APP-only 설치 도구는 새 Product를 옛 주소에 쓰지 못하도록 거부한다.

## 키와 가운데 버튼으로 순정 복구

정상 Product와 독립 Gate가 같은 입력 정책을 사용한다.

1. IGN OFF를200ms 이상 관찰한다.
2. 가운데 ENTER 버튼을 안정적으로 누른다. debounce80ms, IGN ON 전에500ms 이상 유지한다.
3. IGN ON으로 바꾼 뒤에도2초 유지하면 순정 복구를 확정한다.
4. 버튼을 떼면 즉시 취소한다. 준비된 상태는30초 후 만료되며, 만료 후에는 버튼을 다시 떼고 눌러야 한다.

정상 앱은 저장 작업을 종료시킨 뒤 소프트웨어 리셋으로 Gate에 요청을 전달한다. Deep OFF에서도 이 입력을 감시한다. CPU가 멎으면 IWDG가 리셋한다. **리셋 때 이미 ON+ENTER였다는 사실만으로 이전 키 조작을 추측하지 않는다.** 이 경우 Gate가 기다리며 새 OFF→ENTER 유지→ON 순서가 필요하다. 상시12V 분리는 정상 진입 절차에 포함되지 않는다.

명시적 FAULT/WAIT 요청은 복구 대기로 간다. 정상 부팅의 성공 여부는 NOR 저널로 기록하며, 확인되지 않은 부팅3회는 복구 대기로 간다. 이러한 오류 자체가 자동 순정 덮어쓰기를 허가하지 않는다.

## 워치독

- Gate와 앱 초기 진입에서 software IWDG를 시작한다. 옵션바이트 및 WRP는 변경하지 않았다.
- 정상 실행에서는 HealthService만 급식을 허용한다. IO·Storage·Graphics·시작된 BT 태스크의 완료된 작업 진행을 각각 확인한다. IRQ가 도는 사실만으로 건강하다고 판단하지 않는다.
- IO/BT는 RUN에서1초, OFF에서2.5초, Storage2초, Graphics2.5초 마감이다.100ms 감독 태스크를 사용한다.
- BOOT30초, 복구 최대10분, FLASH5초의 별도 제한을 둔다. FLASH 진입에는 최근 정상 실행 증명이 필요하며 실패한 lease는 유예를 새로 만들지 않는다.
- STOP은 최대500ms씩 사용한다. 수면 훅에서 무조건 워치독을 급식하지 않는다.
-60초간 정상 진행한 정확한 BOOT_PENDING만 StorageTask가 CONFIRMED로 영구 기록한다. 전체 NOR 백업 또는 저장소 drain 동안 이 쓰기가 끼어들지 않는다.

실제 LSI 편차, 리셋 시간, STOP 전류, SWD halt와의 상호 작용은 미검증이다. 이 변경이 보드에 아직 설치되지 않았다는 점도 중요하다.

## 업데이트와 순정 복귀는 별도 경로

일반 CFW 업데이트는 `target2`, 전체384KiB 이미지다. 비활성 `CFWA.DAT` 또는 `CFWB.DAT`에 기록한 뒤 물리 readback·CRC/SHA·벡터·장치 UID·필수 자산을 확인한다. `CFWBOOT.DAT` 완료 저널을 마지막에 기록하고 ACK 이후 리셋한다. Gate가 S5-S7만 지우고 복사하며 내부 플래시 전체를 다시 검사한다. 중간에 전원이 끊기면 완료된 복사 요청에서 재개하고 부분 앱을 실행하지 않는다.

`CFWA.DAT`/`CFWB.DAT`는 각각512KiB, `CFWBOOT.DAT`는64KiB다. 파일의 FAT 위치를 매번 검증하며 고정 raw 주소로 쓰지 않는다. 비활성 헤더가 중간 기록 상태여도 별도의 불변 GID1 소유권으로 활성 파일을 보존한다. 최신 정상 저널을 먼저 지우지 않으며 알 수 없는 완료 버전이나 모호한 세대는 쓰기를 거부한다.

순정 복귀는 검증된 `CFWREC.DAT`와 정확히 승인된 V5.16 APP 및 resident BL 해시를 사용한다. 순정 staging 기록/검증 후 기존 metadata writer와 순정 설치기로 넘어간다. 이 경로는 **Gate까지 제거해 순정 APP 전체로 복귀**하는 동작이다.

최초 Gate 설치와 최종 순정 복귀는 기존 BL을 사용하므로, 그 BL 자체의 metadata/S4 설치 중 전원 상실을 새 Gate가 원자적으로 바꿔 주지는 못한다. 순정 BL 손상, 물리 저장장치 고장, 전원 상실, LSI/IWDG 문제, Gate 자체의 결함은 여전히 SWD 또는 하드웨어 복구가 필요할 수 있다. 모든 상황에서 버튼만으로 복구된다는 보장은 하지 않는다.

S4에 옵션바이트 WRP를 걸지 않았으므로, 잘못된 앱이 직접 FLASH 컨트롤러를 조작해 Gate를 지우는 경우까지 하드웨어적으로 막지는 않는다. 정상 업데이트 경로의 범위 검사와 별개의 한계다. SHA 검증 또한 무결성 검사이며 악성 펌웨어를 판별하는 서명 검증은 아니다.

## 설치 도구와 휴대폰 상태

`tools/gate_bundle.py`는 실제 Release/Debug ELF 예산을 검사한 뒤에만 최초448KiB Gate+Product 번들, 이후384KiB Product 이미지, FAT 컨테이너를 만든다. `bootstrap_recovery_install.py --kind gate-a|gate-b|gate-journal`도 같은 실제 ELF 검사를 요구한다. 현재 실제 Release로 실행하면 산출물 디렉터리를 만들기 전에 거부하는 시험이 통과한다.

`tools/build.ps1`도 이전 app.bin/manifest/메모리 보고서를 안전한 분석 기록 폴더로 옮기고, 주소 검사와 메모리 예산을 통과해야 새 BIN을 내보내도록 변경했다. 실패한 Debug 폴더에 과거 성공 파일이 남아 이번 결과로 오인되는 문제를 막는다. 분석 폴더의 ELF/BIN은 검증 근거이며 설치 승인이 아니다.

최초 FAT 할당에는 현재 장치 전체 NOR의 독립 A/B 백업, FAT 소유권 감사, 변경 전 이미지, 물리 readback을 요구한다. 파일 하나를 생성한 뒤에는 FAT가 달라지므로 이전 백업 증거를 재사용하지 않는다. 자동 포맷이나 기존 파일 교체를 추가하지 않았다.

**휴대폰 앱의 target2 및 최초 Gate+Product 전송 흐름은 아직 연결하지 않았다.** 기존 target0 업데이터에 새 Product BIN을 넣으면 된다고 안내하면 안 된다. 기존0x58 식별 SHA는 Gate+Product448KiB이고, 새 GIM1/target2 SHA는 Product384KiB이므로 서로 혼동하면 안 된다. 실차의 BL0.15/SR0701을 벤치 BL0.14/sr0601과 동일 호환으로 간주하지 않는다.

## 검증 근거와 남은 작업

- Gate: 설치 중단101지점을 포함한106회 시험, 최신 버튼 정책 O0/Os, 실제 저널 writer의40회 중단·손상 시험을 통과했다. 상세 숫자는 `gate-tests/summary.json`과 `gate-critical-path-review.md`에 분리했다.
- Product 업데이트: 실제 ARM36개 상황, O0/Os 각각1,621개 검사.384KiB 전송, 물리 재검증, 중간 실패, 취소 후 재시작, 연결 세대 변경, 주소/실행 문맥/자원 요구사항/저널 모호성 검사를 포함한다.
- 워치독206, metadata4,193, 절전449, NOR63, BT 회귀971개 검사를 O0/Os 각각 통과했다. 무선 송수신 시험은 아니다.
- 자산은 기존16,148B 비트맵과 완전 일치하며 O0/Os/Oz-LTO 각각1,043개 검사, 자산 A/B는514중단 지점 및 이전12개/현재18개 패키지 호환을 검사했다.
- Bootstrap 저장소는 O0 464, Os8,937개 검사와 전체128MiB 계획 이미지 비교를 통과했다. 기존 CFW 설정·주행·사진 저널 회귀도 통과했다.
- 호스트 패키지/차단8개 시험, 관리 빌드 정책 및 격리 재생성 검사를 통과했다. 실제 GUI에서 Cube 재생성을 실행한 것은 아니다.
- Integrated·Graphics는 각각 독립된 소스 복사본과 headless workspace에서 실제 Cube 빌드·링크·이미지 검사를 통과했다. 디렉터리명에 맞춘 바이트 동일 IOC 별칭만 복사본에 추가했다. 이들의 기존 SRAM 배치는 Product와 다르며, 상세 여유와 경고는 `profile-builds/summary.json`에 있다.
- 작업 전 소스 백업과 SHA 목록을 보존했다. `source-change-summary.json`의 보호 파일 검사에서 Core·IOC·vendor 원본 변경이 없다.

최종 프로필 빌드 및 LTO 보고서 결과는 `verification-summary.json`에 기록한다. 실제 30FPS, 힙 최저 여유, 물리 버튼·IGN 반복, 워치독 리셋, 전원 차단, RF 설치 시험은 **미실시**다. 먼저 Release13,592B 및 Debug 링크/예비 공간 부족을 기능 보존 조건 안에서 해결해야 한다. 여유 기준을 낮춰 설치하는 방식은 적용하지 않았다.

최종 Release 컴파일/링크는 오류0, 경고1이다. 경고는 SRAM에서 실행해야 하는 FLASH worker를 포함한 RWX LOAD segment다. LTO 도입 중 나타났던756개의 `.su/.cyclo` peer-target 경고는 ST 플러그인의 고정 outputType을 관리 XML에서 올바르게 모델링해 제거했다. LTO는 최종 링크 단계에서 실제 stack/cyclomatic 보고서를 만들고, 비LTO 경계는 기존 파일별 보고서를 유지한다. 경고 억제나 생성 makefile 편집은 하지 않았다. 링크 성공과 설치 예산 통과는 별개이며, 이 Release는 후자에서 실패한다.
