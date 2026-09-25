# CFW→CFW 부팅 확인 재연결 / Companion 6.10.2

## 확인된 코드 결함과 변경

1. 부팅 신원·상태 응답에도 NdcpClient 기본 60초가 적용되어, 유실된 응답 하나가 기기 180초 시험부팅 창을 크게 소모했다. BootHandoff의 전용 NdcpSession만 5초로 변경했다. 실제 NOR 소거·해시·파일 전송의 기존 제한시간은 유지한다.
2. 연결/조회 실패 5회만으로 자동 확인이 종료됐다. Gate가 설치 중인 동안 빠르게 실패하면 BT 준비 이후의 재시도를 놓쳤다. 기존 유한 부팅 창 안에서 2–5초 간격 재시도하며 각 시도는 secure RFCOMM 소켓 하나만 사용한다. Android 페어링 거절·취소, UID 불일치, 롤백은 중단한다.
3. 이전 연결에서 전달한 확인이 NOR에 확정되는 시점과 재접속 조회가 겹치면 정상 확정을 변경된 trial로 오인할 수 있었다. 같은 후보 SHA의 non-trial 상태는 확정/설정초기화 완료를 조회하며 예전 세대 ACK를 보내지 않는다. 남은 기한 0도 먼저 확정 상태를 재조회한다.
4. 실제 응답 실패 opcode·sequence를 오류에 포함하고 마지막 재연결 실패를 폰 저널에 보존한다. 페이로드·페어링 키는 기록하지 않는다.
5. 기존 언어 메뉴를 홈의 항상 보이는 버튼으로 이동했다. 이미 번역된 Context에서 시스템 언어로 복귀할 때 예전 앱 언어를 다시 사용하는 문제를 수정했다. 화면 재생성은 기존 Service를 중단하지 않는다.

## 유지된 계약

기기 30초 건강 검사 / Bluetooth READY / 사용자 새 CFW 화면 확인 / exact UID·Gate·후보 SHA / 실제 영구 확정 조회 / 기기 180초 롤백 / 이전 CFW 및 버튼 순정복원 유지. COMMIT·RESET·파일전송 재실행 없음. APK만 변경; Product6.10.1과 Gate/Bootstrap/NOR 자산 등 ZIP 전체는 이전과 바이트 동일하다.

## 검증

77개 표적 Android 시험 통과, APK 빌드 및 lint 오류0, 기존 APK 서명 일치, 실제 ZIP production importer 통과. 여섯 번 초기 접속 실패 후 성공, 무응답 기한 종료, 확인 ACK 유실 후 조회만 완료, 연결 세대 변경 후 재확인, 두 status 사이 확정, 다른 UID 거부, 언어6종 및 시스템 복귀/홈 메뉴 가시성 검사를 포함한다. 스크린샷은 Robolectric UI 렌더이며 실폰 캡처가 아니다.

실차 로그가 제공되지 않아 신고된 RF 장애의 단일 원인은 확정하지 않았다. 벤치/ST-LINK/실차/ADB/GUI 접근 없음. 이 수정의 실차 무선 성공을 주장하지 않는다. 실제 배포 APK·ZIP 해시와 Latest는 verification-summary.json / github-latest.json에 기록한다.

## 적용

기존 앱 위에 APK6.10.2를 설치한다. 앱 데이터·페어링 삭제, 순정 복귀, Bootstrap 재설치가 필요하지 않다. 기기가 이미 롤백했다면 그 결과를 조회/확인 후 CFW 업데이트를 다시 진행한다. 기기가 아직 후보를 실행 중이면 같은 후보 ZIP의 기존 부팅확인/재연결 경로로 이어간다.

## 배포

GitHub tag `companion-v6.10.2-trial-reconnect`에 APK와 CFW6.10.1 동일 ZIP을 게시하고 asset SHA·Latest·README readback을 확인했다. 이 시점 원격 저장소는 이미 public이었다. 공개 범위를 바꾸지 않았으며 바이너리·릴리스 설명·검증결과만 게시했다. 공용 Latest 도구는 기존 private-only 가정 대신 업로드 기록의 관측 visibility와 현재값의 일치를 검사하도록 수정했다(옛 기록 기본값 private 유지).
