# NewDoe 0.15.2 beta — Weather and initial setup

## 한국어

- **NewDoe-0.15.2.apk**를 설치한 뒤 앱에 포함된 패키지를 선택하고 본체도 0.15.2로 업데이트하세요. A/B·언어별 Product, 기본 자원, 필요한 폰트 준비용 펌웨어가 APK에 함께 들어 있습니다. 보통 외부 ZIP을 선택할 필요가 없습니다.
- 새 설치는 검은 배경 중앙의 **WELCOME! / NEWDOE**와 깜빡이는 버튼 안내로 시작합니다. 아무 버튼을 누르면 인사 화면을 거쳐 **언어 → 일출·일몰 기준 지역 → 실제 연료 칸수 → 연료 인식 ON/OFF → 단위 → 시간대 → 속도 호 최대값**을 설정합니다. 저장을 마치면 재부팅합니다. 연결된 앱에는 현재 단계의 닫을 수 없는 설명이 표시됩니다. 설정에서 재부팅 후 다시 실행할 수 있으며, 무관한 설정은 유지합니다. 시스템 설정 초기화는 별도입니다.
- HOME 날씨는 왼쪽의 컬러 아이콘과 오른쪽의 현재·최고·최저 온도를 표시합니다. 온도에는 °만 붙이며 단위는 설정을 따릅니다. 아래 바는 0~+12시간을 한 번에 표시합니다. 맑음은 노란색, 구름은 회색, 강수확률 40% 이상은 확률에 따른 파란색이며 천둥은 진한 파란색입니다. O 길게 누르기는 기존 HOME 정보 순환입니다. 날씨는 기본 OFF이며 **개인 OpenWeather One Call API 키와 해당 서비스 이용 자격이 필요**합니다. 제공자 요금이 발생할 수 있습니다.
- 연료 측정 오류는 GPS 자리에 같은 크기의 붉은 사선 연료 아이콘으로 표시합니다. Fuel Measurement Error 깜빡임은 제거했고 정상 수신·연료 기능 OFF에서는 GPS로 돌아옵니다. 라이트 모드와 설정 상단 카테고리 호도 제거했습니다.
- 날씨는 폰에서 조회하고 폰·본체 RAM에만 보관합니다. 실제 본체 날씨 화면에서만 첫 조회 후 1시간 또는 30km 기준으로 갱신합니다. 다른 화면에서는 조회하지 않습니다.
- 기존 Gate·NOR 파일 배치·복구 경로는 변경하지 않았습니다. 정상 기기는 일반 Product 업데이트를 사용합니다. 0.14.1 설정 오류 등으로 차단된 기기는 기존 **데이터 보존 순정 복귀 → 재설치** 안내를 따르세요. 자동 초기화·자동 순정 복귀·직접 Gate OTA는 하지 않습니다.
- 기존 순정 복귀 구간은 전원 차단에 완전히 안전하지 않습니다. 해당 구간의 전원을 유지해야 하며 이 제한을 해결했다고 주장하지 않습니다.

## English

Install **NewDoe-0.15.2.apk**, select the included package, review the device/language and update the dashboard to 0.15.2 as well. The APK contains matching A/B language Products, resources and the font-preparation image where needed. Manual ZIP selection is normally unnecessary.

Fresh installations show centered **WELCOME! / NEWDOE** on black with a blinking button prompt. Any button starts a short animated greeting, then **language → sunrise/sunset region → actual fuel bars → fuel recognition ON/OFF → units → timezone → speed-arc maximum**. Durable completion restarts the dashboard. A connected phone shows a non-dismissible explanation of the current step. Setup can be repeated after a guarded restart from Settings without resetting unrelated preferences. System reset is separate; existing users are not reset automatically.

The HOME Weather view shows a colorful condition icon, current temperature and daily high/low using the degree symbol and configured units. A pill displays all 13 hourly forecasts together: yellow for clear, gray for cloudy, and graduated blue for rain probability at least 40%; thunderstorms are dark blue. Hold O cycles the existing HOME information strip. Weather defaults OFF and requires your own **OpenWeather One Call-enabled API key/account**; provider charges may apply. The phone fetches only while the actual dashboard Weather page is visible, initially and then after one hour or 30 km. Forecasts remain in RAM only.

A confirmed fuel measurement error uses a steady red crossed-fuel icon in the GPS position. The flashing error animation is removed; normal fuel or disabled fuel recognition restores GPS. Light mode and the settings category arc are removed. Weather artwork is adapted from Twemoji under CC BY 4.0; attribution and license are included in the app, illustrated manuals and accompanying license file.

Gate, NOR layouts and recovery paths are unchanged. Healthy devices use normal Product updates. Devices blocked by the old 0.14.1 settings issue follow the established keep-data stock-return/reinstall guidance. There is no automatic reset, stock return or direct Gate OTA. **The legacy stock-return interval still requires stable power; full power-interruption safety is not claimed.**

## Validation / 검증 범위

Software-tested beta. ARM tests, language build budgets, EVE software rendering, Android unit tests, artifact hashes and APK signature are checked before publication; see `verification.json`. **No API key was available for authenticated live weather tests. Physical Bluetooth/LCD, motorcycle behavior and power consumption were not measured.** Full-project lint still contains previously recorded translation/API compatibility errors outside the new weather files. Weather screenshots use synthetic data. No performance test or zero-to-100 feature is included.

일반 사용자는 APK를 받으세요. `NewDoe-CFW-0.15.2.zip`은 B 영어용 전문가 설치 파일이며 언어별 ZIP은 별도 모음에 있습니다. Normally use the APK; the default expert ZIP is B English, with other variants in the language archive. Keep the app's pending-install records when updating the APK.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)
