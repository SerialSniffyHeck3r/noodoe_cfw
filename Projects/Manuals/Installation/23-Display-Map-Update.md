# FuckNudo 0.9.55

0.9.44에서 보고된 설치 후 재시작 정지, 지도 도로 단절, 큰 연료 경고의 배경 음영, 화면 변경 시 렌더링 부하를 수정했습니다.

- 설치 종료 때 Bluetooth가 마지막으로 갱신한 키까지 저장한 후 재시작합니다. 실제 저장 실패는 계속 오류로 처리합니다.
- 지도는 폭 기준으로 도로를 선택하고 연결선을 유지합니다. 연결점 개수 때문에 중간 조각을 버리던 선택 방식을 제거했습니다. 지도 화면 밖에서는 전송과 래스터 작업을 멈춥니다.
- 큰 Low Fuel / Fuel Level Critical 경고는 시동 전환 중에도 전체 배경에 음영을 적용하고, 아이콘용 알파 상태를 보존합니다.
- 입력 처리는5ms 주기를 유지하면서 무거운 화면 구성은30Hz 표시 시점에 맞춥니다. 매 프레임 EVE 명령 목록을 압축해 다시 쓰던 작업을 제거하고 완성된 명령 검사와 실제 화면 교체 보호는 유지했습니다.

**APK0.9.55를 먼저 덮어 설치하고, 대응하는0.9.55 ZIP으로 Product를 업데이트하세요.** 새 지도 형식 때문에 APK와 본체를 함께 업데이트해야 합니다. 기존 앱 데이터와 일반 업데이트의 사용자 설정 보존 정책은 그대로입니다. Gate 교체는 필요하지 않습니다.

처음0.9.55를 설치하는 과정의 재시작은 아직 기존 펌웨어가 담당합니다. 따라서 기존0.9.44의 정지 현상이 그 첫 업데이트에서 다시 나타날 수 있습니다. 이 경우 진단을 저장하고 기존 설치·복구 절차를 따르세요. 새 재시작 수정은0.9.55가 실행된 뒤의 업데이트부터 적용됩니다.

107 Android tests,108 ARM update runs and1203 LVGL/EVE frames passed. Actual vehicle installation, ignition reproduction and sustained LCD30fps were not measured. The fuel veil correction passed software rendering tests; its original intermittent physical cause remains unconfirmed. Details and memory limits are in VALIDATION.md.

English: retain width-selected road chains, stop hidden map work, schedule expensive composition at30Hz, validate EVE lists without rewriting them, isolate the large fuel veil, and persist late Bluetooth keys after controller shutdown before reset. Install the matching APK and ZIP. The initial upgrade still runs the old firmware's updater. Stock/resources/Gate/Bootstrap/uninstall/diagnostic images are unchanged.
