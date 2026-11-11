# NewDoe 0.15.6 beta

앱과 내장 펌웨어 **0.15.6 / Android 빌드 152**를 한 묶음으로 제공합니다.

- **계기판 호 색상**: 본체 설정 → 표시에서 기본값 또는 일출 / 일몰을 선택합니다. 기본값은 기존 색입니다. 자동 모드는 설정한 기준 지역과 현지 시각에 따라 빈 속도 호의 색조만 바꿉니다. 시동 OFF는 회색을 유지합니다.
- **날씨 6시간 상세 보기**: HOME 날씨에서 O를 길게 누릅니다. 예보 시각·아이콘·온도·강수량을 여섯 칸으로 표시합니다. O를 짧게 누르면 원래 화면으로 돌아갑니다. 기존 날씨 개요는 +12시간을 유지하며 조회 조건은 바뀌지 않습니다.
- **숫자 표시 통일**: HOME의 일출·일몰과 목적지 거리 숫자를 배터리·알림과 같은 숫자 서체 및 크기로 맞췄습니다. 지도 하단 표시의 크기는 유지합니다.
- 네비게이션의 RAM 접근·그리기·검증 중복을 줄였습니다. 기존 화면과 지도 결과를 소프트웨어로 비교했습니다.

앱 설치 후 **기기 → 현재 기기 상태 확인 → 설치 / 업데이트**를 사용하세요. 주행 연동 중에도 경고 화면을 읽고 업데이트를 확인할 수 있으며, 확인 후 앱이 연결을 넘겨받습니다. 앱과 펌웨어를 모두 업데이트해야 새 날씨 상세 자료를 사용할 수 있습니다. 설치가 진행 중인 기록은 먼저 상태를 확인하며, 자동으로 설치하거나 초기화하지 않습니다.

**Basic API 2.5**는 3시간 간격 원본 예보를 사용합니다. 상세 화면에 간격을 명시하며 같은 시각·값이 반복될 수 있습니다. One Call은 시간별 원본 예보를 사용합니다. 제공되지 않는 값은 `--`입니다. API 키는 앱에서 설정하며 날씨 자동 조회는 본체 날씨 화면에서 기존 1시간 또는 30km 조건을 따릅니다. 상세 보기를 열었다고 조회가 추가되지 않습니다.

Gate와 복구 바이너리, NOR 파일 이름·크기·배치는 변경하지 않았습니다. 전체 사용자에게 순정 복귀나 초기화를 요구하지 않습니다. 기존 순정 복귀 전환 구간의 전원 차단 제한은 그대로입니다. 소프트웨어 검증 베타이며, 이번 버전의 실제 차량·휴대폰·인증된 날씨 조회 검증은 하지 않았습니다.

[한국어 그림 설명서](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.md) · [English illustrated manual](https://github.com/SerialSniffyHeck3r/noodoe_cfw/blob/main/Projects/Manuals/Guide.en.md)

## English

App and included firmware **0.15.6, Android build 152**, form one matched set.

- Select **Display → Arc color theme → Default / Sunrise / sunset**. Default preserves the previous appearance. The optional mode changes only the unfilled speed arc's hue using the configured reference city and local time. IGN OFF remains gray.
- Hold O on HOME Weather for six forecast cards showing source time, icon, temperature and precipitation rate. Short O returns to the overview. The existing +12-hour overview and fetch policy remain unchanged.
- HOME sunrise/sunset and destination distance now use the same numeric typeface and size as battery/unread values. Compact map labels retain their size.
- Navigation memory access, drawing and validation were optimized with software comparisons of the existing screen and map output.

Install the APK, then use **Device → Check current status → Install / update**. The warning flow is available while riding integration is connected; the app takes over the connection after confirmation. Update both the app and firmware to use the expanded weather payload. Existing unfinished operations are checked first. Nothing is installed or reset automatically.

**Basic API 2.5** retains the original 3-hour forecast times and labels that interval; repeated source times/values are possible. One Call uses hourly data. Unavailable values show `--`. Configure the key in the app. Automatic weather refresh still requires the dashboard Weather view and the existing one-hour or 30-km condition. Opening detail does not add a fetch.

Gate/recovery binaries and NOR file names, sizes and placement are unchanged. A stock round trip or reset is not required for everyone. The previously documented power-interruption limitation during the legacy stock-return transition remains. This beta was software-tested; this release has not been verified on a physical vehicle/phone or with an authenticated weather account.

The default expert ZIP is B English; normal users should let the app identify A/B and select the matching menu language. The language-variants archive is for expert manual import. See the included licenses and replacement-library object kit.
