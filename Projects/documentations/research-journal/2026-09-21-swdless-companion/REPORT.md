# SWD 없는 업데이트·컴패니언 구현 기록 — 2026-09-21

**상태: 일부 구현 및 소프트웨어 검증 완료. 전체 계획 완료·실차 설치 승인이 아니다.**

이번 작업에서는 장치에 연결하거나 MCU/NOR를 쓰지 않았다. 현재 벤치는 이전 `2026-09-21-cfw-gate-install` 설치 상태 그대로다. 기존 실기기 결과를 이 수정본의 검증으로 재사용하지 않는다. OpenNoodoe 원본을 수정하지 않고 NoodoeInstaller 포크에서 작업했다.

## 구현한 경로

### 설치·복구

- Gate 부팅 저널 v2: 이전 정상본, 후보, 시험 부팅, 확정, 한 번의 롤백, 사용자 미확인 결과, 설정 초기화 대기를 구분한다. v1 읽기는 유지한다.
- Product의 고정 contract 필드를 Gate/업데이터/패키저가 검사한다. v2 Gate 인계가 없는 Product는 정상 부팅을 진행하지 않는다.
- 60초 태스크 정상 실행, BT READY, 현재 연결 세대의 정확한 후보 sequence/SHA 확인이 모두 있어야 확정한다. 시스템 오류가 활성화된 상태는 확정하지 않는다.
- 시험 부팅의 BT FAULT·시스템 오류·3분 확인 만료는 저장 owner가 끝난 후 Gate로 리셋한다. 시험 중에는 깊은 STOP을 보류해 BT와 제한 시간이 유지되도록 했다. 실제 절전/무선 타이밍은 미측정이다.
- 최초 설치에는 이전 정상 CFW를 만들지 않는다. 후보 실패 때 이전 정상본이 없으면 WAIT에 남으며 순정 복원은 별도 사용자 확정이다.
- 시험 중 설정 기본값은 RAM view다. 확정 후 환경설정/라이더 이름만 한 번 지우고 완료 epoch를 영구 기록한다. 페어링 키·사진·주행 기록을 설정 초기화 대상으로 넣지 않는다.
- 비활성 APP 파일의 별도 manifest와 4KiB 완료 기록으로 이어 보내기. 재접속 후 실제 NOR에서 완료 섹터 CRC를 다시 검사하고 최종 전체 SHA를 검사한다.
- 일상 업데이트는 Product384KiB만 보낸다. FAT/순정 복구본을 기기에서 읽어 감사하며 매번128MiB 백업을 요구하지 않는다. Gate/순정 BL/옵션바이트/임의 주소 쓰기는 앱 기능으로 제공하지 않는다.
- COMMIT 응답 유실 후 재실행은 상태 조회부터 시작한다. 이미 확정된 전송이면 COMMIT을 반복하지 않는다. 현재 자동 재연결/이어 보내기 UX에는 추가 작업이 남아 있다.
- CFW의 순정 복귀도 UID·BL·Gate 및 저장 복구본 감사 후 요청한다. 전송 중에는 복귀를 거부한다. 앱에 롤백 결과 명시적 확인을 추가했으며 실패 이력은 남는다.

### 기록

- 폰의 구조화 이벤트, 중요 명령 전 fsync 의도 기록, 체크섬 있는 설치 저널, 불명확 결과 복원, 진단 ZIP,64MiB 순환 상한/실패 세션 보호.
- 기기의256KiB CFWLOG.DAT: UID 식별,63개4KiB 순환 레코드와 별도 identity, 물리 readback, 완료 표식 마지막 쓰기. StorageTask writer와 독립 Gate writer가 같은 형식이다.
- 로그 조회는 NDCP 요청/결과 mailbox로 StorageTask에서 처리한다. 파서에서 NOR를 읽지 않는다.
- HardFault는 NOR를 쓰지 않고 별도 봉인한 retained fault를 다음 안전한 단계에서 기록한다. **현재 fault 레지스터·MSP/PSP는 남지만 stacked PC/LR 수집은 아직 없다.**
- 부팅·업데이트·롤백·저장 오류·BT 상태 변화를 기록한다. 태스크 정체/메모리 고갈 세부 이벤트와 내보내기 확인 API는 아직 미완료다.
- 로그 본문에는 알림·곡명·정밀 좌표·비밀 키를 넣지 않는다. RAM 마지막 이벤트까지 완전 전원 상실에 보존하는 것은 아니다.

### 컴패니언 기본 연동

- 단일 foreground service worker/socket 소유. 설치와 주행 연결이 동시에 소켓을 열지 않는다. 현재 설치 전에는 주행 연결을 명시적으로 종료해야 한다.
- 기기 기준 설정 목록 조회·변경·영구 저장 완료 확인. 재연결 때 과거 설정 자동 덮어쓰기 없음.
- 앱별 허용 최신 알림6개 추가/수정/삭제·중복 처리.
- 음악 제목/아티스트/재생 상태와 기존32×32 RGB565 RAM 앨범아트 전송. 기기 미디어 버튼 이벤트 중복 실행 방지 및 Android MediaController 실행 결과 응답.
- IGN ON과 유효한 연결 동안 폰 GPS1Hz 요청, IGN OFF/종료 시 해제. UART 속도를 GPS 속도로 바꾸지 않는다.
- 연결 세대 fencing, bounded retry, 요청 제한 시간,4개 설정 요청 큐.
- Android의 실제 권한/foreground 유지/화면 OFF/GPS 실행은 아직 휴대전화에서 검증하지 않았다. Companion Device association/presence 자동 연결은 미구현이다.

