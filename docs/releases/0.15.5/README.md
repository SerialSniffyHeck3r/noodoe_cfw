# NewDoe 0.15.5 — display, setup, TBT and weather fixes

Install **NewDoe-0.15.5.apk** over your existing app. Use **Device → Check current status → Continue installation / check language**, review the selected language and proposed operation, then install the matching included firmware. Previous unfinished operations must be checked first. Gate, NOR file layout and stored user data are preserved by the existing update path.

실제 순정 자원의 `files` / `allFiles` 식별을 앱·본체 양쪽에서 수정했습니다. 구형 CFW 확인 과정에서 사진 하위 폴더를 모두 읽던 경로를 줄였습니다. 알 수 없는 ID나 손상·모순을 B판으로 추측하지 않으며 쓰기 전 검사는 유지합니다. 해당 유저의 실제 자원 헤더는 기존 진단에 없으므로 그 차량의 원인을 모두 확정한 것은 아닙니다.

내비·날씨 사용 후 숫자가 빠지거나 깨지던 EVE 캐시를 수정했습니다. HOME 음영은 속도 화면 기준으로 통일하고 날씨의 사각 카드를 제거했습니다. 초기 설정은 모든 단계에서 검은 배경이며 시간대는 `UTC+09:00`처럼 표시합니다. HOME AUTO는 내용만 순환합니다. TBT 노란 강조는 실제 500m 이하에서 켜지고, 자체 안내의 선택적 올림은 10m 단위입니다. 기존 선택값은 유지합니다. 마지막 확인 ODO와 정비 요약의 ODO 보존 경로도 검증했습니다.

**날씨 → 날씨 API → Basic API 2.5 → 키 확인 · 날씨 조회**로 일반 API 키를 사용할 수 있습니다. One Call도 유지합니다. Basic은 현재 날씨와 3시간 예보이며 오늘 최고·최저는 제공되지 않아 `—` / `--`로 표시합니다. 예보 바는 3시간 구간 근사, 강수량은 구간 평균입니다. 조회당 요청 2회입니다. 키가 맞아도 해당 서비스 권한·활성화 상태 때문에 401이 날 수 있으며 실패한 API와 HTTP 코드를 표시합니다. 자동으로 유료 가입하거나 API를 바꾸지 않습니다.

Both observed stock metadata list formats are supported. Legacy identity reads no longer walk photo subdirectories. Unknown or conflicting identity still stops writes. Full pre-write ownership and recovery checks remain. The affected rider’s actual header was not present in their earlier diagnostics; field confirmation is still needed.

Fixed EVE cache corruption/missing digits after navigation and weather page changes. HOME shading now matches the speed view, without a rectangular weather card. Every setup step has an opaque black background; time zones display as `UTC+09:00`. HOME AUTO changes content without page animation. Yellow TBT emphasis uses actual distance within 500 m; optional native rounding now uses 10 m steps, preserving the existing choice. Last confirmed ODO and maintenance-summary retention were checked.

Choose **Weather → Weather API → Basic API 2.5**, then **Check key / get weather** for current weather and 3-hour forecasts without One Call. Basic has no daily high/low; missing values remain unavailable. Forecast bands approximate 3-hour intervals and precipitation is their average mm/h. Each refresh uses two requests. One Call hourly forecasts remain available. No silent provider switch or paid enrollment occurs.

Automatic fetching remains limited to the dashboard Weather page, after one hour or 30 km. Weather remains in RAM. Manual phone checks work without a dashboard and are limited to one refresh per minute. API selection and stored key survive app restarts; forecast data does not.

Software-tested beta. Physical Bluetooth and authenticated OpenWeather queries have not been verified in this session. The existing stock-return transition power-interruption limitation is not fixed by this release.

한국어·영어 그림 설명서는 함께 제공되는 매뉴얼 ZIP의 index.html을 여세요. 일반 사용자는 APK의 내장 패키지를 사용하세요. 별도 ZIP은 전문가용입니다.
