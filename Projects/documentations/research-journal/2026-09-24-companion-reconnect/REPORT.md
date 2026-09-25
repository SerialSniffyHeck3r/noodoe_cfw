# 6.10.4 재연결 / 롤백 확인 수정

사용자 증상: 설치 확정 후 Disconnected / reconnect 1/8 반복, 앱 재설치로 해결되지 않음. 앞선 롤백 팝업 O 확인 불가도 함께 처리.

## 재현된 원인

`MaintenanceService.runCompanion`은 연결 직후 `CompanionRuntime.gpsDiagnostics()`를 `SessionLog.append("phone_location_status", ...)`에 전달한다. 실제 map의 callbacks, registrations, fix_age_ms, callback_age_ms, screen, location_power_mode, background_permission, media_events, track_changes, art_requests가 SessionLog의 허용 목록에 없었다. 실제 Runtime과 실제 SessionLog를 결합한 시험에서 구 구현은 `Private/unrecognized log field`로 실패했다(`old-log-reproduction.xml`). 포괄 catch가 이를 무선 단절로 오인해 정상 socket을 닫았다.

사용자 실차 로그를 가져온 것은 아니므로 현장에서 발생한 모든 단절의 단일 원인이라고 주장하지 않는다. 표시상 1/8은 주행 연결 경로이며, 설치 BootHandoff의 시험부팅 재접속 경로와 다르다.

## 변경

- 개인정보가 아닌 위 aggregate 진단 필드만 명시적으로 허용. 좌표·미디어 문자열·전화번호·인증키 차단 유지.
- 주기 로그 실패는 표시하되 정상 주행 SPP를 끊지 않음. 변경 요청 전 필수 영구 기록 실패를 무시하는 것은 아님.
- 로컬 예외·명시적 DeviceRejected·취소는 무선 재시도로 처리하지 않음. 단계/opcode/sequence/result/경과시간을 payload 없이 기록.
- 8회 재연결 예산은 정상 owner iteration이 연속 5분 지속되어야 초기화. 중간에 긴 정체가 있으면 정상 시간으로 산입하지 않음. 단일 외부 재시도 owner를 사용.
- 초기 PhoneContent token 준비 경합으로 notification clear가 CFW_BUSY일 때 같은 socket에서 5초 한도로 대기. 명시적 BUSY만 재시도하고 응답 불명은 재전송하지 않음.
- 업데이트 완료 → 자동 주행 전환에서 stopSelf를 먼저 실행하지 않음. main thread와 연결 세대로 종료를 한정해 이전 작업이 새 owner를 중단하지 않게 함.
- 전체 회귀시험에서 기존 health-confirm 요청도 비동기 BootStore 초기화보다 이르면 유실되는 것을 발견. 요청은 latch하고, journal scan 이후 실제 diagnostic/retained boot context를 검사하도록 이동. Diagnostic 자동확정은 계속 금지.
- BootStore의 O 확인 접수와 영구 해제를 분리. fallback health 확인 전에도 일치하는 transaction의 의도를 RAM에 보관하되, confirmed 이후에만 저널 기록. 실패 이력은 유지. ARM 시험은 조기/정상 확인·잘못된 transaction·중복 확인·새 업데이트 허용을 검사.

## 범위

OpenNoodoe 원본, Cube 생성 파일, BSP/vendor, 순정 BL/공장 데이터 변경 없음. 실제 ST-LINK·벤치·실차·RF 연결 없음. 메모리 기준 및 30초 건강/화면확인/롤백 조건을 낮추지 않음. APK만으로 재연결 수정 사용 가능; 로컬 롤백 확인 개선은 새 Product 필요.

## 최종 검증

- Android 표적 시험 116개 통과, lint 오류 0.
- 실제 ARM 코드 모의시험: Product 업데이트/BootStore 98개, trial-confirmation 14개 통과. 원본 로그 결함 재현 자료 보존.
- Release/Debug 빌드와 메모리 기준 통과: APP flash 여유 65,580 / 33,728 B. 일반 SRAM 여유 52,512 / 53,480 B, CCM 16,320 B.
- 기존 APK 서명 유지. 실제 Android ZIP importer로 새 ZIP과 이전 ZIP 검사, 패키지 Product와 최종 ELF 일치, Bootstrap/Gate/stock/resources/uninstall/diagnostic 동일성 확인.
- 실차·정상 RF 기판에서의 재연결 검증은 미실시.