## 아직 구현이 끝나지 않은 항목

1. SPP 사진3슬롯 교체의 전체 앱→기기 영구 저장 경로.
2. 폰4bpp CJK 렌더링·기기 표시·라이더 이름128KiB CFWTEXT A/B 캐시. 파일 이름/예약 mapping만 있고 준비 완료가 아니다.
3. 일상 업데이트에서 자산이 바뀌는 경우의 자산 업로드/이전 버전 자산 보존 전체 경로. 현재 동일 자산 APP 업데이트만 대상으로 한다.
4. Companion Device 기반 자동 시작/연결, 백그라운드 위치 권한 안내·실기기 정책 검증, 전체 네 영역 UX 완성.
5. 물리 로그 파일 자체의 모든 손상 형태에 대한 optional 처리. 로그 레코드/identity 오류는 분리되지만 FAT 파일 크기/교차 연결 손상은 전역 감사 실패가 될 수 있다.
6. stacked fault PC/LR, 모든 health/memory 이벤트, 로그 export ACK, Bootstrap에서의 기기 로그 회수 API.
7. 이어 보내기 초기 scan은 StorageTask에서 수행하지만 최대384KiB 동기 CRC 재검사가 남아 있어 제한 청크 상태기계로 더 나눠야 한다.
8. 실물 GUI Cube 재생성 후 재검사,8시간 부하 및29fps/큐·힙 최저 여유.

Integrated Debug와 Graphics Release도 빌드했다. 실제 GUI 재생성 시험을 대신한 것은 아니다.

이 목록은 단순 실기기 시험만 남았다는 뜻이 아니다. **소스 구현이 남은 기능을 포함한다.**

## 실차 설치 차단

DeviceInfo를 실제로 읽었다는 `target.scope=observed`만으로 설치를 허용하던 조건을 강화했다. 현재 패키저는 `target.deployment=blocked-pending-hardware-validation`을 기록하며 installable=false다. 앱은 별도 no-SWD 검증 완료 표시가 없는 패키지를 거부한다. 시험 fixture의 통과 값은 실제 검증 증거가 아니다.

다음은 아직 없다.

- 실차 BL0.15/SR0701과 해당 실차 순정 복구본의 호환성 실증.
- 정상 무선 기판의 순정→Bootstrap→CFW→다음CFW→실패롤백→순정 전체 왕복.
- BT 없이 버튼만으로 복귀하는 최신 수정본의 물리 시험.
- 순정 BL 최초 설치·순정 복원 도중 전원 중단의 재개/복구 실증.
-2분 업데이트 및8시간/29fps 목표 실측.

벤치의 고장난 BT를 모의시험으로 정상 판정하지 않는다. 실차용 설치 패키지를 배포하지 않았다.

## 증거와 잔량

최종 빌드·시험 수치 및 아티팩트 SHA는 옆의 `verification-summary.json`을 따른다. `product-*-final.log`, `android-final.log`, `gate-tests-final.log`는 실제 실행 기록이다. ARM 시험은 실제 컴파일된 C의 에뮬레이션이며 물리 NOR/무선/패널을 검증하지 않는다.

Debug 여유가 기준에 아주 가까워 추가 기능 전에 더 큰 코드 공통화가 필요하다. 예산·힙·스택·UI 품질 기준을 낮추지 않았다. 이번 정리는 부팅 레코드의 명시적 필드 매핑과 공통 header 검증이며 구조체를 그대로 디스크에 저장하는 방식으로 바꾸지 않았다. 독립 Python LE/CRC fixture로 wire bytes를 비교한다.

## 발견하고 고친 빌드 캐시 결함

기존 Bootstrap 캐시는 wrapper C와 include 디렉터리의 최상위 헤더 mtime만 보았다. `gate_format_shared.c`가 포함하는 `gate_format.c`나 assembler `.incbin`의 압축 순정 이미지가 달라도 stale object를 재사용할 수 있었다. `tools/build_inputs.py`에서 중첩 헤더·quoted include C/INC·incbin을 내용으로 지문 계산하도록 고쳤다. 동일 mtime으로 내용을 바꿔도 캐시가 무효화되는 시험을 추가했다. Bootstrap을 다시 빌드했으며 최종 SHA는 검증 JSON에 있다. 단순 clean 한 번으로 숨긴 문제가 아니다.

기존 v1 벤치 Gate/Product를 이 버전의 일상 업데이트로 자동 이관하는 경로는 제공하지 않는다. 새 v2 초기 설치/마이그레이션 절차가 필요하며, 기존 고정 파일과 충돌하면 create-only 설치기는 덮어쓰지 않고 중단한다.
