# NewDoe0.15.3 beta — Weather display

앱과 본체 펌웨어를 함께 업데이트하세요. APK에 A/B·언어별 Product, 기본 자원과 필요한 폰트 준비용 패키지가 들어 있습니다. 앱에서 실제 기기와 설치 언어를 확인한 뒤 업데이트하세요. 정상 기기는 기존 Product 업데이트를 사용하며, 자동 초기화나 순정 복귀는 하지 않습니다. 기존 Gate·NOR 파일 이름·크기·배치는 유지합니다.

날씨는 웃는 해와 Meteocons 아이콘, 큰 고정 폭 현재 온도, 최고·최저와 두 개의 화살표 바로 표시합니다. 범위는 현재부터+12시간입니다. 위는 날씨 상태, 아래는 실제 강수량(mm/h)이며 강수확률이 아닙니다. 날씨 화면에서는 일반 HOME 하단 정보 줄을 숨깁니다. 메뉴/TBT와 Weather 제목은 겹치지 않습니다.

날씨는 개인 OpenWeather One Call API 키가 필요하며 기본 OFF입니다. 본체 날씨 화면이 보일 때에만 처음 조회하고, 이후1시간 또는30km 이동 간격을 적용합니다. 다른 화면에서는 조회하지 않습니다. 예보는 폰과 본체 RAM에만 보관합니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated guide](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

Update the APK and firmware as one matching set. The APK includes all supported A/B/menu variants and shared resources. Existing Gate/recovery logic and NOR file layouts remain unchanged. Normal devices use Product update; reset and stock return are not automatic.

The redesigned Weather view retains current through+12h forecasts. The upper bar shows conditions; the lower shows actual precipitation amount(mm/h), not probability. Weather is optional and requires your own OpenWeather One Call key. Fetching remains restricted to the visible dashboard Weather page, one hour OR30km between attempts. Forecasts remain RAM-only.

Artwork: Meteocons Fill by Bas Milius, MIT; separate generated smiling sun. See FONT-LICENSES.txt and the app's Weather attribution.

Software-tested beta. Screenshots use synthetic weather through the production ARM/LVGL/EVE renderer. No subscribed key or vehicle was available: authenticated live forecasts, physical LCD/Bluetooth and power consumption are not hardware-qualified. The accepted legacy stock-return power-interruption limitation remains; keep power stable during that interval.
