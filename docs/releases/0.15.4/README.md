# NewDoe 0.15.4 beta — Clock contrast and first setup

앱과 본체 펌웨어를 함께 업데이트하세요. APK에 지원하는 A/B·언어별 Product와 필요한 기본 자원·폰트 준비용 패키지가 들어 있습니다. 앱에서 실제 기기 상태와 설치 언어를 확인한 뒤 업데이트합니다. 정상 기기는 기존 Product 업데이트를 사용하며 초기화·순정 복귀를 자동 실행하지 않습니다. Gate·복구 로직과 NOR 파일 이름·크기·배치는 유지합니다.

- 아날로그·디지털 시계의 사진 배경에 최대160/255의 검은 음영을 적용합니다. 다른 HOME 보기에 돌아가면 시계 음영을 해제합니다. 바늘·시계 숫자·속도 호·ODO·정보 줄은 기존 색상을 유지합니다.
- 초기 설정이 예약되어 있으면 일반 키 ON 호 채움·축소 연출 없이 WELCOME! / NEWDOE로 시작합니다. 다음 정차 안내에서 O를 눌러야 진행합니다. 마법사에는 주행 잠금·자동 종료를 적용하지 않으며, 일반 설정의 기존 주행 잠금은 유지합니다.
- 키 OFF 상태의 디지털 속도는 `--- km/h` 또는 `--- mph`입니다.
- CFW UPDATE가 실제로 보이면 즉시 **보여요**를 누를 수 있습니다. **안 보여요**는10초 후 활성화됩니다. 최초 설치 경고5장은 각각3초 읽은 뒤 다음으로 넘깁니다. 기다림이 끝나도 자동으로 진행하지 않습니다.
- 처음 순정 펌웨어에서 설치기로 전송하는 동안은 키 ON을 유지하세요. **본체의 CFW 설치 프로세스가 시작된 뒤**에는 키 OFF가 가능합니다. 키 OFF와 배터리 전원 분리는 다릅니다. 배터리는 연결하고 양쪽 화면 지시를 따르세요.
- 날씨 온도 옆의 시간대별 위·아래 화살표를 제거했습니다. 현재부터+12시간 예보, 날씨 상태·실제 강수량(mm/h) 두 바는 유지합니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

설명서에 OpenWeather 가입·개인 One Call 키 등록·오류별 확인 방법과 BRouter `.rd5` 파일 준비·지도 지점 지정·실험적 Google Maps/OsmAnd TBT 사용법을 자세히 설명했습니다. 날씨는 기본 OFF이며 개인 API 키와 해당 서비스 사용 권한이 필요합니다. 본체 날씨 화면에서만 조회하고, 이후1시간 또는30km 이동 간격을 적용합니다. 예보는 RAM에만 보관합니다. BRouter는 사용자가 공식 배포처에서 경로 데이터를 준비하는 오프라인 방식입니다.

Update the APK and firmware as a matched set. Existing Gate/recovery logic and NOR file layouts remain unchanged. Normal devices use Product update; reset and stock return are not automatic.

Both analog and digital clocks darken the photo behind them, without darkening their foreground artwork. The wizard opens before the ordinary ignition arc animation. Its stop instruction requires O to continue; the wizard has no movement lock or automatic timeout. Ordinary settings keep their existing movement lock. Key-OFF digital speed shows dashes. CFW UPDATE confirmation is immediately available when the screen is visible; the negative response unlocks after10seconds. Each of the five initial-install warning cards requires3seconds of reading and an explicit Next press.

Keep key ON during the initial stock-to-installer transfer. Key OFF is permitted after the dashboard's CFW installation process has started, with the battery connected and both screens' instructions followed. This does not remove the existing legacy stock-return power-interruption limitation.

Weather's time-of-day temperature arrows are removed. Current through+12h, conditions and actual precipitation bars remain. The updated illustrated guides include OpenWeather account/key setup, BRouter segment import, destination selection and experimental external TBT. Weather is optional, requires your own One Call access and is fetched only while the dashboard Weather page is visible, with the existing one-hour OR30km interval. Forecasts remain RAM-only.

Artwork: Meteocons Fill by Bas Milius, MIT; separate generated smiling sun. See FONT-LICENSES.txt and the app's Weather attribution.

Software-tested beta. EVE previews use production ARM/LVGL rendering and synthetic inputs. No subscribed API key or vehicle was available: authenticated weather retrieval, physical LCD/Bluetooth and power consumption remain unverified. Keep power stable during the accepted legacy stock-return interval; it has not been hardware-qualified here.
