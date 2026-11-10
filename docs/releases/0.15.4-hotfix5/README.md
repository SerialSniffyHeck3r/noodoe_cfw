# NewDoe 0.15.4 Hotfix 5 · Android build 149

## 0.15.4 Hotfix 5 — 내비게이션 연결·목적지 선택·날씨 확인 (빌드 149)

본체가 이미 0.15.4이면 **APK만 기존 앱 위에 설치**하세요. 펌웨어·Gate·NOR 파일은 바뀌지 않으며 이전 핫픽스도 포함됩니다.

- **내비게이션 → 지도에서 목적지 선택**을 누르면 바로 큰 지도가 열립니다. 지도를 끌거나 확대해 중앙 표시를 목적지에 맞춘 뒤 **이 위치 선택 → 오프라인 안내 시작**을 누르세요. 현재 위치로 돌아가기와 지도 파일 관리 버튼도 있습니다. 마지막으로 보던 지도 영역을 기억하며, 새 GPS 위치가 있으면 처음 열 때 현재 위치를 보여줍니다. 지도 선택 결과는 원래 내비 화면으로 돌아옵니다.
- 차량 연결 판정은 기기 탭에서 선택한 누도와 실제 주행 연결을 사용합니다. 구형 화면의 선택값 때문에 연결된 차량을 거절하던 오류를 수정했습니다. 페어링만 된 상태에서는 상단 ▶로 주행 연결부터 시작하세요. 다른 차량·지원하지 않는 펌웨어·끊긴 연결은 안내 시작이 계속 제한됩니다.
- **기기 → 날씨**에서 API 키의 미등록·저장됨·승인됨·인증 거절·확인 실패 상태와 마지막 확인 시각, 실제 HTTP 응답 코드를 볼 수 있습니다. 키 자체는 표시하거나 진단에 기록하지 않습니다.
- **키 확인 · 날씨 조회**는 현재 휴대폰 위치로 직접 1회 조회합니다. 차량 없이도 가능하며, 위치 권한과 위치 서비스가 필요합니다. 현재 날씨·기온·오늘 최고/최저와 +0~+12시간 예보를 텍스트로 표시합니다. 저장된 키만으로 인증 성공이라 표시하지 않습니다. HTTP 401/403이면 키 또는 One Call 이용 권한을, 429이면 제공업체 할당량을 확인하세요. 통신 실패는 키가 틀렸다고 단정하지 않습니다.
- 자동 날씨 조회는 기존처럼 **본체 날씨 화면이 열려 있을 때 1시간 또는 30km 간격**입니다. 버튼으로 하는 수동 확인은 별도이며 반복 조회는 60초 간격으로 제한합니다. 실제 요청 전 취소가 1시간 대기를 소모하던 문제와 키 변경 후 이전 실패 상태가 남던 문제를 수정했습니다. 예보는 RAM에만 보관합니다. 앱을 다시 시작하면 새 조회가 필요합니다.

이번 검증은 자동 시험과 Android 화면 렌더링입니다. 실제 휴대폰·차량의 Bluetooth 및 스크롤 체감, 사용자 키로 OpenWeather 실조회는 별도로 확인해야 합니다.

## 0.15.4 Hotfix 5 — navigation connection, destination picker and weather checks (build 149)

If your dashboard already runs 0.15.4, **install only the APK over the existing app**. Firmware, Gate and NOR files are unchanged; earlier hotfixes are included.

- **Navigate → Select a point on the map** opens a large map immediately. Pan or zoom until the center marker is on the destination, then choose **Use this point → Start offline guidance**. My location and Map files are directly available. The picker remembers the previous area; a fresh riding GPS fix centers the map on your location on entry. Selection returns to the original navigation screen.
- Guidance now uses the same selected vehicle as the riding connection. It no longer rejects that connection because of a retired screen’s saved selection. Pairing alone is not an active riding connection: use ▶ first. Other vehicles, disconnected sessions and unsupported firmware remain blocked.
- **Device → Weather** shows whether the API key is missing, saved, accepted, rejected or unverified, with the last check time and actual HTTP code. The key itself is never displayed or included in diagnostics.
- **Check key / get weather** makes one explicit request at the current phone location, even without a dashboard. Location permission and location services are required. Current conditions, temperature, today’s high/low and +0 to +12 hour forecasts are shown as text. A saved key is not claimed to be accepted. For HTTP 401/403 check the key and One Call access; for 429 check the provider quota. Network failure does not prove the key is wrong.
- Automatic queries still require the **dashboard Weather page**, after one hour or 30 km. Explicit manual checks are separate and limited to once per 60 seconds. Cancelling before an HTTP request no longer consumes the one-hour interval; replacing the key resets the previous failed-key state. Forecasts remain RAM-only and need to be fetched again after app process restart.

Verification uses automated tests and Android software rendering. Physical phone/vehicle Bluetooth, perceived scrolling smoothness and a live OpenWeather request with the user’s key remain to be checked.

