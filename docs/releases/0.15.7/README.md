# NewDoe 0.15.7 beta

앱·내장 펌웨어 **0.15.7 / Android 빌드 153**을 함께 제공합니다.

휴대폰 안내 지도와 본체 재탐색 상태 표시를 추가하고, 밀집 지도·경로 그리기의 작업량을 제한했습니다. 휴대폰은 전체 경로를 유지하고 본체에 보내는 경로만 단순화합니다. HOME 정보 스트립만 있는 모드의 밝은 배경을 복원했습니다. 화면 문구는 “내장 내비게이션 사용 중”입니다.

날씨 6시간 상세 예보를 **한 화면에 시간별 세 칸씩, 두 페이지**로 바꿨습니다. 아이콘을 두 배 크게 그리고 온도·강수량 숫자도 키웠습니다.

1. HOME 날씨에서 **O를 길게** 눌러 상세 예보를 엽니다.
2. 첫 화면은 +1~3시간, 다음 화면은 +4~6시간입니다. 하단 안내 문구와 페이지 번호는 표시하지 않습니다.
3. **위·아래 버튼**으로 페이지를 넘깁니다.
4. **O를 짧게** 눌러 날씨 개요로 돌아갑니다. 길게 눌러도 닫힙니다. 다시 열면 1페이지부터 시작합니다.

기존 +12시간 개요, API 조회 조건과 RAM 저장은 유지합니다. 페이지를 넘겨도 추가로 날씨를 조회하거나 NOR에 쓰지 않습니다. Basic API 2.5의 예보는 원본이 3시간 간격이므로 실제 원본 시각과 값이 반복될 수 있으며, 이를 시간별 새 예보로 꾸미지 않습니다.

APK를 기존 앱 위에 설치하고 **기기 → 현재 상태 확인 → 설치 / 업데이트**를 사용하세요. 앱에 일치하는 펌웨어와 자원이 들어 있습니다. Gate·복구 로직·NOR 파일 배치는 그대로이며 순정 복귀나 초기화를 일괄 요구하지 않습니다.

소프트웨어 검증과 LVGL/EVE 렌더링을 수행했습니다. 이번 버전의 실차·휴대폰·인증된 날씨 조회 검증은 하지 않았습니다. 기존 순정 복귀 전환 구간의 전원 차단 제한은 그대로입니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

App and included firmware **0.15.7, Android build 153**, are one matched set.

The phone now has a full guidance map sharing the route and current guidance with Noodoe. Recalculation is shown before routing completes. Dense map and head-unit path work is bounded; the full phone route remains intact. Strip-only HOME modes restore their bright photo. The label is “Built-in navigation active”.

Weather detail now displays **three larger hourly cards per page**, with two pages. Icons are twice as large; temperature and precipitation numbers are larger.

1. **Hold O** on HOME Weather to open details.
2. The first page shows +1–3 hours; the second shows +4–6 hours. No footer caption or page counter is drawn.
3. **UP/DOWN** switches pages.
4. **Press O** to return to the overview. Holding O also closes details. Reopening starts on page 1.

The existing +12-hour overview, fetch policy and RAM-only storage remain unchanged. Paging adds no API requests or NOR writes. Basic API 2.5 retains its original three-hour forecast timestamps and may repeat source times/values; it does not invent hourly precision.

Install the APK over the existing app, then use **Device → Check current status → Install / update**. Matching firmware/resources are included. Gate, recovery and NOR file layout are unchanged; a stock round trip or reset is not required for everyone.

Software tests and LVGL/EVE rendering were checked. Physical vehicle/phone tests and authenticated weather queries were not performed for this release. The previously documented power-interruption limitation during the legacy stock-return transition remains.

The default expert ZIP is B English. Normal users should let the app identify A/B and select the matching menu language. The language-variants archive is for expert manual import. Licenses and replacement-library objects are included separately.
