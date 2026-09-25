# 00050006 / Bootstrap resource migration — 2026-09-22

## 결론과 사건 구분

사용자는 `00050006` 재발만 보고했다. 발생 단계·앱 마지막 오류·현장 로그 질문은 아직 답변이 없다. 따라서 사용자 사건의 원인을 확정하지 않는다. 코드는 이 오류를 `BOOT_ERR_STORAGE | BS_FORMAT`으로 만든다. 자산뿐 아니라 FAT·컨테이너·내용 검사 실패에도 반환되므로 MCU fault나 watchdog reset이라는 뜻은 아니다.

출시본 6.3.2/6.4의 정상 자산 ID는 `a933141bd995d94a4fe62c8fac693bcf5aca06689a139a2357cad81261036694`, 최신 Product 요구 ID는 `45b5308e2adf6c4333be46fc09ed8e7a9af039dcff895c27428fdc7680b984ce`다. 이전 NOODOE.RSC를 순정 왕복 후 그대로 재사용하면 기존 `Content()`는 새 요구 ID와 다르다는 이유로 BS_FORMAT을 반환했다. 앱 scoped-v2 재사용 분기는 이를 이관하지 않고 검증만 요청했다. 실제 출시 자산과 ARM validator를 O0/Os로 실행해 old-only→6, new→0, oldA/newB→0을 재현했다 (`resource-probe-results.json`).

별개로 Android `ResourceUpdateSession.slot()`의 완료 표식이 저널용 `0x31544d43`으로 잘못되어 있었다. 자산용 `0x434d5431`로 고쳤고, 잘못된 상수를 반복한 기존 단위시험도 수정했다.

## 구현

- Bootstrap capability bit5와 scoped-v2 `0x88` kind0으로 기존 자산 이관을 명시한다. 이전 Bootstrap에는 지원 없는 명령을 보내지 않고 새 Bootstrap 설치 필요를 안내한다.
- FAT 전체 소유권·정확한 기존 1MiB extent·순정 복구본을 확인한 뒤 폰이 물리 원본을 저장한다. 기기도 전체 원본 SHA를 재확인한다.
- 기존 유효 슬롯 하나의 헤더와 전체 body를 검증·보존한다. 반대 512KiB만 새 요구 ID로 교체한다. 대상 원본 SHA와 FAT도 쓰기 전에 다시 대조한다.
- body와 미완료 헤더 기록, 물리 SHA 재읽기, commit-last, 최종 전체 재읽기를 유지한다. 보존 슬롯·FAT·순정·다른 파일의 쓰기 권한을 넓히지 않는다.
- 기존 SWD 자산 이관은 valid-A/erased-B 제약을 유지한다. 양쪽 모두 손상됐거나 선택한 이전 슬롯의 body가 손상됐으면 쓰기 전에 거절하며 자동 포맷하지 않는다.
- 새 요청에서 사용하는 전체 컨테이너 검사(1MiB), 슬롯 재읽기(512KiB), FAT 검사(36KiB)의 진행률 범위를 구별한다. 이전의 metadata-phase 가정을 자산 writer에 적용하지 않는다.
- 오류 화면·앱에 파일 번호 및 phase/subphase를 남긴다. 실패한 변경 명령을 진단 목적으로 재실행하지 않는다.

## 검증

- Android `testDebugUnitTest assembleDebug`: 158 tests / failures0 / errors0 / skipped0.
- ARM 자산 이관 11사례: oldA/emptyB, oldA/interruptedB, invalidA/oldB, two-old-slots, wrong preimage, invalid headers, corrupt body, writer phases1/2/3/4 중단 후 새 감사로 재시도.
- 성공마다 전체 NOR 모델에서 대상 슬롯 바깥 모든 바이트 불변 확인. 손상 거부는 mutation0. 중단 시험은 기존 완료 슬롯 보존.
- scoped-v2 기존 9파일 생성/로컬 복사/재설치/3가지 중단/5개 단절 구간 회귀 통과. 전체 NOR 해시가 재도입되지 않았음을 검증.
- Bootstrap UI ARM 50사례 O0/Os 통과. resource progress의 실제 work span·InstallSession snapshot 경계 O0/Os 통과.
- 최종 ZIP을 실제 APK의 `RecoveryBundle` importer가 수락. 실제 `ResourceUpdateSession.slot()`은 106,496B 자산 데이터 선택 성공.
- APK6.5.1/versionCode7, io.noodoe.installer, 기존 개발 서명 SHA256 `5d268d352019cc1d394296e0ea0a13495daecee038f58def0ae7036fc7c64bae` 유지.
- 새 Bootstrap 458,212B, APP 범위 내 여유540B. 순정 BL/옵션바이트·Gate/제품 메모리 예산 완화 없음. Product/Gate/stock/resources는 직전 BTSettings 패키지와 바이트 동일.

ARM storage fixture는 NOR를 메모리로 모의하고 SHA·메모리 복사를 호스트에서 가속한다. MCU 실행시간·RF 처리량·실제 전원 차단 시험이 아니다. UI fixture는 runtime hardware loop를 대신하지 않는다. 이번 턴 장치 접근·설치·Cube GUI재생성 없음. ADB 연결 폰도 없었다.

## 전달

GitHub 기존 비공개 저장소 `SerialSniffyHeck3r/noodoe_cfw`, tag `companion-v6.5.1-resource-fix`에 APK와 ResourceFix ZIP 및 안내·SHA256을 게시했다. 업로드 후 GitHub asset digest를 로컬과 대조했다 (`github-release.json`).

이전 Bootstrap에서 막힌 사용자는 순정 복귀 → 새 APK와 새 ZIP 선택 → 새 Bootstrap 전송 → CFW 설치 계속 순서가 필요하다. APK만 바꿔서는 기존 Bootstrap의 코드가 바뀌지 않는다. 정상 Product는 동일 바이너리이므로 이번 오류 수정 때문에 다시 설치할 필요가 없다. 실제 사건이 다른 파일/단계에서 발생했다면 이번 재현과 구별해 후속 분석한다.

## 후속 사용자 사진 확인

사용자가 `8a850d50-54f5-4acc-95e0-c17842fb5fa4/1-Photo-1.jpg`를 제공했다. 화면에는 단계2/8(25%), CFW 파일 준비·기록·검증, 중단 위치1,044,480/1,048,576B, 경과2:54/현재단계0:04, 마지막응답0초 전, `Scoped storage operation failed`, ZIP `a6942f62d6af…`가 보인다. 이 SHA 접두사는 직전 BTSettings 릴리스와 일치하며 새 ResourceFix ZIP `e671d65cac66…`가 아니다. 이 사진은 수정본에서 재발했다는 증거가 아니다.

중단 위치는1MiB에서4KiB를 남긴 마지막 표시값이다. 기존 ARM INSPECTING은 마지막4KiB 읽기 직후 Content 검사에서 BS_FORMAT을 반환할 수 있어, 재현한 old-resource 실패와 부합한다. 다만 사진에는 file kind/phase나 자산 ID가 없고 CFWPIC 역시1MiB이므로 사진만으로 NOODOE.RSC라고 확정하지 않는다. 이전에 미확정이던 작업 단계는 이제 파일 준비·검증으로 좁혀졌으며, 정확한 파일과 validator는 여전히 로그가 필요하다. 이번 제공 사진 확인에 따른 추가 펌웨어 변경·장치 접근은 없다.
