# 설치 오류 0006000B / 순정 복귀 지연 수정 — 6.3.2

사용자 증거: 전송·검증 이후 휴대폰 설치 화면 버튼을 연속 두 번 누른 뒤 `Update wasn't confirmed / 0006000B`. Back to stock가 오래 멈춘 뒤 재부팅했고 두 번째 시도에서 순정 복귀. 기기 O 버튼 이중 입력으로 해석하지 않는다. 현장 세션 로그와 리셋 레지스터는 확보하지 않았으므로 최초 COMMIT 실패의 정확한 지점과 실제 워치독 원인은 미확정이다.

## 확인한 결함과 수정

1. `Update_Service.c`는 metadata_commit의 모든 실패를 AMBIGUOUS로 분류했다. Bluetooth 정지 실패나 쓰기 승인 전 실패에도 순정 복귀의 CancelUncommitted가 거절됐다. 이제 어댑터가 미기록을 증명하고 현재 resident metadata에 pending 요청이 없을 때만 REJECTED(12)로 분리한다. destructive 여부가 불명확하면 계속 차단한다.
2. STATUS/후속 거절이 처음 실패 코드를 덮어쓸 수 있었다. FAILED의 원인과 commit_uncertain을 유지한다. STATUS, 재COMMIT, ABORT, BEGIN으로 불확실 상태를 지우지 않는다.
3. Bootstrap의 복귀 대기 코드는 위 거절 이후 60초가 지나면 의도적으로 Watchdog_Fail과 무한 루프를 실행했다. 이를 독립 drain 정책으로 바꿨다. 미기록을 증명한 경우 기존 confirmed recovery intent 경로로 진입하고, 불명확·저장소 미종료는 고정 오류 화면과 상세 정보를 유지한다. 진짜 CPU/owner 고장을 감시하는 워치독은 유지한다.
4. 목표 이미지 검사/BT pause 이후에도 metadata writer는 250ms 이내 정상 health checkpoint를 요구했다. bounded 작업 완료 뒤 모든 등록 owner를 다시 검사하여 FLASH lease 직전 freshness를 확보한다. 250ms 제한이나 FLASH 5초 절대 제한을 늘리지 않았다. 최초 현장 실패가 이 원인이었다는 확정은 아니다.
5. 폰 service에는 이미 busy 직렬화가 있었다. UI의 확인창 중첩과 비동기 상태 표시 전 두 번째 클릭을 추가 차단했다. ‘사용자가 두 번 눌렀기 때문에 펌웨어가 고장났다’고 판단하지 않는다.
6. 읽기 전용 NDCP 0x85/schema1/48B를 추가했다. 단계·결과·상세·destructive·writer·BT resume·update state·first error·uncertain·recovery를 제공한다. 폰은 상관관계가 확인된 COMMIT 거절에서만 이를 읽고 journal 및 구조화 로그에 수치만 남긴다. COMMIT timeout에서 스트림을 재사용하거나 COMMIT/RESET을 반복하지 않는다.

## 순정 복귀 후 설치 재시도

순정 APP로 돌아가도 이미 준비한 9개의 CFW 파일은 남을 수 있다. 파일명 충돌을 무시하거나 삭제하지 않는다.

- 0개: 기존 새 파일 생성 절차.
- 정확히 9개: 현재 FAT 소유권·크기·연속 체인·예약 영역 경계 검사를 다시 수행한다. 폰에서 resources/stock recovery/A/B Product/initial boot journal의 전체 바이트를 ZIP으로부터 계산한 값과 비교한다. 설정·누계·사진·로그는 보존하고 기기측 Content 검사로 UID/형식을 검증한다.
- 일부만 존재하거나 다른 Product/저널이면 명시적으로 중단한다. 부분 FAT 공개 자동 복구, 기존 파일 덮어쓰기, 강제 포맷은 추가하지 않았다.
- `0x81` 72B RUSE는 새 읽기 전용 baseline이다. 순정 왕복으로 바뀐 OTA staging을 옛 해시가 그대로라고 주장하지 않는다. 이전 증거 디렉터리는 그대로 남기며 새 디렉터리에 물리 읽기 및 검증 결과를 보관한다. 기기에서 CREATE 요청도 거절한다.
- 최종 내용 검사와 변경 제외 영역 SHA 검증 뒤에만 기존 OTA staging 권한을 부여한다. 정상 설치 확정 후 설정/CFW 사진 초기화 정책은 그대로다.

## 배포 파일과 사용 순서

- `NoodoeCompanion-6.3.2.apk`: 기존 앱 위에 설치. 서명 인증서는 기존 버전과 동일. 앱 삭제/데이터 삭제 불필요.
- `NoodoeInstaller-6.3.2-BootstrapFix.zip`: 새 Bootstrap을 포함한 대응 ZIP. 현재 사용자 기기는 순정으로 복귀했으므로 1. 새 ZIP 선택 → 2. 순정에서 설치 도구 전송 → 앱의 키 조작 안내 → 3. CFW 설치 계속.
- 이번 ZIP의 **Product 6.3, Gate, 순정 APP, resources는 직전 3e0f8bb6a724… ZIP과 바이트가 동일**하다. 앞서 준비된 파일을 검증해 재사용하기 위한 Bootstrap 수리 패키지다. 새 Bootstrap SHA는 bbe07ef4957f…이다. 새 ZIP을 옛 Bootstrap에 강제 적용하지 않는다.
- 현재 공통 소스 Product Release/Debug도 별도 빌드·예산 검사를 통과했다. 그 결과는 `current-source-build`에 보관했으며 이번 수리 ZIP의 Product를 그 빌드로 바꾸지 않았다.

## 검증

- Android 149시험 실패 0. 중복 확인창, COMMIT 거절 상세 조회/로그, 불명확 상태 무재시도, 기존 컨테이너 재사용/상이한 Product 거절/부분 파일 거절 포함.
- 실제 ARM 어댑터와 UpdateService O0/Os: BT pause 실패, health 거절, erase 전 metadata 거절→복귀 허용; write 후 resume 실패→불명확 유지; STATUS/재COMMIT/ABORT로 불명확 삭제 불가; storage drain 제한 검증.
- ARM Bootstrap UI O0/Os, 공용 프로토콜 회귀 통과.
- ARM 저장소 전체 9파일 생성 및 재접속 시험, 쓰기 각 단계 9개 timeout 취소, stock/staging 변경 후 읽기 전용 재사용, 잘못된 scope 거절, NOR 불변 검증 통과. SHA/memcpy는 동일 의미의 호스트 가속이며 RF 처리시간 시험이 아니다.
- 새 Bootstrap 444,612B / 여유 14,140B. 현재 소스 Product Release 여유 79,244B / Debug 47,784B, 일반 SRAM 여유 57,376B / 55,920B, CCM 16,320B. 힙/스택/용량 기준 축소 없음.
- 실제 최종 ZIP을 Android production importer로 읽고 기존 실차 식별 응답과 대조하여 통과. Product/Gate/resources/stock의 이전 ZIP 대비 바이트 동일성을 별도로 검증했다.
- 실제 기기 접근·설치·무선 시험은 이번에 하지 않았다. 전원 차단이나 실제 BT pause/resume 타이밍, 실제 0006000B의 최초 실패 위치까지 재현했다고 주장하지 않는다. 상세 수치·해시는 verification-summary.json 참조.

완료되지 않은 실제 기록을 성공으로 바꾸거나 진짜 ambiguous metadata를 자동 덮어쓰는 변경은 하지 않았다. 해당 경우에는 고정 오류/로그를 통해 다음 판단 근거를 남긴다.
