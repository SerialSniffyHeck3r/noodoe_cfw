# 6.9.9 변경과 검증

## 화면 OFF GPS

기존 버전에도 `location` FGS 선언·승격과 주행 중 partial wake lock은 있었다. 화면 OFF에서 위치가 멎은 사용자의 사건을 단순히 ‘FGS가 없어서’라고 결론 내리지 않았다. 실제 handset 로그가 없어 Samsung 절전 정책·권한·프로세스 회수 중 무엇이 직접 원인이었는지는 아직 확정하지 못했다.

수정 전에는 legacy GPS_PROVIDER만 메인 looper에 등록했고, 실제 수신이 멎어도 `locating=true`만 보고 계속 대기했다. 위치 등록 실패는 표시/로그 없이 처리했고, 서비스는 주행 연결 중에도 START_NOT_STICKY여서 프로세스 회수 후 재시작 경로가 없었다.

`PhoneLocationFeed`가 위치 구독을 소유한다. 전용 HandlerThread, IGN·연결 수명, 실제 location FGS 여부, FINE 권한, 활성 provider를 검사한다. API31 이상은 플랫폼 fused와 GPS에 HIGH_ACCURACY / 1000ms / max delay0을 요청하고 이전 Android는 GNSS 경로를 사용한다. 새 데이터가 멎으면 30초보다 자주 재등록하지 않는다. 5초가 넘은 위치를 새 위치로 위장하지 않는다. 세션 변경 시 캐시를 지우며 IGN OFF·권한 철회·연결 종료 때 구독을 해제한다. 최근 정확한 GNSS 위치는 coarse fused 표본으로 덮지 않는다. 두 공급원의 빠른 교대도 제한한다.

Android가 백그라운드 location FGS 시작을 거부하면 connectedDevice SPP 서비스는 유지하고 GPS가 동작한다고 표시하지 않는다. Activity 재진입/권한 수정 시 FGS 승격을 다시 시도한다. OS의 sticky 서비스 재시작은 저장된 Product 주행 연결만 대상으로 하며 선택 기기·AUTO/MANUAL/UPDATE·미결 설치 상태를 검사한다. 설치 명령은 재생하지 않는다. 강제 중지·권한 철회·OEM 강제 종료를 코드가 무조건 우회한다고 주장하지 않는다.

30초마다 수신 횟수·등록 횟수·수신 및 fix age·화면 ON/OFF·위치 절전 모드·백그라운드 권한 상태·미디어 변경 횟수를 기록한다. 좌표·곡명·아티스트·페어링 키는 기록하지 않는다.

Android의 화면 OFF 및 백그라운드 시작 제한은 구분해야 한다. 이미 시작된 적절한 위치 FGS는 화면 OFF에서도 사용할 수 있지만, 백그라운드에서 새로 시작할 때에는 추가 제한이 있다. [Android FGS 시작 제한](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start), [위치 절전 모드](https://developer.android.com/reference/android/os/PowerManager#getLocationPowerSaveMode()). 앱의 권한 화면에 항상 위치 허용·배터리 제한 확인·절전 설정 경로를 추가했다.

## 미디어 지연과 깜빡임

`MediaUpdates`는 최대8개 활성 세션의 콜백을 구독하고 서비스의 SPP 작업 스레드를 깨운다. 콜백 스레드에서 직접 소켓을 쓰지 않는다. JPEG 생성/URI 로드 완료도 동일한 깨움 경로를 사용한다. 상태·시간의 주기 갱신은 유지하지만 이벤트 도착 뒤 다음200ms 폴링을 반드시 기다리는 구조를 없앴다. 첫 타일은 최대1초 동안50ms 간격으로 상태를 확인하고 완료되면 기존 간격으로 복귀한다. [Android MediaController.Callback](https://developer.android.com/reference/android/media/session/MediaController.Callback).

6.9.8은 텍스트 식별값에 optional MEDIA_ID를 넣었다. 제목·아티스트가 동일해도 이 필드가 null/값으로 교대하면 전송을 취소하고 새 visual key를 만들어 화면 전환을 반복한다. 새 버전은 표시 문자열로 텍스트를 식별하고 아트는 별도 픽셀 서명으로 식별한다. 재생 앱 목록 순서가 바뀌어도 선택한 재생 중 세션을 유지한다. 1.5초 미만의 일시적인 null 메타데이터/세션 공백은 즉시 화면을 지우지 않는다.

AVRCP급의 짧은 메타데이터 반응을 목표로 할 수 있지만, SPP의 JPEG 전송 바이트·요청/응답 시간과 음악 앱 자체의 아트 제공 지연은 남는다. 이번에는 전송창 확대·프로토콜 ACK 생략·저해상도 이미지 대체를 도입하지 않았다. 체감 속도의 실측 결과를 대신할 수 있는 고정 지연 수치를 제시하지 않는다.

## 펌웨어 UI

평균속도 표시는22×12px, 회청색0xA5B3BA, 기존 알파의170/255로 줄였다. 최고속도 막대·계산·확정된 IGN 세션 초기화는 그대로다. 드라이버·Cube 생성 파일은 변경하지 않았다.

## 검증과 범위

- 새 Android 회귀: 화면 OFF20초 동안 주입 위치 수신, 고정밀1초/배치0 계약, IGN OFF 해제, 정체 후 제한된 재등록, FGS 미허용·권한 철회, 새 세션 초기화, coarse 위치 거부.
- MediaSession 콜백이 폴링 전에 소유자를 깨우는지, 여러 플레이어를100번 재정렬해도 선택 세션이 유지되는지, 콜백 해제가 되는지 검사했다.
- 실제 CompanionRuntime+모의 NDCP에서40회 optional MEDIA_ID 교대/null 메타데이터를 주입했다. 같은 곡은 visual key·BEGIN·FINISH가 각각 한 번이고, 실제 제목 변경 때 새 key가 생긴다.
- 새 프로세스의 Product 주행 재개, 다른 기기·수동 중지·UPDATE·초기화 때 재개 금지를 검사했다.
- Android 전체282개 시험과 lint(Error/Fatal0)를 통과했다. 기존 APK 서명·6.9.9/code22·현재/구 ZIP importer 검증 결과는 verification-summary.json에 기록했다.
- 실제 LVGL/EVE 렌더러 전체 프레임 모의시험2,016회, 사진 혼합1,728프레임. 평균 마커 변경 후 display list 최대7,808 /8,192B. 물리 LCD 캡처나 FPS 측정이 아니다.
- Release flash 여유66,852B / SRAM53,216B. Debug flash35,100B / SRAM54,136B. CCM16,320B 여유. 힙·스택·큐 예산 유지.

사용자 S24 Ultra나 정상 무선 누도에 접근하지 않았다. 위 결과는 Android/RF/기기 전원을 모의한 검사와 로컬 빌드다. 실물에서의 화면 OFF 위치 연속성·미디어 지연·깜빡임 종료는 실제 장치에서 확인해야 하며, 새 진단 로그는 그 원인을 분리할 수 있게 한다.
