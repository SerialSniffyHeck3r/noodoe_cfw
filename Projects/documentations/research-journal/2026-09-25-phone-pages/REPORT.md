# Companion / CFW 6.11.0 — 전화·알림·GPS·시스템 메뉴

기존 앱 위에 APK를 덮어 설치한 뒤, 주행 연결을 수동 중지하고 대응 ZIP으로 **일반 CFW 업데이트**를 진행하세요. 이 릴리스 때문에 순정 복귀나 Bootstrap/Gate 재설치를 할 필요는 없습니다. 기존 APK 서명을 유지합니다.

## 변경 사항

- 최근 통화는 한 화면에 한 건씩 큰 이름·전화번호·통화 시각을 표시합니다. 수신은 초록색 화살표, 발신은 파란색 화살표, 부재중은 빨간색 표시입니다. 제목은 고정하고 선택 항목만 이동합니다.
- 전화 목록 조회 권한과 발신·받기·종료 권한을 분리했습니다. 통화 제어 권한이 없다는 이유로 읽을 수 있는 목록까지 `Permission Denied!`로 가리지 않습니다. 실제 통화 제어에는 Android의 별도 승인이 여전히 필요합니다.
- 휴대전화 페이지는 최신 알림부터 바로 표시하고 최대10개를 순환합니다. 없으면 `No notification`을 표시합니다. 답장 대상과 기존 주행 중 조작 제한은 유지합니다.
- GPS의 선택적 fused 공급자 오류가 정상 GNSS 구독까지 취소하던 경로를 수정했습니다. 위치 수신이 늦다고 구독 전체를 반복 종료하지 않고 제한된 새 위치 요청으로 보완합니다. 권한 철회 시 위치를 폐기하고 오래된 좌표를 최신으로 위장하지 않습니다.
- Settings → System → `Restart Noodoe`: 확인 후 설정·주행 기록을 저장하고 본체를 재부팅합니다. 저장 실패 또는 업데이트 시험 중에는 강제 재부팅하지 않습니다.
- **Settings → System에서 UP을 짧게20번 누르기**: `See dev message`가 나타납니다. 선택하면 **`this is easter egg!`**를 표시합니다. O로 돌아가며, 길게 눌러 반복되는 입력은 횟수에 포함하지 않습니다. 재부팅하면 다시 숨겨집니다.

## 검증

- Android 표적 시험 **138개** 통과, lint 오류0.
- 실제 ARM 코드 모의시험: 설정339개 검사 × O0/Os, 재부팅 저장 경계, 전화 스키마1/2, 알림·입력·전원 상태기계, 업데이트98개와 시험부팅14개 회귀 통과.
- 실제 LVGL 코드 + 모의 EVE I/O: 전환2,016프레임, 최대7,804/8,192B. 실제 LCD 캡처나 실측FPS가 아닙니다.
- Release/Debug 플래시 여유 **66,356 / 34,360B**, 일반 SRAM **52,472 / 53,432B**, CCM **16,320B**. 기존 힙·스택·큐 용량과 예산 유지.
- 실제 Android ZIP importer의 새/구 패키지 검사, 최종 ELF와 패키지 APP 일치, APK 서명 일치 확인.
- Bootstrap·Gate·순정 복구본·자산·삭제/진단 펌웨어는6.10.4와 바이트 동일합니다.

**실제 S24 Ultra 화면 종료 상태의 GPS 수신, 실차 통화·RF·전원 차단 시험은 이번 작업에서 수행하지 않았습니다.** 모의시험으로 재현한 코드 결함은 수정했지만, Samsung 절전 정책을 포함한 실제 수신 결과까지 검증됐다고 주장하지 않습니다.

## 원인·구현 기록

- `PhoneLocationFeed`: 공급자별 독립 등록, 성공한 구독 유지,30초 재시도 간격·10초 취소 가능한 비동기 current-location. 로케이션 서비스와 IGN의 소유권은 유지하며 Activity/화면 상태에 종속하지 않습니다.5개 Android 위치 시험에 screen-off, optional-provider 실패, 권한 철회와 요청 중 SecurityException, timestamp 보존을 포함합니다.
- `PhoneCalls`/`PhonePanelRenderer`: CallLog DATE/TYPE 조회,288×128/18KiB A4 안에 이름36px·번호28px·날짜18px 배치. 숫자·이름 원문은 일반 로그/NDCP metadata에 넣지 않고 이미지를 보냅니다. 기존3줄 목록에서 한 선택 카드로 바꿨으며 폰트 자산·화면 버퍼는 늘리지 않았습니다.
- NDCP capability0x20000일 때 전화 schema2, 기존40+8N 크기의 offset36에 현재 visual_target의 통화 종류0..7. 이전 기기에는 schema1/reserved0을 보냅니다. 펌웨어는 둘 다 검사합니다.
- `settings_system.c`: 숨은 항목 해제는 RAM에만 유지. 재부팅은 UI의 비동기 요청을 StorageTask가 확인하여 설정/주행 저장 성공 후 NVIC_SystemReset을 호출합니다. Gate journal CONFIRMED만 허용,10초 실패 상한; NOR 트랜잭션을 중간에 끊지 않습니다.
- `ui_phone`, `ui_dashboard`, `product_ui`: 별도 빈 시작 페이지를 없애고 선택된 알림ID와 답장 수신자 경계를 유지합니다.
- 메모리: BT assert의 절대 경로/중복 함수명 문자열은 basename+line+failed-expression으로 대체하며 실제 assert를 제거하지 않았습니다. 프로젝트 소유 EVE assert도 basename 사용. bounded 거리표시는32비트 나눗셈, 공용 문자열 복사는중복 인라인을 줄였습니다. vendor/Cube 생성 소스는 수정하지 않았습니다.

## 근거 파일

- `verification-summary.json`: 버전·서명·해시·시험·예산.
- `changes.diff` / `source-hashes.json`: 로컬 변경 및 최종 소스 해시. 비UTF8 주석은 diff에서 backslash escape하며 원본을 재인코딩하지 않았습니다.
- `recent-call-panel.png`: 테스트용 일본어 이름/번호의 Robolectric 렌더 결과. 실기기 캡처 아님.
- `final-*.log`, `*-results.json`: 실제 ARM 코드 모의시험. Android는 `android-verified.log`.
- APK SHA256 `c3666446426694569c898182ee22196bfce2986d7e750be5bbd076b0eab6928e`
- ZIP SHA256 `67ab14d579c5aaaaf3cc8340c43e6ff70848d8035903a151a728d2ad19dbf3e4`

Bootstrap/Gate/설치 자산 변경 없음. 순정 BL·공장 영역·현재 연결된 기기·OpenNoodoe 원본·GUI·ST-LINK는 접근하지 않았습니다.
