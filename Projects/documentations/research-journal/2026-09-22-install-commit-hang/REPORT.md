# 설치 승인 뒤 연결 단절 / 복구 강화 — 2026-09-22

이번 작업은 코드·빌드·ARM 에뮬레이션만 수행했다. 사용자 요청에 따라 연결된 벤치, ST-LINK, UART, ADB, GUI에는 접근하지 않았다. 실차의 실제 사건 로그와 리셋 원인은 확보하지 못했다.

## 확인된 결함과 원인

1. `bootstrap_ui.c`의 전역 진행 표시가 설치 확인창의 `line2`까지 덮어썼다. 원래 `Back / Install CFW`가 표시될 줄에 단계·파일·섹터가 나오므로 기본 Back 선택이 보이지 않았다. 선택은 유지하고 진행 정보만 다음 줄로 옮겼다.
2. 확인창에서 Back으로 안전 취소한 뒤 앱이 소켓을 닫으면 작업이 없는데도 UI가 PAUSED로 들어갔다. 세션 소유자가 이미 사라져 O 3초 취소도 소비되지 않았다. CPU는 정상 루프를 돌며 건강 감시를 통과하므로 워치독이 발생하지 않는 상태였다. 실제 ARM UI 시험에서 기존 코드 실패를 재현했다. 이제 활성 작업이 있을 때만 연결 단절 대기를 유지한다.
3. 명시적 RESET의 ACK를 보낸 뒤 1.5초 안에 소켓이 끊기면 UPDATE_RESET_WAIT가 COMMITTED로 돌아가 재시작이 사라졌다. RESET adapter도 살아 있는 연결을 요구했다. 다른 응답 전송이 ACK 시각을 덮어쓰는 문제도 있었다. ACK는 해당 seq/연결 세대의 첫 전송 완료만 고정하고, 이후 단절·STATUS·중복 ACK가 이미 승인된 재시작을 취소하지 못하게 했다.
4. 내부 설치 메타데이터 쓰기와 실제 readback이 성공했는데 뒤따른 Bluetooth 재개가 실패하면 메타데이터까지 실패/불명확으로 취급했다. 저장 결과와 HCI 결과를 분리해 성공한 기록을 다시 소거하지 않으며 진단의 `resume_result`를 보존한다.
5. `HealthService_Progress`가 늦게 도착한 보고로 과거 timestamp를 먼저 바꿔 버려 감시 누락을 숨길 수 있었다. 이전 보고의 기한부터 검사하고 실패를 고정한다. 제한된 FLASH 작업 완료의 기존 일회성 유예는 유지한다. 타깃 검사 최대 1.5초와 HCI drain 최대 1초는 각각 완료된 단계 사이에서 owner 건강을 확인하므로 합쳐서 허위 지연 실패를 만들지 않는다.
6. reset adapter가 검증 실패로 반환해도 화면은 영원히 설치 중으로 남을 수 있었다. 로컬 및 명시적 RESET 경로 모두 반환을 오류로 고정하며 쓰기/리셋 자동 재시도는 하지 않는다.

사용자가 보고한 '다시 연결 / O 3초' 화면은 성공한 COMMIT 화면과 다르다. 특히 1·2번이 그 증상을 만들 수 있으나 **현장 사건의 단일 원인으로 확정하지 않는다**. 3·4번도 별개의 실제 코드 결함으로 수정했다.

## 새 설치 동작

Bootstrap capability 0x86의 bit6은 물리 승인 후 로컬 설치 지원을 뜻한다. Product는 해당 기능을 광고하지 않는다.

`UpdateService_ConfirmLocal()`은 전부 받은 이미지, 검증된 SHA, 트랜잭션, 불명확한 COMMIT 없음, reset callback을 요구한다. Bootstrap의 실제 O 확인 이벤트만 이 API를 호출한다. 기존 순정 BL/옵션바이트를 수정하지 않는다.

승인 이후에는 연결 세대나 송신 큐와 독립적으로 다음을 수행한다.

1. NOR 전체 448KiB를 4KiB씩 다시 읽어 SHA256·CRC·벡터를 검사한다.
2. 이미 준비한 Product A/B·부팅 저널·자산 의존성과 설치 메타데이터를 대조한다.
3. 공용 bounded metadata writer로 설치 요청을 기록하고 물리 readback을 확인한다.
4. 1.5초 후 동일 pending 메타데이터를 재확인하고 MCU를 재시작한다.

이 동안 새 원격 변경 명령은 거절한다. 상태·읽기 진단은 가능하다. 실패한 검증이나 COMMIT을 반복하거나, 폰이 보내지 않은 RESET ACK를 만들어내지 않는다. 이미 설치가 확정됐다면 O 길게 누르기로 그 기록을 지우지 않는다.

앱은 새 기능을 협상하면 COMMIT/RESET을 추가로 보내지 않는다. O 승인 뒤 재접속 및 Product 정상 실행 확인만 수행한다. 통신 단절 때는 결과 미확인으로 영구 기록한 뒤 조회하며 설치 성공을 추측하지 않는다. 구형 Bootstrap에는 기존 명시적 명령 경로를 유지한다.

## 재전송 없는 재시도

`BootstrapContinuation`은 UID/실행 Bootstrap 식별이 끝난 뒤 동일 ZIP의 journal·transaction·버전·CRC·크기·SHA 및 pending 메타데이터를 대조한다.

