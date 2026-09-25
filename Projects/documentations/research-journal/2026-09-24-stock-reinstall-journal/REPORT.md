# 순정 복귀 완료 뒤 Bootstrap 재설치 차단 수정

## 원인

기기에서 순정 실행이 확인되면 `InstallerController`는 `STOCK_RETURN_CONFIRMED`를 저널에 저장한다. 반면 `StockUpdateSession.installBootstrap()`은 `IMPORTED`와 `STOCK_IDENTIFIED`만 허용했다. 같은 기기·같은 ZIP의 저널을 다시 사용하는 정상적인 Gate 이관/재설치가 첫 전송 이전에 차단됐다.

`stock-return-check`도 완료 상태를 허용하지 않아 문제 해결 마법사에서 재확인하면 다시 차단됐다. `InstallerFailure`는 메시지의 `journal`이라는 단어만으로 저장 공간/기록 오류 안내와 결과 미확인 경고를 붙여 실제 상황을 흐렸다.

## 구현

- 새 설치는 완료 상태에서만 허용한다. 순정 응답·선택 기기의 MAC·패키지의 HW/BL/APP/모델/PCBA·더 높은 Bootstrap 버전·정차 및 IGN ON을 읽어 검증한 뒤 새 작업을 시작한다.
- `InstallJournal.restartAfterStockReturn()`는 이전 Properties 전체를 `attempt-history/<UUID>.journal`에 저장하고 fsync·체크섬 검증·읽기 대조·디렉터리 동기화를 수행한다.
- 새 활성 저널에는 address/bundle/UID/resident 원본 증거만 승계한다. 이전 전송 offset·transaction·생성 중 파일·backup 경로·복원 모드는 보관본에 남기고 활성 작업에서 제거한다. 보관본 경로·체크섬과 새 attempt ID를 기록한다.
- 보관 실패는 첫 원격 쓰기 전에 중단한다. 새 작업 준비 후 앱이 종료되면 다시 순정 사전 검사를 수행한다. BEGIN 전송 후 응답이 끊기면 결과 미확인으로 남아 재전송을 차단한다.
- `stock-return-check`에 완료 상태를 추가해 반복 확인을 허용한다. 확인마다 실제 기기의 순정 식별값을 읽으며 쓰기 명령을 보내지 않는다.
- 상태 차단은 별도 예외로 구분한다. 저장 용량 부족으로 잘못 안내하지 않는다. 순정 복귀 후 마법사 제목도 재설치로 표시한다.

## 회귀 시험

`StockReinstallTest` 7개와 마법사의 이전 APK 오류 상태 복원 시험을 추가했다.

- 순정 복귀 완료 → 새 Bootstrap 전체 전송, 이전 기록/백업/UID 보존, 전송 상태 분리.
- 다른 MAC·다른 HW·이동 중·IGN OFF에서 보관/전송 차단.
- STOCK/NDCP/Product/Uninstall 결과 미확인 상태의 무단 재실행 차단.
- 완료 기록 보관 실패 시 원격 쓰기 0회.
- 보관 이후 재시작, BEGIN 응답 유실 이후 재시작과 재전송 차단.
- 실제 Controller 경로에서 순정 복귀 확인 2회 → 재설치.
- 완료 상태에 대한 잘못된 저장 용량 경고 제거.
- 구 APK의 실패 상태가 남은 폰에서도 재확인 후 마법사를 계속 진행하며 앱 데이터 초기화 불필요.

전체 229개 시험 통과(실패/오류/스킵 0), Debug APK 빌드 성공, lint 오류 0/경고 84. 기존 서명 일치와 APK 6.9.1/code15를 확인했다. 현재·6.8.1·6.4 ZIP 모두 생산 importer 통과. 세부 결과와 APK·ZIP SHA256은 `verification-summary.json`에 기록했다. JVM 프로토콜 시뮬레이션과 Robolectric 검사는 실제 RF/전원 차단 시험이 아니다.

## Gate와 패키지

보관된 6.8.1 ZIP의 64KiB Gate SHA256은 `17ebfc7054f59519990bb692764ae3207404ca3fd2dec88391bd053fc94a833a`, 6.9.0과 최신 UI ZIP은 모두 `a0cbc28360e32710235e0f41f49982151babe5c7a410ca788d4b384d1f271592`다. 코드의 순정 복귀 안내는 실제 읽은 Gate와 선택 ZIP의 Gate가 다를 때 발생한다. 이번 사진만으로 해당 차량의 실제 Gate 해시는 알 수 없으며, 이번 작업에서 장치에 접근하지 않았다.

대응 ZIP은 `2026-09-23-dash-badge-grid/package-final/installer.zip`을 그대로 사용한다. SHA256: `372c5282ff28cc6acd814c019effe3e29c763ef5d2a0b490680a7134032a97c6`. MCU 펌웨어·메모리 배치·순정 BL·공장 데이터·NOR 내용은 이번 수정 대상이 아니다.

## 배포

APK 6.9.1 / versionCode 15. 기존 APK와 동일한 서명으로 데이터 유지 업데이트한다. OpenNoodoe 원본은 수정하지 않았다. APK와 동일한 펌웨어 ZIP을 함께 게시하고 GitHub Latest와 README 다운로드를 갱신한다. 실제 업로드 정보는 `github-release.json`과 `github-latest.json`에 남긴다.