- 완전한 VERIFIED 이미지: FINISH로 전체 물리 readback만 다시 수행한다.
- 마지막 DATA 이후 또는 검증 중 링크가 끊어진 완전한 이미지: 같은 RAM manifest의 NOT_CONNECTED 실패에만 FINISH 재검증을 허용한다. BEGIN/DATA/erase/program은 보내지 않는다.
- 이미 COMMITTED/RESET_WAIT: 재시작 결과 조회로 넘어가며 COMMIT/RESET을 반복하지 않는다.
- 부분 파일·다른 기기/ZIP·해시 불일치·불명확한 메타데이터: 이 지름길로 설치하지 않는다.

완전 MCU 리셋 뒤 RAM manifest가 사라지면 이 경로를 적용하지 않는다. 기존 FAT·컨테이너 재검사가 필요하다. 이를 영구 재개 저널 구현으로 설명하지 않는다.

## 복구와 워치독

기존 `GateGesture`와 순정 복구 경로는 유지했다. 키 OFF에서 O를 놓았다가 1초 유지하고, O를 누른 채 키 ON 후 3초 더 유지한다. 일반 복구 메뉴에서는 O를 놓았다가 새로 2초 유지한다. 실제 설치 기록이 불명확하면 무단 덮어쓰기를 시도하지 않는다.

IWDG 설정은 PR=6 (/256), RLR=4095다. 32kHz 기준 32.768초, LSI 17–47kHz 범위에서 약 22.31–61.69초다. **F429에서 정확한 60초 IWDG timeout은 지원되지 않는다.** 소프트웨어 카운터로 '1분'을 가장하지 않는다. [ST RM0090](https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf).

공용 BSP 변경으로 Bootstrap·Gate·Product에 모두 반영한다. RAM/data 초기화 전 조기 시작, 정상 owner 완료에 따른 refresh, IRQ에서 feed 금지, FLASH의 독립 DWT 제한은 유지한다. 지연·무한 루프에서 감시 실패를 숨기지 않는다. 순정 BL에 진입한 이후의 리셋/시계/워치독 정책은 이번 코드의 통제 범위와 구분한다.

## 검증 및 배포

최종 숫자·SHA·메모리 여유는 `verification-summary.json`, 실제 시험 결과는 `test-results.json`에 있다. 다음을 실제 ARM 코드와 모의 I/O로 검사했다.

- 단절 전/후 RESET ACK, 틀린 seq, 중복 ACK, uint32 tick wrap, 재시작 callback 반환.
- 완전 수신 뒤 단절 및 무선 없이 물리 승인, 송신 큐가 가득 찬 경우, HCI resume 실패.
- 저장 중 데이터 손상, 메타데이터 불일치, 승인 중복, 불명확한 결과의 무단 재시도 금지.
- 취소 후 소켓 종료, 확인 선택 표시, 오프라인 재확인, 기존 비상 IGN+O 제스처.
- 워치독 조기 시작, 늦은 owner 보고, 제한된 FLASH 예외, IRQ refresh 금지.
- Product A/B 업데이트, 부트 저널 경계·resume, 독립 Gate retained handoff, 순정 복원 core.
- Android journal·재시도·기기 격리·UI 및 기존 관련 단위시험.

Bootstrap은 여유가 작아졌으므로 PC의 stock raw-DEFLATE encoder `memLevel`을 8→9로 변경했다. 타깃 해제기·window·기능·데이터 형식은 같고, 원래 순정 APP 448KiB와 SHA가 동일함을 실제 ARM 해제기로 검사했다. 기능/큐/힙을 제거해 맞추지 않았다.

최종 ZIP에는 새 Bootstrap·Gate·Release Product·동일 순정 APP·동일 NOODOE.RSC를 넣는다. Gate 교체가 있으므로 순정 복귀 → 새 Bootstrap → Gate+CFW 경로를 사용한다. 원본 OpenNoodoe, 순정 BL, 옵션바이트, 기존 자산 형식은 수정하지 않았다.

이번 시험은 물리 NOR/무선·RTOS 선점·전원 차단 및 실제 리셋 성공을 보증하지 않는다. 기존 60초 정상 실행/앱 재연결/3분 제한/롤백 조건을 유지한다. '모든 결함 제거', '어떤 고장에도 무조건 무분해 복구'라고 주장하지 않는다.

## 최종 산출물

- APK6.5.2/code8: `NoodoeCompanion-6.5.2.apk`, 기존 개발서명 유지. Android164/0fail.
- Bootstrap458684B/free68B, Gate22492B/free43044B. Bootstrap 추가 기능 여유가 사실상 소진됐다. 이후 기능 추가는 별도 크기 설계가 필요하다.
- Product Release324856B/free68360B, Debug357240B/free35976B. 일반 SRAM free56344/54888B, CCM16320B. 힙/큐/스택/예산 축소 없음.
- ZIP `vehicle015-final/installer.zip`, SHA256 `00975f1fd2072beee81a18b14954c497ebc902fb44f48dd81b0424b8914a462b`. 실제 Android production importer 통과.
- ARM Bootstrap adapter O0/Os 각45644 assertions, 공용 protocol각23388, Product update78 scenarios, UI50, watchdog각210, Gate handoff7, recovery core각36.
- 전송 파일/펌웨어/순정/resource SHA와 source diff는 같은 폴더에 보존. APK 서명은 `apk-signature.txt`.
